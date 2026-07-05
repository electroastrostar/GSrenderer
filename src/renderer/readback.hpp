#pragma once

#include "renderer/splat_renderer.hpp"

#include <memory>

// Async device->host readback ring (plan Phase 5 task 2): pinned staging buffers +
// cudaMemcpyAsync on a dedicated stream so readback never stalls the render loop. The
// consumer (NDI send) runs 1-2 frames behind the renderer — that pipeline depth is the
// price of not blocking, and it's measured: acquire() reports the event-timed copy cost.
// CUDA-only target (compiled into gsr_renderer).
namespace gsr::renderer {

class AsyncReadback {
 public:
  // slots >= 2 (3 = classic triple buffer). Throws std::runtime_error on CUDA failure
  // (startup time).
  AsyncReadback(int width, int height, int slots = 3);
  ~AsyncReadback();
  AsyncReadback(const AsyncReadback&) = delete;
  AsyncReadback& operator=(const AsyncReadback&) = delete;

  // Enqueues an async copy of a width*height RGBA8 device frame. Returns false —
  // logged, frame simply not read back — when every slot is still in flight or on CUDA
  // error (hot path: no exceptions).
  bool begin(const Rgba8* device_pixels);

  // Returns the oldest COMPLETED frame's host pointer (valid until that slot is reused
  // — with the recommended slots>=3 and send-before-next-begin usage this is safe), or
  // nullptr if nothing has finished yet. readback_ms (optional) gets the copy's
  // event-measured duration.
  const Rgba8* acquire(float* readback_ms = nullptr);

  std::uint64_t frames_dropped() const;  // begin() calls rejected with the ring full

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace gsr::renderer
