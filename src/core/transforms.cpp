#include "core/transforms.hpp"

namespace gsr::core {

namespace {
// Rx(180°): (x, y, z) -> (x, -y, -z). Column-major glm.
const glm::mat4 kFlipX{1.0f, 0.0f,  0.0f,  0.0f,   // column 0
                       0.0f, -1.0f, 0.0f,  0.0f,   // column 1
                       0.0f, 0.0f,  -1.0f, 0.0f,   // column 2
                       0.0f, 0.0f,  0.0f,  1.0f};  // column 3
}  // namespace

glm::mat4 world_from_asset_rotation() { return kFlipX; }

glm::mat4 asset_from_world_rotation() { return kFlipX; }

glm::vec3 world_from_asset(const glm::vec3& p_asset) {
  return {p_asset.x, -p_asset.y, -p_asset.z};
}

glm::vec3 asset_from_world(const glm::vec3& p_world) {
  return {p_world.x, -p_world.y, -p_world.z};
}

CameraPose render_from_freed(float pan_rad, float tilt_rad, float roll_rad, float x_m,
                             float y_m, float z_m) {
  CameraPose pose;
  // Axis mapping: (x, y, z)_freed -> (-y, z, -x)_render.
  pose.position = {-y_m, z_m, -x_m};
  // Pan clockwise-from-above = negative rotation about render +Y (up). Tilt up =
  // positive rotation about camera-local +X. Roll clockwise-from-behind = negative
  // rotation about camera-local +Z (the backward axis). Intrinsic order pan->tilt->roll.
  pose.orientation = glm::angleAxis(-pan_rad, glm::vec3(0, 1, 0)) *
                     glm::angleAxis(tilt_rad, glm::vec3(1, 0, 0)) *
                     glm::angleAxis(-roll_rad, glm::vec3(0, 0, 1));
  return pose;
}

CameraPose world_from_stage(const CameraPose& stage_pose, float yaw_rad,
                            const glm::vec3& offset_m) {
  const glm::quat yaw = glm::angleAxis(yaw_rad, glm::vec3(0.0f, 1.0f, 0.0f));
  CameraPose out;
  out.position = yaw * stage_pose.position + offset_m;
  out.orientation = yaw * stage_pose.orientation;
  return out;
}

}  // namespace gsr::core
