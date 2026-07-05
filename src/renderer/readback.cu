#include "renderer/readback.hpp"

#include "core/log.hpp"

#include <cuda_runtime.h>

#include <stdexcept>
#include <vector>

namespace gsr::renderer {

namespace {
bool ok(cudaError_t err, const char* what) {
  if (err != cudaSuccess) {
    gsr::log::get("readback")->error("CUDA error in {}: {}", what, cudaGetErrorString(err));
    return false;
  }
  return true;
}
}  // namespace

struct AsyncReadback::Impl {
  struct Slot {
    Rgba8* host = nullptr;  // pinned
    cudaEvent_t start{}, done{};
    bool in_flight = false;
  };

  std::size_t frame_bytes = 0;
  cudaStream_t stream{};
  std::vector<Slot> slots;
  std::size_t head = 0;  // next slot to fill
  std::size_t tail = 0;  // oldest in-flight slot
  std::size_t in_flight = 0;
  std::uint64_t dropped = 0;

  ~Impl() {
    for (auto& slot : slots) {
      if (slot.host != nullptr) cudaFreeHost(slot.host);
      if (slot.start != nullptr) cudaEventDestroy(slot.start);
      if (slot.done != nullptr) cudaEventDestroy(slot.done);
    }
    if (stream != nullptr) cudaStreamDestroy(stream);
  }
};

AsyncReadback::AsyncReadback(int width, int height, int slots)
    : impl_(std::make_unique<Impl>()) {
  if (width <= 0 || height <= 0 || slots < 2) {
    throw std::runtime_error("AsyncReadback: invalid size or slot count");
  }
  impl_->frame_bytes = static_cast<std::size_t>(width) * height * sizeof(Rgba8);
  if (cudaStreamCreateWithFlags(&impl_->stream, cudaStreamNonBlocking) != cudaSuccess) {
    throw std::runtime_error("AsyncReadback: stream creation failed");
  }
  impl_->slots.resize(static_cast<std::size_t>(slots));
  for (auto& slot : impl_->slots) {
    if (cudaHostAlloc(reinterpret_cast<void**>(&slot.host), impl_->frame_bytes,
                      cudaHostAllocDefault) != cudaSuccess ||
        cudaEventCreate(&slot.start) != cudaSuccess ||
        cudaEventCreate(&slot.done) != cudaSuccess) {
      throw std::runtime_error("AsyncReadback: pinned buffer/event allocation failed");
    }
  }
  gsr::log::get("readback")->info("async readback: {} pinned slots x {} KiB", slots,
                                  impl_->frame_bytes / 1024);
}

AsyncReadback::~AsyncReadback() = default;

bool AsyncReadback::begin(const Rgba8* device_pixels) {
  Impl& im = *impl_;
  if (im.in_flight == im.slots.size()) {
    ++im.dropped;
    return false;  // ring full: renderer outpacing the consumer; drop from readback
  }
  Impl::Slot& slot = im.slots[im.head];
  if (!ok(cudaEventRecord(slot.start, im.stream), "event start") ||
      !ok(cudaMemcpyAsync(slot.host, device_pixels, im.frame_bytes, cudaMemcpyDeviceToHost,
                          im.stream),
          "memcpy async") ||
      !ok(cudaEventRecord(slot.done, im.stream), "event done")) {
    return false;
  }
  slot.in_flight = true;
  im.head = (im.head + 1) % im.slots.size();
  ++im.in_flight;
  return true;
}

const Rgba8* AsyncReadback::acquire(float* readback_ms) {
  Impl& im = *impl_;
  if (im.in_flight == 0) return nullptr;
  Impl::Slot& slot = im.slots[im.tail];
  const cudaError_t query = cudaEventQuery(slot.done);
  if (query == cudaErrorNotReady) return nullptr;
  if (query != cudaSuccess) {
    ok(query, "event query");
    return nullptr;
  }
  if (readback_ms != nullptr) {
    float ms = 0.0f;
    if (ok(cudaEventElapsedTime(&ms, slot.start, slot.done), "event elapsed")) {
      *readback_ms = ms;
    }
  }
  slot.in_flight = false;
  im.tail = (im.tail + 1) % im.slots.size();
  --im.in_flight;
  return slot.host;
}

std::uint64_t AsyncReadback::frames_dropped() const { return impl_->dropped; }

}  // namespace gsr::renderer
