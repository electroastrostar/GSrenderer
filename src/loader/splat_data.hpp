#pragma once

#include <array>
#include <cstddef>
#include <vector>

// CPU-side structure-of-arrays splat container, laid out for direct GPU upload.
// Memory layout and value conventions: docs/splat-memory-layout.md
namespace gsr::loader {

// Y_0^0 spherical-harmonics basis constant; base color = 0.5 + kShC0 * sh_dc.
inline constexpr float kShC0 = 0.28209479177387814f;

// SH "rest" (non-DC) coefficients per color channel for a degree: (d+1)^2 - 1.
// Throws std::invalid_argument for degrees outside 0..3.
int rest_coeffs_for_degree(int degree);

// Inverse mapping from the number of f_rest_* properties in a PLY (all channels combined):
// 0 -> degree 0, 9 -> 1, 24 -> 2, 45 -> 3. Throws std::invalid_argument otherwise.
int degree_from_rest_property_count(int rest_property_count);

struct Bounds {
  std::array<float, 3> min;
  std::array<float, 3> max;
};

struct SplatData {
  std::size_t count = 0;
  int sh_degree = 0;  // 0..3

  // All arrays are tightly packed per splat (SoA). Sizes for N splats:
  std::vector<float> position;  // 3N  [x y z]      meters, asset space
  std::vector<float> scale;     // 3N  [sx sy sz]   linear extents (exp() applied at load)
  std::vector<float> rotation;  // 4N  [w x y z]    unit quaternion (normalized at load)
  std::vector<float> opacity;   // N                [0,1] (sigmoid applied at load)
  std::vector<float> sh_dc;     // 3N  [R G B]      DC SH coefficients, raw
  std::vector<float> sh_rest;   // 3*rest*N         channel-major per splat (PLY file order)
};

// Axis-aligned bounding box over positions. Throws std::invalid_argument when count == 0.
Bounds compute_bounds(const SplatData& data);

// Total payload bytes across all arrays (CPU copy == GPU upload size).
std::size_t byte_size(const SplatData& data);

}  // namespace gsr::loader
