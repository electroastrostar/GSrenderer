#pragma once

#include <glm/glm.hpp>

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

}  // namespace gsr::core
