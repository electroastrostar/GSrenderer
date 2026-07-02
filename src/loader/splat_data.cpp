#include "loader/splat_data.hpp"

#include <stdexcept>
#include <string>

namespace gsr::loader {

int rest_coeffs_for_degree(int degree) {
  if (degree < 0 || degree > 3) {
    throw std::invalid_argument("SH degree out of range 0..3: " + std::to_string(degree));
  }
  return (degree + 1) * (degree + 1) - 1;
}

int degree_from_rest_property_count(int rest_property_count) {
  for (int degree = 0; degree <= 3; ++degree) {
    if (rest_property_count == 3 * rest_coeffs_for_degree(degree)) {
      return degree;
    }
  }
  throw std::invalid_argument("f_rest_* property count " + std::to_string(rest_property_count) +
                              " does not match SH degree 0..3 (expected 0, 9, 24 or 45)");
}

Bounds compute_bounds(const SplatData& data) {
  if (data.count == 0) {
    throw std::invalid_argument("compute_bounds: empty SplatData");
  }
  Bounds b{{data.position[0], data.position[1], data.position[2]},
           {data.position[0], data.position[1], data.position[2]}};
  for (std::size_t i = 1; i < data.count; ++i) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
      const float v = data.position[3 * i + axis];
      if (v < b.min[axis]) b.min[axis] = v;
      if (v > b.max[axis]) b.max[axis] = v;
    }
  }
  return b;
}

std::size_t byte_size(const SplatData& data) {
  return sizeof(float) * (data.position.size() + data.scale.size() + data.rotation.size() +
                          data.opacity.size() + data.sh_dc.size() + data.sh_rest.size());
}

}  // namespace gsr::loader
