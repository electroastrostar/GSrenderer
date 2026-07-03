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

}  // namespace gsr::core
