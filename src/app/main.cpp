#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include <glm/glm.hpp>

#include "core/log.hpp"
#include "core/version.hpp"
#include "loader/ply_loader.hpp"

#ifdef GSR_CUDA_ENABLED
#include "app/preview.hpp"
#endif

namespace {

void print_usage() {
  std::fprintf(stderr,
               "usage: splatcast <asset.ply> [options]\n"
               "  --width N       output width px   (default 1920)\n"
               "  --height N      output height px  (default 1080)\n"
               "  --fov DEG       vertical field of view in degrees (default 60)\n"
               "  --sh-clamp N    clamp SH evaluation degree 0..3 (default: asset degree)\n"
               "  --vsync         lock preview to the display refresh (default off)\n"
               "  --no-flip       don't apply the COLMAP y-down -> y-up scene flip\n"
               "  --freed-port N  listen for FreeD tracking on UDP port N (e.g. 8001)\n"
               "  --latency-ms F  tracking prediction offset in milliseconds\n"
               "  --lens-file P   zoom->focal CSV (see configs/example_lens.csv)\n"
               "  --sensor-height-mm F  sensor height for the lens table (default 24)\n"
               "Runs the Phase 2 debug preview (requires a CUDA build).\n");
}

struct CliOptions {
  std::string asset;
  int width = 1920;
  int height = 1080;
  float fov_deg = 60.0f;  // degrees on the CLI (I/O boundary); radians internally
  int sh_clamp = -1;
  bool vsync = false;
  bool flip_scene = true;
  int freed_port = -1;
  float latency_ms = 0.0f;
  std::string lens_file;
  float sensor_height_mm = 24.0f;
};

bool parse_cli(int argc, char** argv, CliOptions* out) {
  if (argc < 2) return false;
  out->asset = argv[1];
  for (int i = 2; i < argc; ++i) {
    const auto need_value = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: %s needs a value\n", flag);
        return nullptr;
      }
      return argv[++i];
    };
    if (std::strcmp(argv[i], "--width") == 0) {
      const char* v = need_value("--width");
      if (v == nullptr) return false;
      out->width = std::stoi(v);
    } else if (std::strcmp(argv[i], "--height") == 0) {
      const char* v = need_value("--height");
      if (v == nullptr) return false;
      out->height = std::stoi(v);
    } else if (std::strcmp(argv[i], "--fov") == 0) {
      const char* v = need_value("--fov");
      if (v == nullptr) return false;
      out->fov_deg = std::stof(v);
    } else if (std::strcmp(argv[i], "--sh-clamp") == 0) {
      const char* v = need_value("--sh-clamp");
      if (v == nullptr) return false;
      out->sh_clamp = std::stoi(v);
    } else if (std::strcmp(argv[i], "--vsync") == 0) {
      out->vsync = true;
    } else if (std::strcmp(argv[i], "--no-flip") == 0) {
      out->flip_scene = false;
    } else if (std::strcmp(argv[i], "--freed-port") == 0) {
      const char* v = need_value("--freed-port");
      if (v == nullptr) return false;
      out->freed_port = std::stoi(v);
    } else if (std::strcmp(argv[i], "--latency-ms") == 0) {
      const char* v = need_value("--latency-ms");
      if (v == nullptr) return false;
      out->latency_ms = std::stof(v);
    } else if (std::strcmp(argv[i], "--lens-file") == 0) {
      const char* v = need_value("--lens-file");
      if (v == nullptr) return false;
      out->lens_file = v;
    } else if (std::strcmp(argv[i], "--sensor-height-mm") == 0) {
      const char* v = need_value("--sensor-height-mm");
      if (v == nullptr) return false;
      out->sensor_height_mm = std::stof(v);
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  gsr::log::init();
  auto log = gsr::log::get("app");
  log->info("splatcast {} starting (CUDA: {})", gsr::core::version(),
            gsr::core::cuda_enabled() ? "enabled" : "disabled");

  CliOptions cli;
  if (!parse_cli(argc, argv, &cli)) {
    print_usage();
    return 2;
  }

  try {
    log->info("loading asset {}", cli.asset);
    const auto data = gsr::loader::load_ply(cli.asset);
    log->info("loaded {} splats, SH degree {}", data.count, data.sh_degree);

#ifdef GSR_CUDA_ENABLED
    gsr::app::PreviewOptions options;
    options.width = cli.width;
    options.height = cli.height;
    options.fov_y_rad = glm::radians(cli.fov_deg);
    options.sh_degree_clamp = cli.sh_clamp;
    options.vsync = cli.vsync;
    options.flip_scene = cli.flip_scene;
    options.freed_port = cli.freed_port;
    options.latency_ms = cli.latency_ms;
    options.lens_csv = cli.lens_file;
    options.sensor_height_mm = cli.sensor_height_mm;
    const int code = gsr::app::run_preview(data, options);
    gsr::log::shutdown();
    return code;
#else
    log->error("this build has no CUDA toolchain — the renderer/preview is unavailable. "
               "Build on a CUDA machine (A6000) to run Phase 2.");
    gsr::log::shutdown();
    return 1;
#endif
  } catch (const std::exception& e) {
    log->error("fatal: {}", e.what());
    gsr::log::shutdown();
    return 1;
  }
}
