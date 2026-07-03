#pragma once

#include <glm/glm.hpp>

// Host-side companion to covariance.hpp's projection-frame convention.
namespace gsr::renderer {

// Named convention function: extract the world -> projection-frame rotation (row-major,
// 9 floats) from a view matrix. The projection frame is view space with y and z negated
// (y down, +z forward) — the same flip projection_frame_from_view() applies to points.
inline void ewa_rotation_from_view(const glm::mat4& view_from_world, float w_rows[9]) {
  for (int c = 0; c < 3; ++c) {
    w_rows[0 * 3 + c] = view_from_world[c][0];
    w_rows[1 * 3 + c] = -view_from_world[c][1];
    w_rows[2 * 3 + c] = -view_from_world[c][2];
  }
}

}  // namespace gsr::renderer
