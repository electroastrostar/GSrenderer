#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/quaternion.hpp>

#include "core/camera.hpp"
#include "renderer/covariance.hpp"
#include "renderer/ewa_frame.hpp"

using Catch::Approx;

TEST_CASE("ewa_rotation_from_view: identity view gives diag(1,-1,-1)", "[renderer][cov]") {
  float w[9];
  gsr::renderer::ewa_rotation_from_view(glm::mat4(1.0f), w);
  const float expected[9] = {1, 0, 0, 0, -1, 0, 0, 0, -1};
  for (int i = 0; i < 9; ++i) CHECK(w[i] == Approx(expected[i]).margin(1e-6));
}

// Consistency property: for any pose, W * p_world must equal the point adapter applied to
// the view-space rotation of p (both routes into the projection frame agree).
TEST_CASE("ewa_rotation_from_view agrees with projection_frame_from_view", "[renderer][cov]") {
  gsr::core::CameraPose pose;
  pose.position = {2.0f, -1.0f, 4.0f};
  pose.orientation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(0.3f, 1.0f, -0.2f)));
  const glm::mat4 view = gsr::core::view_from_world(pose);

  float w[9];
  gsr::renderer::ewa_rotation_from_view(view, w);

  const glm::vec3 p_world{-3.0f, 0.5f, 1.25f};
  const glm::vec3 rotated = glm::mat3(view) * p_world;  // rotation part only
  float via_adapter[3];
  gsr::renderer::projection_frame_from_view(rotated.x, rotated.y, rotated.z, via_adapter);

  for (int r = 0; r < 3; ++r) {
    const float via_w =
        w[r * 3 + 0] * p_world.x + w[r * 3 + 1] * p_world.y + w[r * 3 + 2] * p_world.z;
    CHECK(via_w == Approx(via_adapter[r]).margin(1e-5));
  }
}
