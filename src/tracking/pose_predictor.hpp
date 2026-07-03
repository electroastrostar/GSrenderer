#pragma once

#include "tracking/udp_listener.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

// Pose filter/predictor (plan Phase 3 Task 4): timestamped ring buffer fed by the
// listener thread, linear-velocity extrapolation queried by the render thread. The
// latency offset lives at the CALL SITE: the app asks for
//   predict(mono_us() + latency_offset_ms * 1000)
// so a config change shifts the query time, not this class's state.
namespace gsr::tracking {

class PosePredictor {
 public:
  // capacity: retained history (only the 2 newest drive linear prediction; the rest is
  // headroom for future smoothing filters). max_extrapolation_us: beyond this horizon
  // past the newest sample, prediction freezes at the newest pose instead of flying
  // off on stale data (tracker dropouts).
  explicit PosePredictor(std::size_t capacity = 256,
                         std::uint64_t max_extrapolation_us = 200'000);

  void push(const TimedPose& sample);

  // Pose at monotonic time t_query_us. Angles are unwrapped before differencing so a
  // pan crossing ±180° extrapolates smoothly. zoom/focus pass through from the newest
  // sample (raw encoder counts don't extrapolate meaningfully). nullopt until the
  // first sample; a single sample predicts as itself.
  std::optional<FreedPose> predict(std::uint64_t t_query_us) const;

  std::size_t size() const;

 private:
  mutable std::mutex mutex_;
  std::vector<TimedPose> ring_;
  std::size_t head_ = 0;   // next write slot
  std::size_t count_ = 0;
  std::uint64_t max_extrapolation_us_;
};

}  // namespace gsr::tracking
