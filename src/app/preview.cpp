#include "app/preview.hpp"

#include "core/camera.hpp"
#include "core/log.hpp"
#include "loader/splat_data.hpp"

#include <GLFW/glfw3.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
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

  gsr::core::CameraPose pose() const {
    gsr::core::CameraPose p;
    p.position = position;
    p.orientation = glm::angleAxis(yaw, glm::vec3(0, 1, 0)) *
                    glm::angleAxis(pitch, glm::vec3(1, 0, 0));
    return p;
  }
};

// Start ~2.5 bounding-radii back from the asset center so the scene is in frame.
FlyCamera initial_camera(const loader::SplatData& data) {
  const auto b = gsr::loader::compute_bounds(data);
  const glm::vec3 center{(b.min[0] + b.max[0]) * 0.5f, (b.min[1] + b.max[1]) * 0.5f,
                         (b.min[2] + b.max[2]) * 0.5f};
  const glm::vec3 extent{b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2]};
  const float radius = 0.5f * std::max(0.5f, glm::length(extent));
  FlyCamera cam;
  cam.position = center + glm::vec3(0.0f, 0.0f, 2.5f * radius);
  return cam;
}

void handle_input(GLFWwindow* window, FlyCamera& cam, float dt, double* last_x,
                  double* last_y) {
  float speed = 2.0f * dt;  // m/s baseline
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

  gsr::renderer::RenderConfig config;
  config.width = options.width;
  config.height = options.height;
  config.sh_degree_clamp = options.sh_degree_clamp;
  gsr::renderer::SplatRenderer renderer(data, config);

  const float fov = options.fov_y_rad > 0.0f ? options.fov_y_rad : glm::radians(60.0f);
  const auto intr =
      gsr::core::intrinsics_from_fov(fov, options.width, options.height, 0.1f, 1000.0f);

  if (glfwInit() != GLFW_TRUE) throw std::runtime_error("preview: glfwInit failed");
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // fixed-size render target this phase
  GLFWwindow* window =
      glfwCreateWindow(options.width, options.height, "splatcast", nullptr, nullptr);
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
      static_cast<size_t>(options.width) * options.height * sizeof(gsr::renderer::Rgba8);
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, options.width, options.height, 0, GL_RGBA,
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

  FlyCamera cam = initial_camera(data);
  double last_x = 0.0, last_y = 0.0;
  glfwGetCursorPos(window, &last_x, &last_y);

  log->info("preview: {}x{}, fov {:.1f} deg, vsync {}", options.width, options.height,
            glm::degrees(fov), options.vsync ? "on" : "off");
  log->info("controls: WASD move, Q/E down/up, right-drag look, Shift fast, Esc quit");

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

    const double now = glfwGetTime();
    const float dt = static_cast<float>(now - prev_time);
    prev_time = now;
    handle_input(window, cam, dt, &last_x, &last_y);

    const auto pose = cam.pose();
    gsr::renderer::CameraFrame frame;
    frame.view_from_world = gsr::core::view_from_world(pose);
    frame.fx = intr.fx;
    frame.fy = intr.fy;
    frame.cx = intr.cx;
    frame.cy = intr.cy;
    frame.camera_position_world = pose.position;

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
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, options.width, options.height, GL_RGBA,
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
      char title[256];
      std::snprintf(title, sizeof title,
                    "splatcast — %.1f fps | cull+proj %.2f ms | sort %.2f ms | blend %.2f "
                    "ms | GPU total %.2f ms | %u pairs",
                    hud_frames / static_cast<float>(now - hud_time), acc.preprocess_ms * inv,
                    acc.sort_ms * inv, acc.blend_ms * inv, acc.total_ms * inv,
                    acc.pairs_rendered);
      glfwSetWindowTitle(window, title);
      if (now - log_time > 1.0) {
        log->info("{:.1f} fps | preprocess {:.2f} ms | sort {:.2f} ms | blend {:.2f} ms | "
                  "gpu {:.2f} ms | {} pairs",
                  hud_frames / static_cast<float>(now - hud_time), acc.preprocess_ms * inv,
                  acc.sort_ms * inv, acc.blend_ms * inv, acc.total_ms * inv,
                  acc.pairs_rendered);
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
