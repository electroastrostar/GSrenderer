#include "output/ndi/ndi_sender.hpp"

#include "core/log.hpp"

#ifdef GSR_NDI_ENABLED
#include <Processing.NDI.Lib.h>
#endif

#include <cmath>
#include <stdexcept>

namespace gsr::output {

#ifndef GSR_NDI_ENABLED

bool ndi_runtime_available() { return false; }

struct NdiSender::Impl {};
NdiSender::NdiSender(const std::string&, int, int, double) {
  throw std::runtime_error(
      "this build has no NDI SDK — install the NDI 6 SDK (https://ndi.video) and "
      "reconfigure with NDI_SDK_DIR set");
}
NdiSender::~NdiSender() = default;
bool NdiSender::send_rgba(const std::uint8_t*, std::uint64_t) { return false; }

#else  // GSR_NDI_ENABLED

#ifdef GSR_NDI_IS_STUB
bool ndi_runtime_available() { return false; }  // compile-check stub: never claim to work
#else
bool ndi_runtime_available() { return true; }
#endif

struct NdiSender::Impl {
  NDIlib_send_instance_t sender = nullptr;
  int width = 0, height = 0;
  int rate_n = 30, rate_d = 1;
};

NdiSender::NdiSender(const std::string& source_name, int width, int height, double fps)
    : impl_(std::make_unique<Impl>()) {
  if (!NDIlib_initialize()) {
    throw std::runtime_error("NDI initialization failed (stub build or unsupported CPU)");
  }
  NDIlib_send_create_t create{};
  create.p_ndi_name = source_name.c_str();
  create.clock_video = false;  // our FramePacer owns the cadence (plan §6.6: NDI timing
                               // is free-running; don't fight it here)
  impl_->sender = NDIlib_send_create(&create);
  if (impl_->sender == nullptr) {
    throw std::runtime_error("NDIlib_send_create failed for source '" + source_name + "'");
  }
  impl_->width = width;
  impl_->height = height;
  // Represent common rates exactly (24/25/30 and NTSC variants).
  if (std::abs(fps - std::round(fps)) < 1e-6) {
    impl_->rate_n = static_cast<int>(std::lround(fps));
    impl_->rate_d = 1;
  } else {
    impl_->rate_n = static_cast<int>(std::lround(fps * 1001.0));
    impl_->rate_d = 1001;
  }
  gsr::log::get("ndi")->info("NDI source '{}' created: {}x{} @ {}/{}", source_name, width,
                             height, impl_->rate_n, impl_->rate_d);
}

NdiSender::~NdiSender() {
  if (impl_ != nullptr && impl_->sender != nullptr) {
    NDIlib_send_destroy(impl_->sender);
    NDIlib_destroy();
  }
}

bool NdiSender::send_rgba(const std::uint8_t* rgba, std::uint64_t timecode_us) {
  if (impl_->sender == nullptr) return false;
  NDIlib_video_frame_v2_t frame{};
  frame.xres = impl_->width;
  frame.yres = impl_->height;
  frame.FourCC = NDIlib_FourCC_type_RGBA;
  frame.frame_rate_N = impl_->rate_n;
  frame.frame_rate_D = impl_->rate_d;
  frame.frame_format_type = NDIlib_frame_format_type_progressive;
  frame.timecode = static_cast<std::int64_t>(timecode_us) * 10;  // us -> 100 ns units
  frame.p_data = const_cast<std::uint8_t*>(rgba);
  frame.line_stride_in_bytes = impl_->width * 4;
  NDIlib_send_send_video_v2(impl_->sender, &frame);  // synchronous: buffer reusable now
  return true;
}

#endif  // GSR_NDI_ENABLED

}  // namespace gsr::output
