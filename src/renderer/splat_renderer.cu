#include "renderer/splat_renderer.hpp"

#include "core/log.hpp"
#include "renderer/covariance.hpp"
#include "renderer/ewa_frame.hpp"
#include "renderer/sh.hpp"

#include <cub/cub.cuh>
#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace gsr::renderer {

namespace {

constexpr int kTileSize = 16;
constexpr int kBlockThreads = kTileSize * kTileSize;  // 256

// Load-time check: throws (allowed outside the frame loop).
void cuda_check_throw(cudaError_t err, const char* what) {
  if (err != cudaSuccess) {
    throw std::runtime_error(std::string("CUDA error in ") + what + ": " +
                             cudaGetErrorString(err));
  }
}

// Hot-path check: logs and returns false, never throws.
bool cuda_check_log(cudaError_t err, const char* what) {
  if (err != cudaSuccess) {
    gsr::log::get("renderer")->error("CUDA error in {}: {}", what, cudaGetErrorString(err));
    return false;
  }
  return true;
}

// Grow-only device buffer.
template <typename T>
struct DeviceBuffer {
  T* ptr = nullptr;
  size_t capacity = 0;  // elements

  bool reserve(size_t n) {
    if (n <= capacity) return true;
    if (ptr != nullptr) cudaFree(ptr);
    ptr = nullptr;
    capacity = 0;
    if (!cuda_check_log(cudaMalloc(&ptr, n * sizeof(T)), "cudaMalloc")) return false;
    capacity = n;
    return true;
  }
  void reserve_throw(size_t n) {
    if (!reserve(n)) throw std::runtime_error("device allocation failed");
  }
  void free() {
    if (ptr != nullptr) cudaFree(ptr);
    ptr = nullptr;
    capacity = 0;
  }
};

struct DeviceParams {
  float view[16];  // column-major view_from_world
  float w_rows[9];
  float cam_pos[3];
  float fx, fy, cx, cy;
  float tan_fovx, tan_fovy;
  float znear;
  int width, height;
  int grid_x, grid_y;
  int sh_degree;
  int rest_per_channel;
};

// ---------------------------------------------------------------- preprocess

__global__ void preprocess_kernel(int n, const float* __restrict__ position,
                                  const float* __restrict__ scale,
                                  const float* __restrict__ rotation,
                                  const float* __restrict__ opacity,
                                  const float* __restrict__ sh_dc,
                                  const float* __restrict__ sh_rest, DeviceParams p,
                                  float2* __restrict__ means2d, float* __restrict__ depths,
                                  float4* __restrict__ conic_opacity,
                                  float3* __restrict__ rgb, int* __restrict__ radii,
                                  unsigned int* __restrict__ tiles_touched) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  radii[i] = 0;
  tiles_touched[i] = 0;

  const float px = position[3 * i + 0];
  const float py = position[3 * i + 1];
  const float pz = position[3 * i + 2];

  // World -> view (column-major mat4), then the named y/z flip into the projection frame.
  const float vx = p.view[0] * px + p.view[4] * py + p.view[8] * pz + p.view[12];
  const float vy = p.view[1] * px + p.view[5] * py + p.view[9] * pz + p.view[13];
  const float vz = p.view[2] * px + p.view[6] * py + p.view[10] * pz + p.view[14];
  float t[3];
  projection_frame_from_view(vx, vy, vz, t);
  if (t[2] < p.znear) return;  // behind / too close

  const float inv_z = 1.0f / t[2];
  const float u = p.fx * t[0] * inv_z + p.cx;
  const float v = p.fy * t[1] * inv_z + p.cy;

  float cov6[6];
  covariance_3d(scale + 3 * i, rotation + 4 * i, cov6);
  float cov2d[3];
  project_covariance_ewa(cov6, t[0], t[1], t[2], p.fx, p.fy, p.tan_fovx, p.tan_fovy,
                         p.w_rows, cov2d);
  float conic[3];
  float radius = 0.0f;
  const float op = opacity[i];
  if (!conic_from_cov2d(cov2d, op, conic, &radius)) return;

