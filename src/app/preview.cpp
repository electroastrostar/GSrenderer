#include "app/preview.hpp"

#include "core/camera.hpp"
#include "core/log.hpp"
#include "core/transforms.hpp"
#include "loader/splat_data.hpp"
#include "tracking/lens_table.hpp"
#include "tracking/pose_predictor.hpp"
#include "tracking/udp_listener.hpp"

#include <GLFW/glfw3.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

// Buffer-object entry points are post-GL1.1, so on Windows they must be resolved at
// runtime. We need exactly four; loading them via glfwGetProcAddress avoids a loader
// dependency (glad/glew).
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif

namespace gsr::app {

namespace {

using GlGenBuffersFn = void(APIENTRY*)(GLsizei, GLuint*);
using GlBindBufferFn = void(APIENTRY*)(GLenum, GLuint);
using GlBufferDataFn = void(APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
using GlDeleteBuffersFn = void(APIENTRY*)(GLsizei, const GLuint*);

struct GlBufferApi {
  GlGenBuffersFn gen = nullptr;
  GlBindBufferFn bind = nullptr;
  GlBufferDataFn data = nullptr;
  GlDeleteBuffersFn del = nullptr;

  void load() {
    gen = reinterpret_cast<GlGenBuffersFn>(glfwGetProcAddress("glGenBuffers"));
    bind = reinterpret_cast<GlBindBufferFn>(glfwGetProcAddress("glBindBuffer"));
    data = reinterpret_cast<GlBufferDataFn>(glfwGetProcAddress("glBufferData"));
    del = reinterpret_cast<GlDeleteBuffersFn>(glfwGetProcAddress("glDeleteBuffers"));
    if (!gen || !bind || !data || !del) {
      throw std::runtime_error("preview: OpenGL buffer-object functions unavailable");
    }
  }
};

bool cuda_ok(cudaError_t err, const char* what) {
  if (err != cudaSuccess) {
    gsr::log::get("preview")->error("CUDA error in {}: {}", what, cudaGetErrorString(err));
    return false;
  }
  return true;
}

struct FlyCamera {
  glm::vec3 position{0.0f};
  float yaw = 0.0f;    // radians about world +Y
  float pitch = 0.0f;  // radians about camera right
  float base_speed = 2.0f;   // scene-units/s, set from the scene's robust radius
  float speed_scale = 1.0f;  // scroll-wheel multiplier

  gsr::core::CameraPose pose() const {
    gsr::core::CameraPose p;
    p.position = position;
    p.orientation = glm::angleAxis(yaw, glm::vec3(0, 1, 0)) *
                    glm::angleAxis(pitch, glm::vec3(1, 0, 0));
    return p;
  }
};

// Start ~2.5 radii back from the scene core. ROBUST (percentile) bounds, not the raw
// bounding box: SfM scenes carry sky/background outlier splats hundreds of units out,
// and raw bounds would park the camera "infinitely far" with movement that feels dead
// (PR #3 operator report). Fly speed also derives from this radius: SfM scene scale is
// arbitrary, so a fixed m/s is meaningless.
FlyCamera initial_camera(const loader::SplatData& data, bool flip_scene) {
  const auto b = gsr::loader::compute_robust_bounds(data);
  const glm::vec3 center_asset{(b.min[0] + b.max[0]) * 0.5f, (b.min[1] + b.max[1]) * 0.5f,
                               (b.min[2] + b.max[2]) * 0.5f};
  const glm::vec3 center =
      flip_scene ? gsr::core::world_from_asset(center_asset) : center_asset;
  const glm::vec3 extent{b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2]};
  const float radius = 0.5f * std::max(0.5f, glm::length(extent));
  FlyCamera cam;
  cam.position = center + glm::vec3(0.0f, 0.0f, 2.5f * radius);
  cam.base_speed = 0.6f * radius;  // ~4 s to cross the scene core at neutral scroll
  return cam;
}

// Scroll wheel scales fly speed (1.15x per detent), clamped to a generous range.
void scroll_callback(GLFWwindow* window, double /*dx*/, double dy) {
  auto* cam = static_cast<FlyCamera*>(glfwGetWindowUserPointer(window));
  if (cam == nullptr) return;
  cam->speed_scale *= std::pow(1.15f, static_cast<float>(dy));
  cam->speed_scale = std::min(1000.0f, std::max(0.001f, cam->speed_scale));
}

void handle_input(GLFWwindow* window, FlyCamera& cam, float dt, double* last_x,
                  double* last_y) {
  float speed = cam.base_speed * cam.speed_scale * dt;
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 5.0f;

  const auto pose = cam.pose();
  const glm::vec3 fwd = gsr::core::forward_world(pose);
  const glm::vec3 right = gsr::core::right_world(pose);
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.position += fwd * speed;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.position -= fwd * speed;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.position += right * speed;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.position -= right * speed;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) cam.position.y += speed;
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cam.position.y -= speed;

