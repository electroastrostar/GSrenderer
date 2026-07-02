#include "loader/ply_loader.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gsr::loader {

namespace {

[[noreturn]] void fail(const std::filesystem::path& path, const std::string& reason) {
  throw std::runtime_error("load_ply(" + path.string() + "): " + reason);
}

// Byte width of a PLY scalar type name; nullopt for unknown/list types.
std::optional<std::size_t> scalar_size(const std::string& type) {
  if (type == "char" || type == "uchar" || type == "int8" || type == "uint8") return 1;
  if (type == "short" || type == "ushort" || type == "int16" || type == "uint16") return 2;
  if (type == "int" || type == "uint" || type == "int32" || type == "uint32" ||
      type == "float" || type == "float32")
    return 4;
  if (type == "double" || type == "float64" || type == "int64" || type == "uint64") return 8;
  return std::nullopt;
}

bool is_float32(const std::string& type) { return type == "float" || type == "float32"; }

struct Header {
  std::size_t vertex_count = 0;
  std::size_t stride = 0;  // bytes per vertex record
  // Byte offsets of required properties within a record.
  std::size_t x = 0, y = 0, z = 0;
  std::size_t scale[3] = {0, 0, 0};
  std::size_t rot[4] = {0, 0, 0, 0};
  std::size_t opacity = 0;
  std::size_t dc[3] = {0, 0, 0};
  std::vector<std::size_t> rest;  // offsets of f_rest_0..R-1, index == coefficient index
};

// Strip a trailing '\r' (files written on Windows).
void chomp(std::string& line) {
  if (!line.empty() && line.back() == '\r') line.pop_back();
}

Header parse_header(std::ifstream& file, const std::filesystem::path& path) {
  std::string line;
  if (!std::getline(file, line)) fail(path, "empty file");
  chomp(line);
  if (line != "ply") fail(path, "not a PLY file (missing 'ply' magic)");

  Header header;
  bool format_seen = false;
  bool in_vertex_element = false;
  bool vertex_seen = false;

  struct Pending {
    std::string name;
    std::size_t offset;
    bool f32;
  };
  std::vector<Pending> props;

  while (std::getline(file, line)) {
    chomp(line);
    std::istringstream tokens(line);
    std::string keyword;
    tokens >> keyword;

    if (keyword == "comment" || keyword == "obj_info" || keyword.empty()) continue;
    if (keyword == "end_header") {
      if (!format_seen) fail(path, "missing 'format' line");
      if (!vertex_seen) fail(path, "no 'element vertex' in header");

      // Resolve required properties from the collected list.
      auto offset_of = [&](const std::string& name) -> std::size_t {
        for (const auto& p : props) {
          if (p.name == name) {
            if (!p.f32) fail(path, "property '" + name + "' must be float32");
            return p.offset;
          }
        }
        fail(path, "missing required property '" + name + "'");
      };
      header.x = offset_of("x");
      header.y = offset_of("y");
      header.z = offset_of("z");
      header.opacity = offset_of("opacity");
      for (int i = 0; i < 3; ++i) header.scale[i] = offset_of("scale_" + std::to_string(i));
      for (int i = 0; i < 4; ++i) header.rot[i] = offset_of("rot_" + std::to_string(i));
      for (int i = 0; i < 3; ++i) header.dc[i] = offset_of("f_dc_" + std::to_string(i));

      // f_rest_* must form a contiguous index range 0..R-1.
      std::size_t rest_count = 0;
      for (const auto& p : props) {
        if (p.name.rfind("f_rest_", 0) == 0) ++rest_count;
      }
      header.rest.resize(rest_count);
      for (std::size_t i = 0; i < rest_count; ++i) {
        header.rest[i] = offset_of("f_rest_" + std::to_string(i));
      }
      return header;
    }

    if (keyword == "format") {
      std::string kind, version;
      tokens >> kind >> version;
      if (kind != "binary_little_endian") {
        fail(path, "unsupported format '" + kind + "' (only binary_little_endian)");
      }
      format_seen = true;
    } else if (keyword == "element") {
      std::string name;
      std::size_t count = 0;
      tokens >> name >> count;
      if (name == "vertex") {
        if (vertex_seen) fail(path, "duplicate 'element vertex'");
        vertex_seen = true;
        in_vertex_element = true;
        header.vertex_count = count;
      } else {
        // Vertex data must come first in the file; an element declared before it would
        // shift the data section in a way we don't parse. Elements after vertex are
        // simply never read.
        if (!vertex_seen && count > 0) {
          fail(path, "unsupported element '" + name + "' before vertex data");
        }
        in_vertex_element = false;
      }
    } else if (keyword == "property") {
      if (!in_vertex_element) continue;  // properties of trailing elements: ignored
      std::string type, name;
      tokens >> type >> name;
      if (type == "list") fail(path, "list properties are not supported on vertices");
      const auto size = scalar_size(type);
      if (!size) fail(path, "unknown property type '" + type + "'");
      props.push_back({name, header.stride, is_float32(type)});
      header.stride += *size;
    } else {
      fail(path, "unrecognized header keyword '" + keyword + "'");
    }
  }
  fail(path, "unterminated header (no 'end_header')");
}

inline float read_f32(const char* base, std::size_t offset) {
  float v;
  std::memcpy(&v, base + offset, sizeof v);
  return v;
}

inline float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

}  // namespace

