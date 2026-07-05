#include "core/camera.hpp"

#include <cmath>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

namespace gsr::core {

Intrinsics intrinsics_from_fov(float fov_y_rad, int width, int height, float znear,
                               float zfar) {
  if (fov_y_rad <= 0.0f || fov_y_rad >= glm::pi<float>()) {
    throw std::invalid_argument("intrinsics_from_fov: fov_y_rad out of (0, pi)");
  }
  if (width <= 0 || height <= 0 || znear <= 0.0f || zfar <= znear) {
    throw std::invalid_argument("intrinsics_from_fov: invalid size or clip planes");
  }
  Intrinsics intr;
  intr.fy = 0.5f * static_cast<float>(height) / std::tan(0.5f * fov_y_rad);
  intr.fx = intr.fy;  // square pixels
  intr.cx = 0.5f * static_cast<float>(width);
  intr.cy = 0.5f * static_cast<float>(height);
  intr.width = width;
  intr.height = height;
  intr.znear = znear;
  intr.zfar = zfar;
  return intr;
}

Intrinsics with_overscan(const Intrinsics& intr, float fraction) {
  if (fraction < 0.0f) {
    throw std::invalid_argument("with_overscan: fraction must be >= 0");
  }
  const int pad_x = static_cast<int>(std::lround(0.5f * fraction * intr.width));
  const int pad_y = static_cast<int>(std::lround(0.5f * fraction * intr.height));
  Intrinsics out = intr;
  out.width = intr.width + 2 * pad_x;
  out.height = intr.height + 2 * pad_y;
  out.cx = intr.cx + static_cast<float>(pad_x);
  out.cy = intr.cy + static_cast<float>(pad_y);
  return out;
}

glm::mat4 view_from_world(const CameraPose& pose) {
  const glm::mat3 r_transpose = glm::transpose(glm::mat3_cast(pose.orientation));
  glm::mat4 view(r_transpose);
  view[3] = glm::vec4(-(r_transpose * pose.position), 1.0f);
  return view;
}

glm::mat4 clip_from_view(const Intrinsics& intr) {
  if (intr.fx <= 0.0f || intr.fy <= 0.0f || intr.width <= 0 || intr.height <= 0 ||
      intr.znear <= 0.0f || intr.zfar <= intr.znear) {
    throw std::invalid_argument("clip_from_view: invalid intrinsics");
  }
  // Frustum edges on the near plane from pixel intrinsics. Image y points down while
  // camera y points up, hence top comes from cy (pixels above the principal point).
  const float left = -intr.cx * intr.znear / intr.fx;
  const float right = (static_cast<float>(intr.width) - intr.cx) * intr.znear / intr.fx;
  const float top = intr.cy * intr.znear / intr.fy;
  const float bottom = -(static_cast<float>(intr.height) - intr.cy) * intr.znear / intr.fy;
  return glm::frustum(left, right, bottom, top, intr.znear, intr.zfar);
}

glm::mat4 clip_from_world(const Intrinsics& intr, const CameraPose& pose) {
  return clip_from_view(intr) * view_from_world(pose);
}

glm::vec3 forward_world(const CameraPose& pose) {
  return pose.orientation * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::vec3 right_world(const CameraPose& pose) {
  return pose.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 up_world(const CameraPose& pose) {
  return pose.orientation * glm::vec3(0.0f, 1.0f, 0.0f);
}

}  // namespace gsr::core