  double mx = 0.0, my = 0.0;
  glfwGetCursorPos(window, &mx, &my);
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
    constexpr float kSensitivity = 0.0035f;
    cam.yaw -= static_cast<float>(mx - *last_x) * kSensitivity;
    cam.pitch -= static_cast<float>(my - *last_y) * kSensitivity;
    const float limit = glm::half_pi<float>() - 0.01f;
    cam.pitch = cam.pitch > limit ? limit : (cam.pitch < -limit ? -limit : cam.pitch);
  }
  *last_x = mx;
  *last_y = my;
}

}  // namespace

int run_preview(const loader::SplatData& data, const PreviewOptions& options) {
  auto log = gsr::log::get("preview");

  // Mode B overscan expands the render target; the base (lens) image is the exact
  // center crop of what we render/output.
  const float fov = options.fov_y_rad > 0.0f ? options.fov_y_rad : glm::radians(60.0f);
  const auto base_intr = gsr::core::with_overscan(
      gsr::core::intrinsics_from_fov(fov, options.width, options.height, 0.1f, 1000.0f),
      options.overscan_fraction);
  const int out_width = base_intr.width;
  const int out_height = base_intr.height;

  gsr::renderer::RenderConfig config;
  config.width = out_width;
  config.height = out_height;
  config.sh_degree_clamp = options.sh_degree_clamp;
  gsr::renderer::SplatRenderer renderer(data, config);
  if (options.overscan_fraction > 0.0f) {
    log->info("overscan {:.1f}%: rendering {}x{} (base {}x{} is the center crop)",
              options.overscan_fraction * 100.0f, out_width, out_height, options.width,
              options.height);
  }

  // Tracked-camera mode: listener feeds the predictor; the render loop queries
  // predict(now + latency) and converts through render_from_freed.
  std::unique_ptr<gsr::tracking::PosePredictor> predictor;
  std::unique_ptr<gsr::tracking::FreedListener> listener;
  std::unique_ptr<gsr::tracking::LensTable> lens;
  if (options.freed_port >= 0) {
    predictor = std::make_unique<gsr::tracking::PosePredictor>();
    // Horizon must cover the intended latency offset plus packet jitter, or the offset
    // silently caps out at the anti-dropout default (PR #4).
    predictor->set_max_extrapolation_us(
        static_cast<std::uint64_t>(options.latency_ms > 0 ? options.latency_ms : 0) *
            1000 +
        100'000);
    listener = std::make_unique<gsr::tracking::FreedListener>(
        static_cast<std::uint16_t>(options.freed_port),
        [p = predictor.get()](const gsr::tracking::TimedPose& pose) { p->push(pose); });
    if (!options.lens_csv.empty()) {
      lens = std::make_unique<gsr::tracking::LensTable>(
          gsr::tracking::LensTable::from_csv(options.lens_csv));
      log->info("lens table {} loaded; sensor height {:.1f} mm", options.lens_csv,
                options.sensor_height_mm);
    }
    log->info("tracked-camera mode: FreeD on UDP {} (latency offset {:.1f} ms{})",
              listener->port(), options.latency_ms,
              lens ? ", lens table" : ", fixed intrinsics");
  }

  if (glfwInit() != GLFW_TRUE) throw std::runtime_error("preview: glfwInit failed");
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // fixed-size render target this phase
  GLFWwindow* window =
      glfwCreateWindow(out_width, out_height, "splatcast", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    throw std::runtime_error("preview: window creation failed (GL context)");
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(options.vsync ? 1 : 0);

  GlBufferApi gl;
  gl.load();

  // Texture + PBO + CUDA registration (the interop triangle).
  const size_t frame_bytes =
      static_cast<size_t>(out_width) * out_height * sizeof(gsr::renderer::Rgba8);
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out_width, out_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  GLuint pbo = 0;
  gl.gen(1, &pbo);
  gl.bind(GL_PIXEL_UNPACK_BUFFER, pbo);
  gl.data(GL_PIXEL_UNPACK_BUFFER, static_cast<std::ptrdiff_t>(frame_bytes), nullptr,
          GL_STREAM_DRAW);
  gl.bind(GL_PIXEL_UNPACK_BUFFER, 0);

  cudaGraphicsResource* pbo_resource = nullptr;
  if (!cuda_ok(cudaGraphicsGLRegisterBuffer(&pbo_resource, pbo,
                                            cudaGraphicsRegisterFlagsWriteDiscard),
               "cudaGraphicsGLRegisterBuffer")) {
    glfwDestroyWindow(window);
    glfwTerminate();
    throw std::runtime_error("preview: CUDA/GL interop registration failed");
  }

  // COLMAP-convention assets are y-down; the fly camera lives in y-up render world and
  // the renderer receives the composed asset-space view (named transforms, CLAUDE.md).
  const bool flip_scene = options.flip_scene;
  FlyCamera cam = initial_camera(data, flip_scene);
  glfwSetWindowUserPointer(window, &cam);
  glfwSetScrollCallback(window, scroll_callback);
  double last_x = 0.0, last_y = 0.0;
  glfwGetCursorPos(window, &last_x, &last_y);

  log->info("preview: {}x{}, fov {:.1f} deg, vsync {}", out_width, out_height,
            glm::degrees(fov), options.vsync ? "on" : "off");
  log->info("controls: WASD move, Q/E down/up, right-drag look, SCROLL speed, Shift fast, "
            "Esc quit (fly speed {:.2f} units/s)",
            cam.base_speed);

  bool was_tracked = false;
  gsr::core::CameraPose last_tracked_pose;
  // Live-adjustable prediction offset ([ / ] keys) + measured lead for the HUD: the
  // pan difference between the predicted pose and the newest raw packet, in degrees.
  // Live adjustment makes the latency acceptance test self-referenced (the camera
  // visibly jumps along the orbit on each step) instead of relying on operator memory
  // across restarts (PR #4 feedback). Also previews the Phase 7 set-latency control.
  float latency_ms = options.latency_ms;
  float lead_deg = 0.0f;
  bool bracket_left_was = false, bracket_right_was = false;
  double prev_time = glfwGetTime();
  double hud_time = prev_time;
  double log_time = prev_time;
  int hud_frames = 0;
  gsr::renderer::FrameTimings acc{};
  int exit_code = 0;

  while (glfwWindowShouldClose(window) == 0) {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (predictor) {  // [ / ] step the prediction offset by 50 ms (edge-triggered)
      const bool lb = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
      const bool rb = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
      const float before = latency_ms;
      if (rb && !bracket_right_was) latency_ms = std::min(2000.0f, latency_ms + 50.0f);
      if (lb && !bracket_left_was) latency_ms = std::max(0.0f, latency_ms - 50.0f);
      if (latency_ms != before) {
        predictor->set_max_extrapolation_us(
            static_cast<std::uint64_t>(latency_ms) * 1000 + 100'000);
        log->info("prediction latency offset -> {:.0f} ms", latency_ms);
      }
      bracket_left_was = lb;
      bracket_right_was = rb;
    }

    const double now = glfwGetTime();
    const float dt = static_cast<float>(now - prev_time);
    prev_time = now;

    // Tracker drives the camera while packets are FRESH; on dropout (simulator
    // stopped, cable pulled) control hands back to the fly camera in place after 0.5 s
    // instead of freezing forever (PR #4 operator report).
    gsr::core::CameraPose pose;
    gsr::core::Intrinsics intr = base_intr;
    bool tracked = false;
    if (predictor) {
      constexpr std::uint64_t kTrackingStaleUs = 500'000;
      const std::uint64_t now_us = gsr::log::mono_us();
      const auto newest = listener->latest();
      const bool fresh =
          newest.has_value() && now_us - newest->t_mono_us < kTrackingStaleUs;
      const auto latency_us =
          static_cast<std::int64_t>(static_cast<double>(latency_ms) * 1000.0);
      if (fresh) {
        if (const auto predicted = predictor->predict(
                now_us + static_cast<std::uint64_t>(latency_us > 0 ? latency_us : 0))) {
          pose = gsr::core::world_from_stage(
            gsr::core::render_from_freed(predicted->pan_rad, predicted->tilt_rad,
                                         predicted->roll_rad, predicted->x_m,
                                         predicted->y_m, predicted->z_m),
            options.stage_yaw_rad, options.stage_offset);
          if (lens) {
            const float fy = gsr::tracking::focal_px_from_mm(
                lens->focal_mm(predicted->zoom_raw), options.sensor_height_mm,
                options.height);
            intr.fx = fy;  // square pixels
            intr.fy = fy;
          }
          tracked = true;
          last_tracked_pose = pose;
          // Measured prediction lead vs the newest raw packet (wrapped pan delta).
          float delta = predicted->pan_rad - newest->pose.pan_rad;
          while (delta >= glm::pi<float>()) delta -= 2.0f * glm::pi<float>();
          while (delta < -glm::pi<float>()) delta += 2.0f * glm::pi<float>();
          lead_deg = glm::degrees(delta);
        }
      }
    }
    if (!tracked) {
      if (was_tracked) {
        // Sync the fly camera to where tracking left off so control resumes in place.
        cam.position = last_tracked_pose.position;
        const glm::vec3 fwd = gsr::core::forward_world(last_tracked_pose);
        cam.pitch = std::asin(fwd.y < -1.0f ? -1.0f : (fwd.y > 1.0f ? 1.0f : fwd.y));
        cam.yaw = std::atan2(-fwd.x, -fwd.z);
        gsr::log::get("preview")->info("tracking stale — fly camera resumes at the last "
                                       "tracked pose");
      }
      handle_input(window, cam, dt, &last_x, &last_y);
      pose = cam.pose();
    }
    was_tracked = tracked;

    gsr::renderer::CameraFrame frame;
    // The renderer works in the splats' own space: compose the world->view transform
    // with world_from_asset, and hand it the camera position in asset space (SH origin).
    frame.view_from_world =
        flip_scene ? gsr::core::view_from_world(pose) * gsr::core::world_from_asset_rotation()
                   : gsr::core::view_from_world(pose);
    frame.fx = intr.fx;
    frame.fy = intr.fy;
    frame.cx = intr.cx;
    frame.cy = intr.cy;
    frame.camera_position_world =
        flip_scene ? gsr::core::asset_from_world(pose.position) : pose.position;

    gsr::renderer::FrameTimings timings;
    const gsr::renderer::Rgba8* device_pixels = renderer.render_device(frame, &timings);
    if (device_pixels == nullptr) {
      log->error("render failed; closing preview");
      exit_code = 1;
      break;
    }

    // Device-to-device into the mapped PBO, then PBO -> texture on the GL side.
    void* mapped = nullptr;
    size_t mapped_size = 0;
    if (!cuda_ok(cudaGraphicsMapResources(1, &pbo_resource), "map PBO") ||
        !cuda_ok(cudaGraphicsResourceGetMappedPointer(&mapped, &mapped_size, pbo_resource),
                 "mapped pointer") ||
        !cuda_ok(cudaMemcpy(mapped, device_pixels, frame_bytes, cudaMemcpyDeviceToDevice),
                 "copy to PBO") ||
        !cuda_ok(cudaGraphicsUnmapResources(1, &pbo_resource), "unmap PBO")) {
      exit_code = 1;
      break;
    }

    gl.bind(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, out_width, out_height, GL_RGBA,
                    GL_UNSIGNED_BYTE, nullptr);
    gl.bind(GL_PIXEL_UNPACK_BUFFER, 0);

    // Fullscreen textured quad (fixed-function is plenty for a debug view). The renderer
    // writes top-left origin; GL textures sample bottom-left, so flip V.
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0.f, 1.f); glVertex2f(-1.f, -1.f);
    glTexCoord2f(1.f, 1.f); glVertex2f(1.f, -1.f);
    glTexCoord2f(1.f, 0.f); glVertex2f(1.f, 1.f);
    glTexCoord2f(0.f, 0.f); glVertex2f(-1.f, 1.f);
    glEnd();
    glfwSwapBuffers(window);

    gsr::log::advance_frame();
    ++hud_frames;
    acc.preprocess_ms += timings.preprocess_ms;
    acc.sort_ms += timings.sort_ms;
    acc.blend_ms += timings.blend_ms;
    acc.total_ms += timings.total_ms;
    acc.pairs_rendered = timings.pairs_rendered;

    // HUD in the title 4x/s; frame-stamped log line 1x/s (plan Phase 2 task 4).
    if (now - hud_time > 0.25 && hud_frames > 0) {
      const float inv = 1.0f / static_cast<float>(hud_frames);
      char tracking_hud[128] = "";
      if (listener) {
        const auto ts = listener->stats();
        std::snprintf(tracking_hud, sizeof tracking_hud,
                      " | trk %.0f Hz ok:%llu rej:%llu | lat %.0fms lead %+.1fdeg",
                      ts.packet_rate_hz, static_cast<unsigned long long>(ts.packets_ok),
                      static_cast<unsigned long long>(ts.packets_rejected), latency_ms,
                      lead_deg);
      }
      char title[352];
      std::snprintf(title, sizeof title,
                    "splatcast — %.1f fps | cull+proj %.2f ms | sort %.2f ms | blend %.2f "
                    "ms | GPU total %.2f ms | %u pairs | spd %.2f%s",
                    hud_frames / static_cast<float>(now - hud_time), acc.preprocess_ms * inv,
                    acc.sort_ms * inv, acc.blend_ms * inv, acc.total_ms * inv,
                    acc.pairs_rendered, cam.base_speed * cam.speed_scale, tracking_hud);
      glfwSetWindowTitle(window, title);
      if (now - log_time > 1.0) {
        log->info("{:.1f} fps | preprocess {:.2f} ms | sort {:.2f} ms | blend {:.2f} ms | "
                  "gpu {:.2f} ms | {} pairs{}",
                  hud_frames / static_cast<float>(now - hud_time), acc.preprocess_ms * inv,
                  acc.sort_ms * inv, acc.blend_ms * inv, acc.total_ms * inv,
                  acc.pairs_rendered, tracking_hud);
        log_time = now;
      }
      hud_time = now;
      hud_frames = 0;
      acc = {};
    }
  }

  cudaGraphicsUnregisterResource(pbo_resource);
  gl.del(1, &pbo);
  glDeleteTextures(1, &texture);
  glfwDestroyWindow(window);
  glfwTerminate();
  return exit_code;
}

}  // namespace gsr::app
