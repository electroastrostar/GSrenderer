#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Camera model. Renderer camera space (docs/architecture.md §Coordinate Spaces):
// right-handed, +X right, +Y up, -Z forward. In Phase 2 world space == splat asset space;
// the FreeD/UE transforms arrive in Phases 3/4 as their own X_from_Y functions.
namespace gsr::core {

// Pinhole intrinsics in pixels. cx/cy measured from the top-left image corner (image y
// points down, camera y points up — the projection accounts for the flip).
struct Intrinsics {
  float fx = 0.0f, fy = 0.0f;  // focal lengths, px
  float cx = 0.0f, cy = 0.0f;  // principal point, px from top-left
  int width = 0, height = 0;   // image size, px
  float znear = 0.1f, zfar = 1000.0f;  // clip planes, meters
};

// Symmetric-frustum convenience: vertical FOV in RADIANS (convert at I/O boundaries only),
// square pixels, centered principal point.
Intrinsics intrinsics_from_fov(float fov_y_rad, int width, int height, float znear,
                               float zfar);

struct CameraPose {
  glm::vec3 position{0.0f};                        // world, meters
  glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};   // rotates camera-space vectors to world
};

// World -> view (camera) transform for the pose.
glm::mat4 view_from_world(const CameraPose& pose);

// View -> clip. Built from pixel intrinsics via an asymmetric (off-axis) frustum, so a
// shifted principal point already produces the off-axis projection Phase 4 needs.
// OpenGL clip conventions (z in [-1, 1]).
glm::mat4 clip_from_view(const Intrinsics& intr);

// Convenience composition: world -> clip.
glm::mat4 clip_from_world(const Intrinsics& intr, const CameraPose& pose);

// Camera-space forward/right/up axes expressed in world space (unit vectors).
glm::vec3 forward_world(const CameraPose& pose);
glm::vec3 right_world(const CameraPose& pose);
glm::vec3 up_world(const CameraPose& pose);

}  // namespace gsr::core
