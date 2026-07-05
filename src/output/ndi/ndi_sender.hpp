#pragma once

#include <cstdint>
#include <memory>
#include <string>

// NDI sender (plan Phase 5 task 1). The header is SDK-free; the implementation compiles
// against the NDI SDK when CMake finds it (NDI_SDK_DIR or the default install path), or
// against the compile-check stub with -DGSR_NDI_STUB=ON. Runtime honesty: without the
// real SDK the constructor throws — the app can never silently "stream" nothing.
namespace gsr::output {

// True when this binary was built against the real NDI SDK (not the stub).
bool ndi_runtime_available();

class NdiSender {
 public:
  // Creates the named NDI source. Throws std::runtime_error when built without the real
  // SDK or when NDI initialization fails (startup time).
  NdiSender(const std::string& source_name, int width, int height, double fps);
  ~NdiSender();
  NdiSender(const NdiSender&) = delete;
  NdiSender& operator=(const NdiSender&) = delete;

  // Sends one RGBA8 frame (tightly packed, width*height*4 bytes). Synchronous NDI send:
  // the buffer is reusable as soon as this returns. timecode_us is the monotonic frame
  // stamp (converted to NDI's 100 ns units). Returns false on failure (logged, hot path).
  bool send_rgba(const std::uint8_t* rgba, std::uint64_t timecode_us);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace gsr::output
