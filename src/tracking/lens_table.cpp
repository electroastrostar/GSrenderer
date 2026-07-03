#include "tracking/lens_table.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace gsr::tracking {

namespace {
[[noreturn]] void fail(const std::filesystem::path& path, const std::string& reason) {
  throw std::runtime_error("LensTable(" + path.string() + "): " + reason);
}
}  // namespace

LensTable LensTable::from_csv(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) fail(path, "cannot open file");

  LensTable table;
  std::string line;
  std::size_t line_no = 0;
  bool header_allowed = true;  // one non-numeric header line may precede the data
  while (std::getline(file, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;

    std::istringstream fields(line);
    std::string zoom_text, focal_text;
    if (!std::getline(fields, zoom_text, ',') || !std::getline(fields, focal_text, ',')) {
      if (header_allowed) {
        header_allowed = false;
        continue;
      }
      fail(path, "line " + std::to_string(line_no) + ": expected 'zoom_raw,focal_mm'");
    }
    try {
      const auto zoom = static_cast<std::uint32_t>(std::stoul(zoom_text));
      const float focal = std::stof(focal_text);
      if (focal <= 0.0f) {
        fail(path, "line " + std::to_string(line_no) + ": focal_mm must be positive");
      }
      table.rows_.emplace_back(zoom, focal);
      header_allowed = false;
    } catch (const std::invalid_argument&) {
      if (header_allowed) {
        header_allowed = false;
        continue;
      }
      fail(path, "line " + std::to_string(line_no) + ": non-numeric value");
    } catch (const std::out_of_range&) {
      fail(path, "line " + std::to_string(line_no) + ": value out of range");
    }
  }
  if (table.rows_.empty()) fail(path, "no calibration rows");
  std::sort(table.rows_.begin(), table.rows_.end());
  return table;
}

LensTable LensTable::fixed(float focal_mm) {
  if (focal_mm <= 0.0f) throw std::runtime_error("LensTable::fixed: focal_mm must be > 0");
  LensTable table;
  table.fixed_focal_mm_ = focal_mm;
  return table;
}

float LensTable::focal_mm(std::uint32_t zoom_raw) const {
  if (rows_.empty()) return fixed_focal_mm_;
  if (zoom_raw <= rows_.front().first) return rows_.front().second;
  if (zoom_raw >= rows_.back().first) return rows_.back().second;

  const auto upper = std::lower_bound(
      rows_.begin(), rows_.end(), zoom_raw,
      [](const std::pair<std::uint32_t, float>& row, std::uint32_t key) {
        return row.first < key;
      });
  const auto lower = upper - 1;
  const float span = static_cast<float>(upper->first - lower->first);
  const float fraction =
      span > 0.0f ? static_cast<float>(zoom_raw - lower->first) / span : 0.0f;
  return lower->second + fraction * (upper->second - lower->second);
}

float focal_px_from_mm(float focal_mm, float sensor_size_mm, int pixels) {
  return focal_mm / sensor_size_mm * static_cast<float>(pixels);
}

}  // namespace gsr::tracking
