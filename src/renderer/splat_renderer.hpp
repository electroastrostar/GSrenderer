#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "loader/splat_data.hpp"

// GPU splat rasterizer (Phase 2). Pipeline per plan §Phase 2: frustum cull → EWA 2D
// covariance projection → 16×16 tile binning → per-tile radix sort by view depth →
// front-to-back alpha blending. Implementation is CUDA (splat_renderer.cu); this header is
// CUDA-free so host code can include it unconditionally, but the target only exists when
// the toolchain has CUDA (GSR_CUDA_ENABLED).
namespace gsr::renderer {

struct Rgba8 {
  unsigned char r, g, b, a;
};

struct RenderConfig {
  int width = 1920;
  int height = 1080;
  float znear = 0.2f;                      // meters; splats closer than this are culled
  int sh_degree_clamp = -1;                // -1 = full asset degree; 0..3 = clamp (perf flag)
  float background[3] = {0.f, 0.f, 0.f};   // composited behind transparent regions
};

// Per-frame camera state (all in renderer conventions; see core/camera.hpp).
struct CameraFrame {
  glm::mat4 view_from_world{1.0f};
  float fx = 0.f, fy = 0.f;  // pixels
  float cx = 0.f, cy = 0.f;  // pixels from top-left
  glm::vec3 camera_position_world{0.0f};  // SH view origin (physical camera position)
};

struct FrameTimings {
  float preprocess_ms = 0.f;  // cull + project + SH
  float sort_ms = 0.f;        // key build + radix sort + range identify
  float blend_ms = 0.f;
  float total_ms = 0.f;
  unsigned int pairs_rendered = 0;  // splat-tile pairs after culling/binning
};

class SplatRenderer {
 public:
  // Uploads the asset to the GPU. Throws std::runtime_error on CUDA failure (load time).
  SplatRenderer(const loader::SplatData& data, const RenderConfig& config);
  ~SplatRenderer();
  SplatRenderer(const SplatRenderer&) = delete;
  SplatRenderer& operator=(const SplatRenderer&) = delete;

  // Renders one frame. Returns the DEVICE pointer to width*height RGBA8 pixels (row-major,
  // top-left origin), or nullptr on failure — hot path: errors are logged, never thrown.
  const Rgba8* render_device(const CameraFrame& camera, FrameTimings* timings = nullptr);

  // Copies the last rendered frame to host memory (width*height Rgba8). Debug/readback
  // convenience; the interop path hands the device pointer straight to GL. False on error.
  bool read_back(Rgba8* host_pixels) const;

  int width() const;
  int height() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace gsr::renderer
