#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "tracking/pose_predictor.hpp"

using Catch::Approx;
namespace trk = gsr::tracking;

namespace {
constexpr float kPi = 3.14159265358979323846f;

trk::TimedPose sample(std::uint64_t t_us, float x, float pan_rad = 0.0f) {
  trk::TimedPose s;
  s.t_mono_us = t_us;
  s.pose.x_m = x;
  s.pose.pan_rad = pan_rad;
  s.pose.zoom_raw = 42;
  return s;
}
}  // namespace

TEST_CASE("predict is empty until a sample arrives; one sample predicts itself",
          "[tracking][predict]") {
  trk::PosePredictor predictor;
  CHECK_FALSE(predictor.predict(1'000'000).has_value());

  predictor.push(sample(100'000, 1.5f));
  const auto p = predictor.predict(2'000'000);
  REQUIRE(p.has_value());
  CHECK(p->x_m == Approx(1.5f));
  CHECK(predictor.size() == 1);
}

// Hand-computed: x goes 1.0 -> 2.0 over 100 ms, so at t1 + 100 ms it reaches 3.0 and at
// t1 + 50 ms it reaches 2.5.
TEST_CASE("linear position extrapolation matches hand-computed values",
          "[tracking][predict]") {
  trk::PosePredictor predictor;
  predictor.push(sample(0, 1.0f));
  predictor.push(sample(100'000, 2.0f));

  CHECK(predictor.predict(200'000)->x_m == Approx(3.0f).epsilon(1e-5));
  CHECK(predictor.predict(150'000)->x_m == Approx(2.5f).epsilon(1e-5));
  // Querying at/before the newest sample returns it unextrapolated.
  CHECK(predictor.predict(100'000)->x_m == Approx(2.0f));
  CHECK(predictor.predict(50'000)->x_m == Approx(2.0f));
}

// Hand-computed wrap case: pan +179 deg -> -179 deg is a +2 deg step the short way
// around, so one more step lands at -177 deg — never a 358 deg snap backwards.
TEST_CASE("angle extrapolation unwraps across +/-180 deg", "[tracking][predict]") {
  const float deg = kPi / 180.0f;
  trk::PosePredictor predictor;
  predictor.push(sample(0, 0.0f, 179.0f * deg));
  predictor.push(sample(100'000, 0.0f, -179.0f * deg));

  const auto p = predictor.predict(200'000);
  REQUIRE(p.has_value());
  CHECK(p->pan_rad == Approx(-177.0f * deg).epsilon(1e-4));
}

// The latency offset shifts the query time — the acceptance criterion "latency offset
// measurably shifts the predicted pose" at the unit level.
TEST_CASE("larger latency offset shifts the prediction further", "[tracking][predict]") {
  trk::PosePredictor predictor;
  predictor.push(sample(0, 0.0f));
  predictor.push(sample(100'000, 1.0f));  // 10 m/s

  const std::uint64_t now = 100'000;
  const float at_20ms = predictor.predict(now + 20'000)->x_m;
  const float at_80ms = predictor.predict(now + 80'000)->x_m;
  CHECK(at_20ms == Approx(1.2f).epsilon(1e-4));
  CHECK(at_80ms == Approx(1.8f).epsilon(1e-4));
  CHECK(at_80ms > at_20ms);
}

TEST_CASE("prediction freezes past the extrapolation horizon (stale tracking)",
          "[tracking][predict]") {
  trk::PosePredictor predictor(256, 200'000);  // 200 ms horizon
  predictor.push(sample(0, 0.0f));
  predictor.push(sample(100'000, 1.0f));

  const float at_horizon = predictor.predict(300'000)->x_m;   // exactly 200 ms out
  const float way_beyond = predictor.predict(5'000'000)->x_m;  // 4.9 s of dropout
  CHECK(at_horizon == Approx(3.0f).epsilon(1e-4));
  CHECK(way_beyond == Approx(at_horizon).epsilon(1e-4));  // frozen, not flying off
}

TEST_CASE("zoom/focus pass through from the newest sample", "[tracking][predict]") {
  trk::PosePredictor predictor;
  predictor.push(sample(0, 0.0f));
  predictor.push(sample(100'000, 1.0f));
  CHECK(predictor.predict(150'000)->zoom_raw == 42u);
}

TEST_CASE("ring buffer caps history at capacity", "[tracking][predict]") {
  trk::PosePredictor predictor(8);
  for (int i = 0; i < 20; ++i) {
    predictor.push(sample(static_cast<std::uint64_t>(i) * 10'000, static_cast<float>(i)));
  }
  CHECK(predictor.size() == 8);
  // Newest two still drive prediction: x = 19 at t=190ms, slope 100 units/s.
  CHECK(predictor.predict(200'000)->x_m == Approx(20.0f).epsilon(1e-4));
}
