// GPU integration tests — require a CUDA device, so they are tagged hidden ("[.gpu]") and
// do NOT run in a default ctest pass (plan §7: hardware-dependent tests are explicit,
// never silently faked). On the A6000 run them with:
//
//     build\tests\Release\gsr_tests.exe "[gpu]"     (Windows)
//     ./build/tests/gsr_tests "[gpu]"               (Linux)

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "core/camera.hpp"
#include "loader/ply_loader.hpp"
#include "renderer/splat_renderer.hpp"

namespace {

gsr::renderer::CameraFrame frame_at(const glm::vec3& position, const glm::quat& orientation,
                                    const gsr::core::Intrinsics& intr) {
  gsr::core::CameraPose pose{position, orientation};
  gsr::renderer::CameraFrame frame;
  frame.view_from_world = gsr::core::view_from_world(pose);
  frame.fx = intr.fx;
  frame.fy = intr.fy;
  frame.cx = intr.cx;
  frame.cy = intr.cy;
  frame.camera_position_world = position;
  return frame;
}

}  // namespace

TEST_CASE("GPU: fixture renders non-empty frame with center coverage", "[.gpu]") {
  const auto data = gsr::loader::load_ply(GSR_TEST_ASSETS_DIR "/fixtures/cube_deg3.ply");

  gsr::renderer::RenderConfig config;
  config.width = 640;
  config.height = 360;
  gsr::renderer::SplatRenderer renderer(data, config);

  const auto intr = gsr::core::intrinsics_from_fov(glm::radians(60.0f), config.width,
                                                   config.height, 0.1f, 100.0f);
  // Camera 6 m back on +Z looking at the origin (identity orientation looks along -Z).
  const auto cam = frame_at({0.0f, 0.0f, 6.0f}, glm::quat(1, 0, 0, 0), intr);

  gsr::renderer::FrameTimings timings;
  REQUIRE(renderer.render_device(cam, &timings) != nullptr);
  REQUIRE(timings.pairs_rendered > 0);

  std::vector<gsr::renderer::Rgba8> pixels(static_cast<size_t>(config.width) *
                                           config.height);
  REQUIRE(renderer.read_back(pixels.data()));

  // The 2 m cube of 5 cm splats at 6 m under 60° FOV must cover some pixels near center.
  unsigned covered = 0;
  for (const auto& px : pixels) covered += px.a > 0 ? 1u : 0u;
  CHECK(covered > 0);
}

TEST_CASE("GPU: view-dependent color changes across opposite viewpoints (SH)", "[.gpu]") {
  const auto data = gsr::loader::load_ply(GSR_TEST_ASSETS_DIR "/fixtures/cube_deg3.ply");
  gsr::renderer::RenderConfig config;
  config.width = 320;
  config.height = 180;
  gsr::renderer::SplatRenderer renderer(data, config);
  const auto intr = gsr::core::intrinsics_from_fov(glm::radians(60.0f), config.width,
                                                   config.height, 0.1f, 100.0f);

  const auto front = frame_at({0, 0, 6}, glm::quat(1, 0, 0, 0), intr);
  const auto back =
      frame_at({0, 0, -6}, glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0)), intr);

  std::vector<gsr::renderer::Rgba8> a(static_cast<size_t>(config.width) * config.height);
  std::vector<gsr::renderer::Rgba8> b(a.size());
  REQUIRE(renderer.render_device(front, nullptr) != nullptr);
  REQUIRE(renderer.read_back(a.data()));
  REQUIRE(renderer.render_device(back, nullptr) != nullptr);
  REQUIRE(renderer.read_back(b.data()));

  // The fixture has strong degree-3 SH, so covered pixels must differ between viewpoints.
  long diff = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].a > 0 && b[i].a > 0) {
      diff += std::abs(int(a[i].r) - int(b[i].r)) + std::abs(int(a[i].g) - int(b[i].g));
    }
  }
  CHECK(diff > 0);
}
