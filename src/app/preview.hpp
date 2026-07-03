#pragma once

#include "loader/splat_data.hpp"
#include "renderer/splat_renderer.hpp"

// Interactive debug preview (Phase 2): GLFW window fed by the CUDA renderer through a
// GL pixel-buffer-object interop path (no CPU round-trip). Free-fly camera:
//   WASD move, Q/E down/up, right-mouse drag to look, Shift = 5x speed, Esc quits.
// Window title carries the timing HUD; a frame-stamped summary logs once per second.
// Only compiled when the toolchain has CUDA (see src/app/CMakeLists.txt).
namespace gsr::app {

struct PreviewOptions {
  int width = 1920;
  int height = 1080;
  float fov_y_rad = 0.0f;  // 0 = default 60 degrees
  int sh_degree_clamp = -1;
  bool vsync = false;  // keep off for perf measurement (plan Phase 2 perf gate)
  // COLMAP-convention assets are y-down; by default the scene is viewed through the
  // world_from_asset flip so it appears upright (same default as SuperSplat).
  // --no-flip disables this for assets exported already y-up.
  bool flip_scene = true;

  // Tracked-camera mode (Phase 3): >= 0 listens for FreeD D1 on this UDP port and the
  // tracker drives the camera (fly controls stay active until the first packet).
  int freed_port = -1;
  float latency_ms = 0.0f;        // prediction offset: predict(now + latency)
  std::string lens_csv;           // zoom->focal table; empty = fixed FOV intrinsics
  float sensor_height_mm = 24.0f; // sensor height matching the lens table's focal values
};

// Blocks until the window closes. Returns process exit code (0 = clean run).
// Throws only during setup; per-frame errors are logged and end the loop.
int run_preview(const loader::SplatData& data, const PreviewOptions& options);

}  // namespace gsr::app