  // Bounding tile rect from the alpha-cutoff radius, then the exact per-tile overlap
  // test. MUST match duplicate_keys_kernel exactly (same rect, same test) or the
  // prefix-sum offsets are corrupted.
  const int x_min = min(p.grid_x, max(0, static_cast<int>((u - radius) / kTileSize)));
  const int x_max =
      min(p.grid_x, max(0, static_cast<int>((u + radius + kTileSize - 1.0f) / kTileSize)));
  const int y_min = min(p.grid_y, max(0, static_cast<int>((v - radius) / kTileSize)));
  const int y_max =
      min(p.grid_y, max(0, static_cast<int>((v + radius + kTileSize - 1.0f) / kTileSize)));
  const float cutoff = power_cutoff(op);
  int touched = 0;
  for (int ty = y_min; ty < y_max; ++ty) {
    for (int tx = x_min; tx < x_max; ++tx) {
      if (tile_overlaps_splat(tx, ty, kTileSize, u, v, conic, cutoff)) ++touched;
    }
  }
  if (touched <= 0) return;

  // SH color from the physical-camera view direction (world space).
  float dx = px - p.cam_pos[0], dy = py - p.cam_pos[1], dz = pz - p.cam_pos[2];
  const float inv_len = rsqrtf(dx * dx + dy * dy + dz * dz);
  dx *= inv_len;
  dy *= inv_len;
  dz *= inv_len;
  float color[3];
  eval_sh_color(p.sh_degree, sh_dc + 3 * i, sh_rest + 3 * p.rest_per_channel * i, dx, dy, dz,
                color);

  means2d[i] = make_float2(u, v);
  depths[i] = t[2];
  conic_opacity[i] = make_float4(conic[0], conic[1], conic[2], op);
  rgb[i] = make_float3(color[0], color[1], color[2]);
  radii[i] = static_cast<int>(radius);
  tiles_touched[i] = static_cast<unsigned int>(touched);
}

// ------------------------------------------------------------------- binning

__global__ void duplicate_keys_kernel(int n, const float2* __restrict__ means2d,
                                      const float* __restrict__ depths,
                                      const int* __restrict__ radii,
                                      const float4* __restrict__ conic_opacity,
                                      const unsigned int* __restrict__ offsets, int grid_x,
                                      int grid_y, std::uint64_t* __restrict__ keys,
                                      unsigned int* __restrict__ values) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n || radii[i] == 0) return;

  const float u = means2d[i].x, v = means2d[i].y;
  const float radius = static_cast<float>(radii[i]);
  // Same rect + same overlap test as preprocess_kernel — the counts must match.
  const int x_min = min(grid_x, max(0, static_cast<int>((u - radius) / kTileSize)));
  const int x_max =
      min(grid_x, max(0, static_cast<int>((u + radius + kTileSize - 1.0f) / kTileSize)));
  const int y_min = min(grid_y, max(0, static_cast<int>((v - radius) / kTileSize)));
  const int y_max =
      min(grid_y, max(0, static_cast<int>((v + radius + kTileSize - 1.0f) / kTileSize)));

  const float4 con = conic_opacity[i];
  const float conic[3] = {con.x, con.y, con.z};
  const float cutoff = power_cutoff(con.w);

  unsigned int off = offsets[i];  // exclusive prefix sum of tiles_touched
  const unsigned int depth_bits = __float_as_uint(depths[i]);  // >0 so order-preserving
  for (int ty = y_min; ty < y_max; ++ty) {
    for (int tx = x_min; tx < x_max; ++tx) {
      if (!tile_overlaps_splat(tx, ty, kTileSize, u, v, conic, cutoff)) continue;
      const std::uint64_t tile = static_cast<std::uint64_t>(ty) * grid_x + tx;
      keys[off] = (tile << 32) | depth_bits;
      values[off] = static_cast<unsigned int>(i);
      ++off;
    }
  }
}

