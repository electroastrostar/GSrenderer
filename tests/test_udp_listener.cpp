// Loopback tests: bind an ephemeral port, send real packets through the OS UDP stack.
// Polling with deadlines keeps them deterministic-enough for CI while still exercising
// the actual socket path (no tracker hardware involved — plan §7).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#include "tracking/freed.hpp"
#include "tracking/udp_listener.hpp"

using namespace std::chrono_literals;
namespace trk = gsr::tracking;

namespace {
// Polls `done` for up to `deadline`; returns whether it became true.
template <typename Predicate>
bool wait_for(Predicate done, std::chrono::milliseconds deadline) {
  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < deadline) {
    if (done()) return true;
    std::this_thread::sleep_for(2ms);
  }
  return done();
}
}  // namespace

TEST_CASE("listener receives, decodes and stamps good packets", "[tracking][udp]") {
  std::atomic<int> callbacks{0};
  trk::FreedListener listener(0, [&](const trk::TimedPose&) { ++callbacks; });
  REQUIRE(listener.port() != 0);

  trk::FreedPose pose;
  pose.camera_id = 5;
  pose.pan_rad = 0.5f;
  pose.x_m = 2.0f;
  trk::UdpSender sender("127.0.0.1", listener.port());
  const auto packet = trk::serialize_freed_d1(pose);
  REQUIRE(sender.send(packet));

  REQUIRE(wait_for([&] { return listener.stats().packets_ok >= 1; }, 2000ms));
  const auto latest = listener.latest();
  REQUIRE(latest.has_value());
  CHECK(latest->pose.camera_id == 5);
  CHECK(latest->pose.pan_rad == Catch::Approx(0.5f).epsilon(1e-3));
  CHECK(latest->pose.x_m == Catch::Approx(2.0f).epsilon(1e-3));
  CHECK(latest->t_mono_us > 0);
  CHECK(callbacks.load() >= 1);
}

TEST_CASE("listener counts corrupt packets as rejected", "[tracking][udp]") {
  trk::FreedListener listener(0);
  trk::UdpSender sender("127.0.0.1", listener.port());

  auto bad = trk::serialize_freed_d1(trk::FreedPose{});
  bad[10] ^= 0xFF;  // breaks the checksum
  REQUIRE(sender.send(bad));
  std::uint8_t runt[3] = {0xD1, 0, 0};  // wrong size
  REQUIRE(sender.send(runt));

  REQUIRE(wait_for([&] { return listener.stats().packets_rejected >= 2; }, 2000ms));
  CHECK(listener.stats().packets_ok == 0);
  CHECK_FALSE(listener.latest().has_value());
}

TEST_CASE("listener measures packet rate over a window", "[tracking][udp]") {
  trk::FreedListener listener(0);
  trk::UdpSender sender("127.0.0.1", listener.port());
  const auto packet = trk::serialize_freed_d1(trk::FreedPose{});

  // ~50 Hz for ~1.3 s so at least one full rate window elapses.
  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < 1300ms) {
    sender.send(packet);
    std::this_thread::sleep_for(20ms);
  }
  REQUIRE(wait_for([&] { return listener.stats().packet_rate_hz > 0.0; }, 2000ms));
  const auto stats = listener.stats();
  // Loose bounds — CI scheduling jitter — but it must be the right order of magnitude.
  CHECK(stats.packet_rate_hz > 20.0);
  CHECK(stats.packet_rate_hz < 90.0);
}
