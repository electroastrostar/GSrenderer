#include "tracking/pose_predictor.hpp"

#include <cmath>

namespace gsr::tracking {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Wraps to [-pi, pi).
float wrap_angle(float rad) {
  rad = std::fmod(rad + kPi, 2.0f * kPi);
  if (rad < 0.0f) rad += 2.0f * kPi;
  return rad - kPi;
}

// Linear extrapolation of an angle: the sample-to-sample delta is wrapped to the short
// way around before scaling, so ±180° crossings don't snap.
float extrapolate_angle(float a0, float a1, float scale) {
  const float delta = wrap_angle(a1 - a0);
  return wrap_angle(a1 + delta * scale);
}

float extrapolate_linear(float v0, float v1, float scale) { return v1 + (v1 - v0) * scale; }

}  // namespace

PosePredictor::PosePredictor(std::size_t capacity, std::uint64_t max_extrapolation_us)
    : ring_(capacity > 2 ? capacity : 2), max_extrapolation_us_(max_extrapolation_us) {}

void PosePredictor::push(const TimedPose& sample) {
  std::lock_guard<std::mutex> lock(mutex_);
  ring_[head_] = sample;
  head_ = (head_ + 1) % ring_.size();
  if (count_ < ring_.size()) ++count_;
}

std::optional<FreedPose> PosePredictor::predict(std::uint64_t t_query_us) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (count_ == 0) return std::nullopt;

  const std::size_t newest = (head_ + ring_.size() - 1) % ring_.size();
  const TimedPose& s1 = ring_[newest];
  if (count_ == 1 || t_query_us <= s1.t_mono_us) {
    return s1.pose;  // no basis (or no need) to extrapolate
  }

  const std::size_t previous = (head_ + ring_.size() - 2) % ring_.size();
  const TimedPose& s0 = ring_[previous];
  if (s1.t_mono_us <= s0.t_mono_us) return s1.pose;  // duplicate/reordered timestamps

  std::uint64_t horizon = t_query_us - s1.t_mono_us;
  if (horizon > max_extrapolation_us_) horizon = max_extrapolation_us_;  // stale: freeze
  const float scale = static_cast<float>(horizon) /
                      static_cast<float>(s1.t_mono_us - s0.t_mono_us);

  FreedPose out = s1.pose;
  out.pan_rad = extrapolate_angle(s0.pose.pan_rad, s1.pose.pan_rad, scale);
  out.tilt_rad = extrapolate_angle(s0.pose.tilt_rad, s1.pose.tilt_rad, scale);
  out.roll_rad = extrapolate_angle(s0.pose.roll_rad, s1.pose.roll_rad, scale);
  out.x_m = extrapolate_linear(s0.pose.x_m, s1.pose.x_m, scale);
  out.y_m = extrapolate_linear(s0.pose.y_m, s1.pose.y_m, scale);
  out.z_m = extrapolate_linear(s0.pose.z_m, s1.pose.z_m, scale);
  return out;
}

std::size_t PosePredictor::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return count_;
}

}  // namespace gsr::tracking