__global__ void tile_ranges_kernel(int num_pairs, const std::uint64_t* __restrict__ keys,
                                   uint2* __restrict__ ranges) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= num_pairs) return;
  const unsigned int tile = static_cast<unsigned int>(keys[i] >> 32);
  if (i == 0) {
    ranges[tile].x = 0;
  } else {
    const unsigned int prev = static_cast<unsigned int>(keys[i - 1] >> 32);
    if (tile != prev) {
      ranges[prev].y = i;
      ranges[tile].x = i;
    }
  }
  if (i == num_pairs - 1) ranges[tile].y = num_pairs;
}

// -------------------------------------------------------------------- blend

__global__ void blend_kernel(const uint2* __restrict__ ranges,
                             const unsigned int* __restrict__ point_list, int width,
                             int height, const float2* __restrict__ means2d,
                             const float4* __restrict__ conic_opacity,
                             const float3* __restrict__ rgb, float3 background,
                             Rgba8* __restrict__ out) {
  const int tile = blockIdx.y * gridDim.x + blockIdx.x;
  const int px = blockIdx.x * kTileSize + threadIdx.x;
  const int py = blockIdx.y * kTileSize + threadIdx.y;
  const bool inside = px < width && py < height;
  const float2 pixel = make_float2(static_cast<float>(px), static_cast<float>(py));
  const int tid = threadIdx.y * kTileSize + threadIdx.x;

  __shared__ float2 s_xy[kBlockThreads];
  __shared__ float4 s_con[kBlockThreads];
  __shared__ float3 s_rgb[kBlockThreads];

  const uint2 range = ranges[tile];
  bool done = !inside;
  float transmittance = 1.0f;
  float cr = 0.0f, cg = 0.0f, cb = 0.0f;

  for (unsigned int start = range.x; start < range.y; start += kBlockThreads) {
    if (__syncthreads_count(done) == kBlockThreads) break;

    const unsigned int fetch = start + tid;
    if (fetch < range.y) {
      const unsigned int splat = point_list[fetch];
      s_xy[tid] = means2d[splat];
      s_con[tid] = conic_opacity[splat];
      s_rgb[tid] = rgb[splat];
    }
    __syncthreads();

    const int batch = min(kBlockThreads, static_cast<int>(range.y - start));
    for (int k = 0; k < batch && !done; ++k) {
      const float dx = s_xy[k].x - pixel.x;
      const float dy = s_xy[k].y - pixel.y;
      const float4 con = s_con[k];
      const float power = -0.5f * (con.x * dx * dx + con.z * dy * dy) - con.y * dx * dy;
      if (power > 0.0f) continue;

      const float alpha = fminf(0.99f, con.w * __expf(power));
      if (alpha < 1.0f / 255.0f) continue;

      const float next_t = transmittance * (1.0f - alpha);
      if (next_t < 1e-4f) {
        done = true;
        continue;
      }
      const float weight = alpha * transmittance;
      cr += s_rgb[k].x * weight;
      cg += s_rgb[k].y * weight;
      cb += s_rgb[k].z * weight;
      transmittance = next_t;
    }
  }

  if (inside) {
    const float r = cr + transmittance * background.x;
    const float g = cg + transmittance * background.y;
    const float b = cb + transmittance * background.z;
    Rgba8 o;
    o.r = static_cast<unsigned char>(fminf(1.0f, fmaxf(0.0f, r)) * 255.0f);
    o.g = static_cast<unsigned char>(fminf(1.0f, fmaxf(0.0f, g)) * 255.0f);
    o.b = static_cast<unsigned char>(fminf(1.0f, fmaxf(0.0f, b)) * 255.0f);
    o.a = static_cast<unsigned char>(fminf(1.0f, 1.0f - transmittance) * 255.0f);
    out[py * width + px] = o;
  }
}

// Smallest end bit covering the tile id for the radix sort.
int sort_end_bit(int num_tiles) {
  int bit = 0;
  while ((1 << bit) < num_tiles) ++bit;
  return 32 + bit;
}

