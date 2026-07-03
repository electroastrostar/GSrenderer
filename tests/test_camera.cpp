#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

#include "core/camera.hpp"

using Catch::Approx;
using gsr::core::CameraPose;
using gsr::core::Intrinsics;

namespace {
glm::vec3 project_to_ndc(const glm::mat4& m, const glm::vec3& p) {
  const glm::vec4 clip = m * glm::vec4(p, 1.0f);
  return glm::vec3(clip) / clip.w;
}
}  // namespace

// Hand-checked fixture: 90° vertical FOV at 1920x1080 -> fy = 540/tan(45°) = 540.
TEST_CASE("intrinsics_from_fov: 90 deg at 1080p gives fy=540, centered", "[core][camera]") {
  const auto intr =
      gsr::core::intrinsics_from_fov(glm::half_pi<float>(), 1920, 1080, 0.1f, 100.0f);
  CHECK(intr.fy == Approx(540.0f));
  CHECK(intr.fx == Approx(540.0f));
  CHECK(intr.cx == Approx(960.0f));
  CHECK(intr.cy == Approx(540.0f));
}

TEST_CASE("intrinsics_from_fov rejects invalid input", "[core][camera]") {
  CHECK_THROWS_AS(gsr::core::intrinsics_from_fov(0.0f, 1920, 1080, 0.1f, 100.0f),
                  std::invalid_argument);
  CHECK_THROWS_AS(gsr::core::intrinsics_from_fov(1.0f, 1920, -1, 0.1f, 100.0f),
                  std::invalid_argument);
  CHECK_THROWS_AS(gsr::core::intrinsics_from_fov(1.0f, 1920, 1080, 1.0f, 0.5f),
                  std::invalid_argument);
}

TEST_CASE("view_from_world: identity pose is identity", "[core][camera]") {
  const glm::mat4 v = gsr::core::view_from_world(CameraPose{});
  CHECK(v == glm::mat4(1.0f));
}

// Hand-checked: camera at (1,2,3), no rotation -> pure translation by (-1,-2,-3).
TEST_CASE("view_from_world: translation only", "[core][camera]") {
  CameraPose pose;
  pose.position = {1.0f, 2.0f, 3.0f};
  const glm::vec4 origin_in_view = gsr::core::view_from_world(pose) * glm::vec4(0, 0, 0, 1);
  CHECK(origin_in_view.x == Approx(-1.0f));
  CHECK(origin_in_view.y == Approx(-2.0f));
  CHECK(origin_in_view.z == Approx(-3.0f));
}

// Hand-checked on paper: camera rotated +90° about Y looks along world -X, so the world
// point (-5,0,0) must land on the view-space optical axis at (0,0,-5).
TEST_CASE("view_from_world: yaw 90 deg maps -X world onto the optical axis",
          "[core][camera]") {
  CameraPose pose;
  pose.orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 1, 0));
  const glm::vec4 p = gsr::core::view_from_world(pose) * glm::vec4(-5, 0, 0, 1);
  CHECK(p.x == Approx(0.0f).margin(1e-5));
  CHECK(p.y == Approx(0.0f).margin(1e-5));
  CHECK(p.z == Approx(-5.0f).epsilon(1e-5));

  CHECK(gsr::core::forward_world(pose).x == Approx(-1.0f).epsilon(1e-5));
  CHECK(gsr::core::right_world(pose).z == Approx(-1.0f).epsilon(1e-5));
  CHECK(gsr::core::up_world(pose).y == Approx(1.0f).epsilon(1e-5));
}

TEST_CASE("clip_from_view: centered intrinsics match glm::perspective", "[core][camera]") {
  const float fov = glm::radians(60.0f);
  const auto intr = gsr::core::intrinsics_from_fov(fov, 1920, 1080, 0.1f, 100.0f);
  const glm::mat4 ours = gsr::core::clip_from_view(intr);
  const glm::mat4 ref = glm::perspective(fov, 1920.0f / 1080.0f, 0.1f, 100.0f);
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      CHECK(ours[c][r] == Approx(ref[c][r]).margin(1e-5));
    }
  }
}

// Hand-checked off-axis case: principal point at 3/4 width. A point on the optical axis
// (0,0,-d) projects to pixel u = cx, i.e. NDC x = 2*cx/width - 1 = 0.5.
TEST_CASE("clip_from_view: shifted principal point yields off-axis frustum",
          "[core][camera]") {
  auto intr = gsr::core::intrinsics_from_fov(glm::half_pi<float>(), 1920, 1080, 0.1f, 100.0f);
  intr.cx = 0.75f * 1920.0f;
  const glm::mat4 proj = gsr::core::clip_from_view(intr);
  const glm::vec3 on_axis = project_to_ndc(proj, {0, 0, -10});
  CHECK(on_axis.x == Approx(0.5f).epsilon(1e-4));
  CHECK(on_axis.y == Approx(0.0f).margin(1e-5));

  // Image y points down: a point ABOVE the axis (+y view) must land at NDC y > 0.
  intr = gsr::core::intrinsics_from_fov(glm::half_pi<float>(), 1920, 1080, 0.1f, 100.0f);
  const glm::vec3 above = project_to_ndc(gsr::core::clip_from_view(intr), {0, 2, -10});
  CHECK(above.y > 0.0f);
}

TEST_CASE("clip_from_view rejects invalid intrinsics", "[core][camera]") {
  CHECK_THROWS_AS(gsr::core::clip_from_view(Intrinsics{}), std::invalid_argument);
}

// Composition sanity: a point 10 m straight ahead of a translated+rotated camera lands at
// NDC center with depth inside [-1,1].
TEST_CASE("clip_from_world composes view and projection", "[core][camera]") {
  const auto intr =
      gsr::core::intrinsics_from_fov(glm::radians(60.0f), 1920, 1080, 0.1f, 100.0f);
  CameraPose pose;
  pose.position = {3.0f, 1.0f, -2.0f};
  pose.orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 1, 0));
  const glm::vec3 ahead = pose.position + 10.0f * gsr::core::forward_world(pose);
  const glm::vec3 ndc = project_to_ndc(gsr::core::clip_from_world(intr, pose), ahead);
  CHECK(ndc.x == Approx(0.0f).margin(1e-4));
  CHECK(ndc.y == Approx(0.0f).margin(1e-4));
  CHECK(ndc.z > -1.0f);
  CHECK(ndc.z < 1.0f);
}
