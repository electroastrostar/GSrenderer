#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "loader/ply_loader.hpp"
#include "loader/splat_data.hpp"

namespace fs = std::filesystem;

namespace {

// ---- synthetic INRIA-style PLY builder -------------------------------------------------

struct TestSplat {
  float pos[3];
  float scale_log[3];
  float rot[4];  // w x y z, as stored (unnormalized allowed)
  float opacity_logit;
  float dc[3];
  std::vector<float> rest;  // 3 * rest_coeffs_for_degree(degree)
};

// INRIA property order: x y z nx ny nz f_dc_* f_rest_* opacity scale_* rot_*
std::vector<std::string> inria_property_order(int degree, bool with_normals) {
  std::vector<std::string> names = {"x", "y", "z"};
  if (with_normals) names.insert(names.end(), {"nx", "ny", "nz"});
  for (int i = 0; i < 3; ++i) names.push_back("f_dc_" + std::to_string(i));
  const int rest = 3 * gsr::loader::rest_coeffs_for_degree(degree);
  for (int i = 0; i < rest; ++i) names.push_back("f_rest_" + std::to_string(i));
  names.push_back("opacity");
  for (int i = 0; i < 3; ++i) names.push_back("scale_" + std::to_string(i));
  for (int i = 0; i < 4; ++i) names.push_back("rot_" + std::to_string(i));
  return names;
}

float field_value(const TestSplat& s, const std::string& name) {
  if (name == "x") return s.pos[0];
  if (name == "y") return s.pos[1];
  if (name == "z") return s.pos[2];
  if (name == "nx" || name == "ny" || name == "nz") return 0.0f;
  if (name == "opacity") return s.opacity_logit;
  if (name.rfind("f_dc_", 0) == 0) return s.dc[std::stoul(name.substr(5))];
  if (name.rfind("f_rest_", 0) == 0) return s.rest.at(std::stoul(name.substr(7)));
  if (name.rfind("scale_", 0) == 0) return s.scale_log[std::stoul(name.substr(6))];
  if (name.rfind("rot_", 0) == 0) return s.rot[std::stoul(name.substr(4))];
  FAIL("unknown test property " + name);
  return 0.0f;
}

std::string build_ply(const std::vector<std::string>& property_order,
                      const std::vector<TestSplat>& splats,
                      const std::string& format = "binary_little_endian",
                      long vertex_count_override = -1) {
  std::string out = "ply\nformat " + format + " 1.0\ncomment synthetic test asset\n";
  const std::size_t count =
      vertex_count_override >= 0 ? static_cast<std::size_t>(vertex_count_override) : splats.size();
  out += "element vertex " + std::to_string(count) + "\n";
  for (const auto& name : property_order) out += "property float " + name + "\n";
  out += "end_header\n";
  for (const auto& s : splats) {
    for (const auto& name : property_order) {
      const float v = field_value(s, name);
      char bytes[sizeof(float)];
      std::memcpy(bytes, &v, sizeof v);
      out.append(bytes, sizeof v);
    }
  }
  return out;
}

// Writes bytes to a unique temp file, removed when the fixture goes out of scope.
class TempPly {
 public:
  explicit TempPly(const std::string& bytes) {
    static std::mt19937_64 rng{std::random_device{}()};
    path_ = fs::temp_directory_path() /
            ("gsr_test_" + std::to_string(rng()) + ".ply");
    std::ofstream f(path_, std::ios::binary);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
  ~TempPly() { std::error_code ec; fs::remove(path_, ec); }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

TestSplat make_splat(int degree, float seed) {
  TestSplat s{};
  for (int i = 0; i < 3; ++i) s.pos[i] = seed + static_cast<float>(i);
  for (int i = 0; i < 3; ++i) s.scale_log[i] = 0.1f * seed;
  s.rot[0] = 2.0f;  // unnormalized on purpose; loader must normalize to (1,0,0,0)
  s.rot[1] = s.rot[2] = s.rot[3] = 0.0f;
  s.opacity_logit = 0.0f;  // sigmoid(0) = 0.5 exactly
  for (int i = 0; i < 3; ++i) s.dc[i] = seed * 10.0f + static_cast<float>(i);
  s.rest.resize(3u * static_cast<unsigned>(gsr::loader::rest_coeffs_for_degree(degree)));
  for (std::size_t i = 0; i < s.rest.size(); ++i) {
    s.rest[i] = seed + 0.01f * static_cast<float>(i);
  }
  return s;
}

}  // namespace

// ---- happy paths ------------------------------------------------------------------------

TEST_CASE("load_ply reads a hand-checked degree-1 fixture", "[loader][ply]") {
  // One splat with values chosen so expected outputs are derivable on paper:
  // scale_log = ln(2) -> scale 2.0; opacity logit 0 -> 0.5; quat (2,0,0,0) -> (1,0,0,0).
  TestSplat s{};
  s.pos[0] = 1.5f; s.pos[1] = -2.0f; s.pos[2] = 3.25f;
  s.scale_log[0] = s.scale_log[1] = s.scale_log[2] = std::log(2.0f);
  s.rot[0] = 2.0f;
  s.opacity_logit = 0.0f;
  s.dc[0] = 0.25f; s.dc[1] = 0.5f; s.dc[2] = -0.75f;
  s.rest.assign(9, 0.0f);
  s.rest[0] = 0.125f; s.rest[8] = -0.5f;

  TempPly ply(build_ply(inria_property_order(1, true), {s}));
  const auto data = gsr::loader::load_ply(ply.path());

  REQUIRE(data.count == 1);
  CHECK(data.sh_degree == 1);
  CHECK(data.position[0] == Catch::Approx(1.5f));
  CHECK(data.position[1] == Catch::Approx(-2.0f));
  CHECK(data.position[2] == Catch::Approx(3.25f));
  CHECK(data.scale[0] == Catch::Approx(2.0f));
  CHECK(data.scale[2] == Catch::Approx(2.0f));
  CHECK(data.rotation[0] == Catch::Approx(1.0f));
  CHECK(data.rotation[1] == Catch::Approx(0.0f));
  CHECK(data.opacity[0] == Catch::Approx(0.5f));
  CHECK(data.sh_dc[0] == Catch::Approx(0.25f));
  CHECK(data.sh_dc[2] == Catch::Approx(-0.75f));
  REQUIRE(data.sh_rest.size() == 9);
  CHECK(data.sh_rest[0] == Catch::Approx(0.125f));
  CHECK(data.sh_rest[8] == Catch::Approx(-0.5f));
}

TEST_CASE("load_ply handles SH degrees 0 through 3", "[loader][ply]") {
  for (int degree = 0; degree <= 3; ++degree) {
    DYNAMIC_SECTION("degree " << degree) {
      const std::vector<TestSplat> splats = {make_splat(degree, 1.0f), make_splat(degree, 2.0f),
                                             make_splat(degree, 3.0f)};
      TempPly ply(build_ply(inria_property_order(degree, true), splats));
      const auto data = gsr::loader::load_ply(ply.path());

      const auto rest =
          static_cast<std::size_t>(3 * gsr::loader::rest_coeffs_for_degree(degree));
      REQUIRE(data.count == 3);
      CHECK(data.sh_degree == degree);
      REQUIRE(data.sh_rest.size() == rest * 3);
      for (std::size_t i = 0; i < 3; ++i) {
        CHECK(data.position[3 * i] == Catch::Approx(splats[i].pos[0]));
        CHECK(data.opacity[i] == Catch::Approx(0.5f));
        if (rest > 0) {
          CHECK(data.sh_rest[rest * i] == Catch::Approx(splats[i].rest[0]));
          CHECK(data.sh_rest[rest * i + rest - 1] == Catch::Approx(splats[i].rest[rest - 1]));
        }
      }
    }
  }
}

TEST_CASE("load_ply is independent of property order and skips unknown scalars",
          "[loader][ply]") {
  const TestSplat s = make_splat(1, 4.0f);
  // Scrambled order, no normals.
  std::vector<std::string> order = inria_property_order(1, false);
  std::reverse(order.begin(), order.end());
  TempPly ply(build_ply(order, {s}));
  const auto data = gsr::loader::load_ply(ply.path());
  REQUIRE(data.count == 1);
  CHECK(data.sh_degree == 1);
  CHECK(data.position[0] == Catch::Approx(s.pos[0]));
  CHECK(data.sh_dc[1] == Catch::Approx(s.dc[1]));
  CHECK(data.sh_rest[3] == Catch::Approx(s.rest[3]));
}

TEST_CASE("load_ply accepts zero splats", "[loader][ply]") {
  TempPly ply(build_ply(inria_property_order(0, true), {}));
  const auto data = gsr::loader::load_ply(ply.path());
  CHECK(data.count == 0);
  CHECK(data.sh_degree == 0);
}

// ---- malformed input --------------------------------------------------------------------

TEST_CASE("load_ply rejects malformed files", "[loader][ply]") {
  const TestSplat s = make_splat(1, 5.0f);

  SECTION("nonexistent file") {
    CHECK_THROWS_AS(gsr::loader::load_ply("/nonexistent/nope.ply"), std::runtime_error);
  }
  SECTION("missing ply magic") {
    TempPly ply("plyx\nformat binary_little_endian 1.0\nend_header\n");
    CHECK_THROWS_AS(gsr::loader::load_ply(ply.path()), std::runtime_error);
  }
  SECTION("ascii format") {
    TempPly ply(build_ply(inria_property_order(1, true), {s}, "ascii"));
    CHECK_THROWS_AS(gsr::loader::load_ply(ply.path()), std::runtime_error);
  }
  SECTION("big-endian format") {
    TempPly ply(build_ply(inria_property_order(1, true), {s}, "binary_big_endian"));
    CHECK_THROWS_AS(gsr::loader::load_ply(ply.path()), std::runtime_error);
  }
  SECTION("missing required property (opacity)") {
    auto order = inria_property_order(1, true);
    std::erase(order, "opacity");
    TempPly ply(build_ply(order, {s}));
    CHECK_THROWS_AS(gsr::loader::load_ply(ply.path()), std::runtime_error);
  }
  SECTION("f_rest count that matches no SH degree") {
    auto order = inria_property_order(1, true);  // has f_rest_0..8
    std::erase(order, "f_rest_8");               // now 8 -> invalid
    TestSplat t = s;
    TempPly ply(build_ply(order, {t}));
    CHECK_THROWS_AS(gsr::loader::load_ply(ply.path()), std::runtime_error);
  }
  SECTION("truncated vertex data") {
    // Header claims 3 splats, body contains 2.
    TempPly ply(build_ply(inria_property_order(1, true), {s, s}, "binary_little_endian", 3));
    CHECK_THROWS_AS(gsr::loader::load_ply(ply.path()), std::runtime_error);
  }
  SECTION("unterminated header") {
    TempPly ply("ply\nformat binary_little_endian 1.0\nelement vertex 1\nproperty float x\n");
    CHECK_THROWS_AS(gsr::loader::load_ply(ply.path()), std::runtime_error);
  }
}