struct StageTimer {
  cudaEvent_t begin{}, end{};
  StageTimer() {
    cudaEventCreate(&begin);
    cudaEventCreate(&end);
  }
  ~StageTimer() {
    cudaEventDestroy(begin);
    cudaEventDestroy(end);
  }
  void start() { cudaEventRecord(begin); }
  float stop() {  // ms; synchronizes on the stop event
    cudaEventRecord(end);
    cudaEventSynchronize(end);
    float ms = 0.0f;
    cudaEventElapsedTime(&ms, begin, end);
    return ms;
  }
};

}  // namespace

// ------------------------------------------------------------------ Impl

struct SplatRenderer::Impl {
  RenderConfig config;
  int splat_count = 0;
  int sh_degree = 0;
  int rest_per_channel = 0;
  int grid_x = 0, grid_y = 0;

  // Static asset buffers.
  DeviceBuffer<float> position, scale, rotation, opacity, sh_dc, sh_rest;
  // Per-splat frame intermediates.
  DeviceBuffer<float2> means2d;
  DeviceBuffer<float> depths;
  DeviceBuffer<float4> conic_opacity;
  DeviceBuffer<float3> rgb;
  DeviceBuffer<int> radii;
  DeviceBuffer<unsigned int> tiles_touched, offsets;
  // Binning + sort (sized on demand per frame).
  DeviceBuffer<std::uint64_t> keys_in, keys_out;
  DeviceBuffer<unsigned int> vals_in, vals_out;
  DeviceBuffer<unsigned char> cub_temp;
  DeviceBuffer<uint2> ranges;
  DeviceBuffer<Rgba8> image;

  StageTimer timer_pre, timer_sort, timer_blend, timer_total;

  ~Impl() {
    position.free(); scale.free(); rotation.free(); opacity.free();
    sh_dc.free(); sh_rest.free();
    means2d.free(); depths.free(); conic_opacity.free(); rgb.free();
    radii.free(); tiles_touched.free(); offsets.free();
    keys_in.free(); keys_out.free(); vals_in.free(); vals_out.free();
    cub_temp.free(); ranges.free(); image.free();
  }
};

SplatRenderer::SplatRenderer(const loader::SplatData& data, const RenderConfig& config)
    : impl_(std::make_unique<Impl>()) {
  if (data.count == 0) throw std::runtime_error("SplatRenderer: empty asset");
  if (config.width <= 0 || config.height <= 0) {
    throw std::runtime_error("SplatRenderer: invalid output size");
  }
  auto log = gsr::log::get("renderer");

  Impl& im = *impl_;
  im.config = config;
  im.splat_count = static_cast<int>(data.count);
  im.sh_degree = config.sh_degree_clamp >= 0
                     ? std::min(config.sh_degree_clamp, data.sh_degree)
                     : data.sh_degree;
  im.rest_per_channel = loader::rest_coeffs_for_degree(data.sh_degree);  // asset stride
  im.grid_x = (config.width + kTileSize - 1) / kTileSize;
  im.grid_y = (config.height + kTileSize - 1) / kTileSize;

  const size_t n = data.count;
  auto upload = [](DeviceBuffer<float>& buf, const std::vector<float>& host,
                   const char* what) {
    buf.reserve_throw(host.size() > 0 ? host.size() : 1);
    if (!host.empty()) {
      cuda_check_throw(cudaMemcpy(buf.ptr, host.data(), host.size() * sizeof(float),
                                  cudaMemcpyHostToDevice),
                       what);
    }
  };
  upload(im.position, data.position, "upload position");
  upload(im.scale, data.scale, "upload scale");
  upload(im.rotation, data.rotation, "upload rotation");
  upload(im.opacity, data.opacity, "upload opacity");
  upload(im.sh_dc, data.sh_dc, "upload sh_dc");
  upload(im.sh_rest, data.sh_rest, "upload sh_rest");

  im.means2d.reserve_throw(n);
  im.depths.reserve_throw(n);
  im.conic_opacity.reserve_throw(n);
  im.rgb.reserve_throw(n);
  im.radii.reserve_throw(n);
  im.tiles_touched.reserve_throw(n);
  im.offsets.reserve_throw(n);
  im.ranges.reserve_throw(static_cast<size_t>(im.grid_x) * im.grid_y);
  im.image.reserve_throw(static_cast<size_t>(config.width) * config.height);

  log->info("SplatRenderer: {} splats uploaded ({} MiB), SH degree {} (asset {}), {}x{} "
            "({}x{} tiles)",
            n, gsr::loader::byte_size(data) / (1024 * 1024), im.sh_degree, data.sh_degree,
            config.width, config.height, im.grid_x, im.grid_y);
}