SplatData load_ply(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) fail(path, "cannot open file");

  const Header header = parse_header(file, path);
  int degree = 0;
  try {
    degree = degree_from_rest_property_count(static_cast<int>(header.rest.size()));
  } catch (const std::invalid_argument& e) {
    fail(path, e.what());
  }
  const std::size_t rest = header.rest.size();  // per splat, all channels
  const std::size_t n = header.vertex_count;

  SplatData data;
  data.count = n;
  data.sh_degree = degree;
  data.position.resize(3 * n);
  data.scale.resize(3 * n);
  data.rotation.resize(4 * n);
  data.opacity.resize(n);
  data.sh_dc.resize(3 * n);
  data.sh_rest.resize(rest * n);

  // Stream in ~1 MiB chunks; multi-GB assets must not be slurped whole.
  const std::size_t rows_per_chunk = std::max<std::size_t>(1, (1u << 20) / header.stride);
  std::vector<char> chunk(rows_per_chunk * header.stride);

  std::size_t done = 0;
  while (done < n) {
    const std::size_t rows = std::min(rows_per_chunk, n - done);
    file.read(chunk.data(), static_cast<std::streamsize>(rows * header.stride));
    if (static_cast<std::size_t>(file.gcount()) != rows * header.stride) {
      fail(path, "truncated vertex data at splat " + std::to_string(done));
    }
    for (std::size_t r = 0; r < rows; ++r, ++done) {
      const char* row = chunk.data() + r * header.stride;

      data.position[3 * done + 0] = read_f32(row, header.x);
      data.position[3 * done + 1] = read_f32(row, header.y);
      data.position[3 * done + 2] = read_f32(row, header.z);

      for (std::size_t i = 0; i < 3; ++i) {
        data.scale[3 * done + i] = std::exp(read_f32(row, header.scale[i]));
        data.sh_dc[3 * done + i] = read_f32(row, header.dc[i]);
      }

      float q[4];
      float norm_sq = 0.0f;
      for (std::size_t i = 0; i < 4; ++i) {
        q[i] = read_f32(row, header.rot[i]);
        norm_sq += q[i] * q[i];
      }
      // Degenerate quaternions become identity rather than NaN-poisoning the frame.
      const float inv_norm = norm_sq > 0.0f ? 1.0f / std::sqrt(norm_sq) : 0.0f;
      data.rotation[4 * done + 0] = norm_sq > 0.0f ? q[0] * inv_norm : 1.0f;
      for (std::size_t i = 1; i < 4; ++i) {
        data.rotation[4 * done + i] = q[i] * inv_norm;
      }

      data.opacity[done] = sigmoid(read_f32(row, header.opacity));

      for (std::size_t i = 0; i < rest; ++i) {
        data.sh_rest[rest * done + i] = read_f32(row, header.rest[i]);
      }
    }
  }
  return data;
}

}  // namespace gsr::loader
