#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include <glm/glm.hpp>

#include "app/config.hpp"
#include "core/log.hpp"
#include "core/version.hpp"
#include "loader/ply_loader.hpp"

#ifdef GSR_CUDA_ENABLED
#include "app/preview.hpp"
#endif

namespace {

void print_usage() {
  std::fprintf(
      stderr,
      "usage: splatcast [asset.ply] [options]\n"
      "  --config PATH   TOML run config (configs/example_run.toml); flags override it\n"
      "  --width N / --height N    output size px (default 1920x1080)\n"
      "  --fov DEG       vertical field of view (default 60)\n"
      "  --overscan PCT  Mode B overscan percent; base image = exact center crop\n"
      "  --sh-clamp N    clamp SH degree 0..3 (default: asset degree)\n"
      "  --vsync         lock to display refresh (default off)\n"
      "  --no-flip       don't apply the COLMAP y-down -> y-up scene flip\n"
      "  --freed-port N  listen for FreeD tracking on UDP port N (e.g. 8001)\n"
      "  --latency-ms F  tracking prediction offset ([ / ] adjust live)\n"
      "  --lens-file P   zoom->focal CSV (configs/example_lens.csv)\n"
      "  --sensor-height-mm F      sensor height for the lens table (default 24)\n"
      "  --ndi NAME      stream frames as this NDI source (requires the NDI SDK)\n"
      "  --fps F         NDI target frame rate 24/25/30 (default 30)\n"
      "  --stage-yaw-deg F         stage alignment yaw about up, +CCW from above\n"
      "  --stage-offset X,Y,Z      stage alignment offset, meters\n");
}

// Two-pass parse: --config loads the file first, then every other flag overrides it.
bool parse_cli(int argc, char** argv, gsr::app::RunConfig* cfg) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      gsr::app::apply_config_file(argv[i + 1], cfg);
    }
  }
  for (int i = 1; i < argc; ++i) {
    const auto value = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: %s needs a value\n", flag);
        return nullptr;
      }
      return argv[++i];
    };
    const char* v = nullptr;
    if (argv[i][0] != '-') { cfg->asset = argv[i]; }
    else if (std::strcmp(argv[i], "--config") == 0) { ++i; /* handled in pass 1 */ }
    else if (std::strcmp(argv[i], "--width") == 0 && (v = value("--width"))) cfg->width = std::stoi(v);
    else if (std::strcmp(argv[i], "--height") == 0 && (v = value("--height"))) cfg->height = std::stoi(v);
    else if (std::strcmp(argv[i], "--fov") == 0 && (v = value("--fov"))) cfg->fov_deg = std::stof(v);
    else if (std::strcmp(argv[i], "--overscan") == 0 && (v = value("--overscan"))) cfg->overscan_pct = std::stof(v);
    else if (std::strcmp(argv[i], "--sh-clamp") == 0 && (v = value("--sh-clamp"))) cfg->sh_clamp = std::stoi(v);
    else if (std::strcmp(argv[i], "--vsync") == 0) cfg->vsync = true;
    else if (std::strcmp(argv[i], "--no-flip") == 0) cfg->flip_scene = false;
    else if (std::strcmp(argv[i], "--freed-port") == 0 && (v = value("--freed-port"))) cfg->freed_port = std::stoi(v);
    else if (std::strcmp(argv[i], "--latency-ms") == 0 && (v = value("--latency-ms"))) cfg->latency_ms = std::stof(v);
    else if (std::strcmp(argv[i], "--lens-file") == 0 && (v = value("--lens-file"))) cfg->lens_file = v;
    else if (std::strcmp(argv[i], "--sensor-height-mm") == 0 && (v = value("--sensor-height-mm"))) cfg->sensor_height_mm = std::stof(v);
    else if (std::strcmp(argv[i], "--ndi") == 0 && (v = value("--ndi"))) cfg->ndi_name = v;
    else if (std::strcmp(argv[i], "--fps") == 0 && (v = value("--fps"))) cfg->ndi_fps = std::stof(v);
    else if (std::strcmp(argv[i], "--stage-yaw-deg") == 0 && (v = value("--stage-yaw-deg"))) cfg->stage_yaw_deg = std::stof(v);
    else if (std::strcmp(argv[i], "--stage-offset") == 0 && (v = value("--stage-offset"))) {
      if (std::sscanf(v, "%f,%f,%f", &cfg->stage_offset_m[0], &cfg->stage_offset_m[1],
                      &cfg->stage_offset_m[2]) != 3) {
        std::fprintf(stderr, "error: --stage-offset needs X,Y,Z\n");
        return false;
      }
    } else {
      // Unknown flag, or a known flag whose value() already reported the miss.
      if (v == nullptr && argv[i][0] == '-') {
        std::fprintf(stderr, "error: unknown or incomplete option '%s'\n", argv[i]);
      }
      return false;
    }
  }
  if (cfg->asset.empty()) {
    std::fprintf(stderr, "error: no asset given (positional argument or config file)\n");
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  gsr::log::init();
  auto log = gsr::log::get("app");
  log->info("splatcast {} starting (CUDA: {})", gsr::core::version(),
            gsr::core::cuda_enabled() ? "enabled" : "disabled");

  gsr::app::RunConfig cfg;
  try {
    if (argc < 2 || !parse_cli(argc, argv, &cfg)) {
      print_usage();
      return 2;
    }

    log->info("loading asset {}", cfg.asset);
    const auto data = gsr::loader::load_ply(cfg.asset);
    log->info("loaded {} splats, SH degree {}", data.count, data.sh_degree);

#ifdef GSR_CUDA_ENABLED
    gsr::app::PreviewOptions options;
    options.width = cfg.width;
    options.height = cfg.height;
    options.fov_y_rad = glm::radians(cfg.fov_deg);
    options.sh_degree_clamp = cfg.sh_clamp;
    options.vsync = cfg.vsync;
    options.flip_scene = cfg.flip_scene;
    options.freed_port = cfg.freed_port;
    options.latency_ms = cfg.latency_ms;
    options.lens_csv = cfg.lens_file;
    options.sensor_height_mm = cfg.sensor_height_mm;
    options.overscan_fraction = cfg.overscan_pct / 100.0f;
    options.ndi_name = cfg.ndi_name;
    options.ndi_fps = cfg.ndi_fps;
    options.stage_yaw_rad = glm::radians(cfg.stage_yaw_deg);
    options.stage_offset = {cfg.stage_offset_m[0], cfg.stage_offset_m[1],
                            cfg.stage_offset_m[2]};
    const int code = gsr::app::run_preview(data, options);
    gsr::log::shutdown();
    return code;
#else
    log->error("this build has no CUDA toolchain — the renderer/preview is unavailable. "
               "Build on a CUDA machine to run it.");
    gsr::log::shutdown();
    return 1;
#endif
  } catch (const std::exception& e) {
    log->error("fatal: {}", e.what());
    gsr::log::shutdown();
    return 1;
  }
}