SplatRenderer::~SplatRenderer() = default;

int SplatRenderer::width() const { return impl_->config.width; }
int SplatRenderer::height() const { return impl_->config.height; }

const Rgba8* SplatRenderer::render_device(const CameraFrame& camera, FrameTimings* timings) {
  Impl& im = *impl_;
  const int n = im.splat_count;
  im.timer_total.start();

  DeviceParams p{};
  const float* view = &camera.view_from_world[0][0];
  for (int i = 0; i < 16; ++i) p.view[i] = view[i];
  ewa_rotation_from_view(camera.view_from_world, p.w_rows);
  p.cam_pos[0] = camera.camera_position_world.x;
  p.cam_pos[1] = camera.camera_position_world.y;
  p.cam_pos[2] = camera.camera_position_world.z;
  p.fx = camera.fx;
  p.fy = camera.fy;
  p.cx = camera.cx;
  p.cy = camera.cy;
  p.tan_fovx = 0.5f * static_cast<float>(im.config.width) / camera.fx;
  p.tan_fovy = 0.5f * static_cast<float>(im.config.height) / camera.fy;
  p.znear = im.config.znear;
  p.width = im.config.width;
  p.height = im.config.height;
  p.grid_x = im.grid_x;
  p.grid_y = im.grid_y;
  p.sh_degree = im.sh_degree;
  p.rest_per_channel = im.rest_per_channel;

  const int threads = 256;
  const int blocks = (n + threads - 1) / threads;

  // 1. Preprocess: cull + project + SH.
  im.timer_pre.start();
  preprocess_kernel<<<blocks, threads>>>(n, im.position.ptr, im.scale.ptr, im.rotation.ptr,
                                         im.opacity.ptr, im.sh_dc.ptr, im.sh_rest.ptr, p,
                                         im.means2d.ptr, im.depths.ptr, im.conic_opacity.ptr,
                                         im.rgb.ptr, im.radii.ptr, im.tiles_touched.ptr);
  if (!cuda_check_log(cudaGetLastError(), "preprocess_kernel")) return nullptr;
  const float pre_ms = im.timer_pre.stop();

  // 2. Exclusive scan of tiles_touched -> offsets, plus the total pair count.
  im.timer_sort.start();
  size_t scan_bytes = 0;
  cub::DeviceScan::ExclusiveSum(nullptr, scan_bytes, im.tiles_touched.ptr, im.offsets.ptr, n);
  if (!im.cub_temp.reserve(scan_bytes)) return nullptr;
  if (!cuda_check_log(cub::DeviceScan::ExclusiveSum(im.cub_temp.ptr, scan_bytes,
                                                    im.tiles_touched.ptr, im.offsets.ptr, n),
                      "DeviceScan::ExclusiveSum")) {
    return nullptr;
  }

  unsigned int last_offset = 0, last_touched = 0;
  if (!cuda_check_log(cudaMemcpy(&last_offset, im.offsets.ptr + (n - 1), sizeof(unsigned int),
                                 cudaMemcpyDeviceToHost),
                      "scan readback") ||
      !cuda_check_log(cudaMemcpy(&last_touched, im.tiles_touched.ptr + (n - 1),
                                 sizeof(unsigned int), cudaMemcpyDeviceToHost),
                      "scan readback")) {
    return nullptr;
  }
  const unsigned int num_pairs = last_offset + last_touched;

  const int num_tiles = im.grid_x * im.grid_y;
  if (!cuda_check_log(cudaMemset(im.ranges.ptr, 0, sizeof(uint2) * num_tiles),
                      "ranges memset")) {
    return nullptr;
  }

  if (num_pairs > 0) {
    // 3. Duplicate [tile|depth] keys per covered tile.
    if (!im.keys_in.reserve(num_pairs) || !im.keys_out.reserve(num_pairs) ||
        !im.vals_in.reserve(num_pairs) || !im.vals_out.reserve(num_pairs)) {
      return nullptr;
    }
    duplicate_keys_kernel<<<blocks, threads>>>(n, im.means2d.ptr, im.depths.ptr,
                                               im.radii.ptr, im.conic_opacity.ptr,
                                               im.offsets.ptr, im.grid_x, im.grid_y,
                                               im.keys_in.ptr, im.vals_in.ptr);
    if (!cuda_check_log(cudaGetLastError(), "duplicate_keys_kernel")) return nullptr;

    // 4. Radix sort pairs by [tile | depth].
    size_t sort_bytes = 0;
    cub::DeviceRadixSort::SortPairs(nullptr, sort_bytes, im.keys_in.ptr, im.keys_out.ptr,
                                    im.vals_in.ptr, im.vals_out.ptr, num_pairs, 0,
                                    sort_end_bit(num_tiles));
    if (!im.cub_temp.reserve(sort_bytes)) return nullptr;
    if (!cuda_check_log(cub::DeviceRadixSort::SortPairs(
                            im.cub_temp.ptr, sort_bytes, im.keys_in.ptr, im.keys_out.ptr,
                            im.vals_in.ptr, im.vals_out.ptr, num_pairs, 0,
                            sort_end_bit(num_tiles)),
                        "DeviceRadixSort::SortPairs")) {
      return nullptr;
    }

    // 5. Per-tile ranges in the sorted list.
    const int range_blocks = (static_cast<int>(num_pairs) + threads - 1) / threads;
    tile_ranges_kernel<<<range_blocks, threads>>>(static_cast<int>(num_pairs),
                                                  im.keys_out.ptr, im.ranges.ptr);
    if (!cuda_check_log(cudaGetLastError(), "tile_ranges_kernel")) return nullptr;
  }
  const float sort_ms = im.timer_sort.stop();

  // 6. Front-to-back blend, one 16x16 block per tile.
  im.timer_blend.start();
  const dim3 grid(im.grid_x, im.grid_y);
  const dim3 block(kTileSize, kTileSize);
  const float3 bg = make_float3(im.config.background[0], im.config.background[1],
                                im.config.background[2]);
  blend_kernel<<<grid, block>>>(im.ranges.ptr, im.vals_out.ptr, im.config.width,
                                im.config.height, im.means2d.ptr, im.conic_opacity.ptr,
                                im.rgb.ptr, bg, im.image.ptr);
  if (!cuda_check_log(cudaGetLastError(), "blend_kernel")) return nullptr;
  const float blend_ms = im.timer_blend.stop();
  const float total_ms = im.timer_total.stop();

  if (timings != nullptr) {
    timings->preprocess_ms = pre_ms;
    timings->sort_ms = sort_ms;
    timings->blend_ms = blend_ms;
    timings->total_ms = total_ms;
    timings->pairs_rendered = num_pairs;
  }
  return im.image.ptr;
}

bool SplatRenderer::read_back(Rgba8* host_pixels) const {
  const Impl& im = *impl_;
  const size_t bytes =
      static_cast<size_t>(im.config.width) * im.config.height * sizeof(Rgba8);
  return cuda_check_log(
      cudaMemcpy(host_pixels, im.image.ptr, bytes, cudaMemcpyDeviceToHost), "read_back");
}

}  // namespace gsr::renderer
