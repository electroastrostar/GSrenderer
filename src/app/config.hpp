#pragma once

#include <string>

// Run configuration (plan §4: one TOML file per run). CLI flags override file values;
// the file overrides the built-in defaults below. Schema: configs/example_run.toml.
namespace gsr::app {

struct RunConfig {
  std::string asset;  // splat .ply path (required by the time the app runs)

  // [output]
  int width = 1920;
  int height = 1080;
  float fov_deg = 60.0f;      // degrees on the wire/CLI (I/O boundary); radians internally
  bool vsync = false;
  float overscan_pct = 0.0f;  // Mode B overscan, percent (10 = 10%)

  // [render]
  int sh_clamp = -1;  // -1 = full asset degree
  bool flip_scene = true;

  // [tracking]
  int freed_port = -1;  // -1 = tracking off
  float latency_ms = 0.0f;

  // [lens]
  std::string lens_file;
  float sensor_height_mm = 24.0f;

  // [stage] — tracker origin -> scene origin alignment (world_from_stage)
  float stage_yaw_deg = 0.0f;
  float stage_offset_m[3] = {0.0f, 0.0f, 0.0f};
};

// Overlays TOML file values onto *config (fields absent from the file keep their current
// values). Throws std::runtime_error on unreadable or malformed input — load time only.
void apply_config_file(const std::string& path, RunConfig* config);

}  // namespace gsr::app
