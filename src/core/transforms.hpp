#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/camera.hpp"

// Named inter-space transforms (CLAUDE.md coordinate rules). Single source of truth for
// the conventions: docs/architecture.md §Coordinate Spaces.
namespace gsr::core {

// Splat asset space follows the COLMAP/INRIA convention: right-handed, +Y DOWN (gravity
// points along +Y), +Z forward. Render world is right-handed, +Y UP. They are related by
// a 180° rotation about X — an involution, so both directions use the same matrix.
// (Reference viewers like SuperSplat apply this same flip by default.)
glm::mat4 world_from_asset_rotation();
glm::mat4 asset_from_world_rotation();

// Point transforms (rotation only — the spaces share the origin and metric scale).
glm::vec3 world_from_asset(const glm::vec3& p_asset);
glm::vec3 asset_from_world(const glm::vec3& p_world);

// FreeD/StarTracker space (docs/freed-protocol.md): right-handed, Z up, pan-zero camera
// faces +X; pan positive = clockwise seen from above, tilt positive = up, roll positive
// = clockwise from behind the camera; composition pan -> tilt -> roll. Render world:
// Y up, identity camera faces -Z. Axis mapping (proper rotation, det +1):
//   freed X -> render -Z,  freed Y -> render -X,  freed Z -> render +Y
// so a pan-zero FreeD camera lands exactly on the render identity orientation.
// VERIFY signs against the real StarTracker on stage (plan §6 watch item); the
// simulator and these tests pin today's documented assumption. Takes the six pose
// scalars (radians/meters, already converted at the FreeD parse boundary) so core does
// not depend on the tracking module.
CameraPose render_from_freed(float pan_rad, float tilt_rad, float roll_rad, float x_m,
                             float y_m, float z_m);

}  // namespace gsr::core
