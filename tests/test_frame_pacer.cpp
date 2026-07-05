#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "output/frame_pacer.hpp"

using Catch::Approx;
using gsr::output::FramePacer;

// Hand-computed at 25 fps (interval 40'000 us), anchored at t = 1'000'000.
TEST_CASE("pacer schedules fixed slots and counts on-time frames", "[output][pacer]") {
  FramePacer pacer(25.0, 1'000'000);
  CHECK(pacer.interval_us() == Approx(40'000.0));
  CHECK(pacer.next_deadline_us() == 1'040'000);

  pacer.frame_done(1'030'000);  // finished early
  CHECK(pacer.next_deadline_us() == 1'080'000);
  pacer.frame_done(1'080'000);  // exactly on the deadline is on time
  CHECK(pacer.frames_total() == 2);
  CHECK(pacer.frames_late() == 0);
  CHECK(pacer.next_deadline_us() == 1'120'000);
}

// A late frame counts once and scheduling snaps to the NEXT future slot on the original
// grid — no queued catch-up frames, no grid drift.
TEST_CASE("late frames skip missed slots without drifting the grid", "[output][pacer]") {
  FramePacer pacer(25.0, 1'000'000);

  pacer.frame_done(1'085'000);  // missed the 1'040'000 slot and the 1'080'000 slot
  CHECK(pacer.frames_late() == 1);
  CHECK(pacer.next_deadline_us() == 1'120'000);  // still on the anchor grid

  pacer.frame_done(2'000'000);  // long stall: 22 slots gone
  CHECK(pacer.frames_late() == 2);
  CHECK(pacer.next_deadline_us() == 2'040'000);  // next future slot, grid preserved
}

// 24 fps has a non-integer interval (41'666.67 us) — the grid must not accumulate
// rounding drift: slot 24 lands at exactly 1 second.
TEST_CASE("fractional intervals do not accumulate drift (24 fps)", "[output][pacer]") {
  FramePacer pacer(24.0, 0);
  for (int i = 0; i < 23; ++i) pacer.frame_done(pacer.next_deadline_us());
  CHECK(pacer.next_deadline_us() == 1'000'000);  // llround(24 * 41666.6667)
  CHECK(pacer.frames_late() == 0);
}

TEST_CASE("pacer rejects invalid rates", "[output][pacer]") {
  CHECK_THROWS_AS(FramePacer(0.0, 0), std::invalid_argument);
  CHECK_THROWS_AS(FramePacer(-24.0, 0), std::invalid_argument);
  CHECK_THROWS_AS(FramePacer(2000.0, 0), std::invalid_argument);
}
