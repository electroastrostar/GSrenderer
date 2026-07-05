#pragma once

#include <cstdint>

// Software frame pacer (plan Phase 5 task 3): locks the render loop to 24/25/30 fps by
// scheduling frame starts on a fixed slot grid derived from the monotonic clock, and
// counts frames that miss their slot. Pure logic — the caller supplies timestamps
// (gsr::log::mono_us() in the app; fake clocks in tests) and does the sleeping.
namespace gsr::output {

class FramePacer {
 public:
  // Anchors the slot grid at now_us; the first deadline is one interval later.
  // Throws std::invalid_argument for fps outside (0, 1000].
  FramePacer(double fps, std::uint64_t now_us);

  // Deadline of the current frame slot (absolute monotonic us). The caller renders,
  // sends, then calls frame_done(now); sleeping until this deadline between frames is
  // the caller's job.
  std::uint64_t next_deadline_us() const;

  // Marks the frame complete. Finishing after the slot deadline counts as LATE, and the
  // next deadline snaps forward to the next future slot on the ORIGINAL grid — missed
  // slots are skipped, never queued, so one stall can't cause a catch-up burst.
  void frame_done(std::uint64_t now_us);

  std::uint64_t frames_total() const { return total_; }
  std::uint64_t frames_late() const { return late_; }
  double interval_us() const { return interval_us_; }

 private:
  double interval_us_ = 0.0;
  std::uint64_t anchor_us_ = 0;
  std::uint64_t slot_ = 1;  // index of the pending deadline on the grid
  std::uint64_t total_ = 0;
  std::uint64_t late_ = 0;
};

}  // namespace gsr::output
