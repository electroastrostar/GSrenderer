#include "output/frame_pacer.hpp"

#include <cmath>
#include <stdexcept>

namespace gsr::output {

FramePacer::FramePacer(double fps, std::uint64_t now_us) : anchor_us_(now_us) {
  if (!(fps > 0.0) || fps > 1000.0) {
    throw std::invalid_argument("FramePacer: fps must be in (0, 1000]");
  }
  interval_us_ = 1e6 / fps;
}

std::uint64_t FramePacer::next_deadline_us() const {
  return anchor_us_ +
         static_cast<std::uint64_t>(std::llround(static_cast<double>(slot_) * interval_us_));
}

void FramePacer::frame_done(std::uint64_t now_us) {
  ++total_;
  const std::uint64_t deadline = next_deadline_us();
  if (now_us > deadline) {
    ++late_;
    // Snap to the next future slot on the original grid (skip missed slots).
    const double elapsed = static_cast<double>(now_us - anchor_us_);
    slot_ = static_cast<std::uint64_t>(std::floor(elapsed / interval_us_)) + 1;
  } else {
    ++slot_;
  }
}

}  // namespace gsr::output
