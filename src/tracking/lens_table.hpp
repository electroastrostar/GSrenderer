#pragma once

#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

// Lens model (plan Phase 3 Task 5): maps FreeD zoom raw encoder counts to focal length.
// Raw counts are tracker/lens-specific and meaningless without calibration — without a
// table, use fixed() intrinsics from config (plan: "if no table is present, allow fixed
// intrinsics"). A stage lens-calibration pass replaces this before the Phase 5 milestone
// (plan §6.5).
namespace gsr::tracking {

class LensTable {
 public:
  // CSV, one mapping per line: "zoom_raw,focal_mm". Lines starting with '#' and a
  // non-numeric header line are skipped; extra columns (e.g. focus) are ignored.
  // Compatible with column exports from Mo-Sys lens files. Throws std::runtime_error
  // on unreadable/empty/malformed input (load time).
  static LensTable from_csv(const std::filesystem::path& path);

  // Fixed-focal fallback: focal_mm(anything) == focal.
  static LensTable fixed(float focal_mm);

  // Focal length in mm for a raw zoom count: linear interpolation between calibration
  // rows, clamped to the table's end values outside its range.
  float focal_mm(std::uint32_t zoom_raw) const;

  bool is_fixed() const { return rows_.empty(); }

 private:
  LensTable() = default;
  std::vector<std::pair<std::uint32_t, float>> rows_;  // sorted by zoom_raw
  float fixed_focal_mm_ = 0.0f;
};

// Pixel focal length from physical focal length: f_px = f_mm / sensor_size_mm * pixels.
// Use the matching axis (sensor height with image height for fy). Pure conversion —
// lives here with the lens model, not in core, because mm-domain values only exist at
// this boundary.
float focal_px_from_mm(float focal_mm, float sensor_size_mm, int pixels);

}  // namespace gsr::tracking
