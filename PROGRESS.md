# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 3 — FreeD Tracking
Ingest + Pose Filtering** (branch `claude/phase-0-scaffolding-qf5mw4`, restarted from
`main` after Phase 2 PR #3 merged — session tooling pins the branch name).

**Phase 3 testing constraint (operator):** no StarTracker access (not at the studio).
All acceptance runs against `tools/freed_simulator` (plan Task 3) — desk-testable by
design. Bonus path: the iOS app "VirtualProductionCamera" emits standard FreeD D1 over
UDP and can act as a handheld tracker against the same listener port.

## Done

- **Phase 0 — complete.** PR #1 (scaffolding, logging, tests).
- **Phase 1 — complete.** PR #2 (PLY loader w/ full SH, SoA, asset_inspector).
- **Phase 2 — complete.** PR #3 (CUDA rasterizer, preview + interop, fly camera, HUD;
  operator-verified on a 3080 laptop incl. 3 feedback iterations: robust framing,
  world_from_asset flip, opacity-aware/exact-overlap binning).

- Phase 3, Task 1 — FreeD D1 codec (`src/tracking/freed.*`): parse/serialize/checksum,
  wire→internal unit conversion at the boundary; `docs/freed-protocol.md`; 5 tests incl.
  hand-built byte fixture, round-trip, malformed rejection, clamping. 58/58 green.

- Phase 3, Task 2 — UDP listener (`src/tracking/udp_listener.*`): Winsock/POSIX socket
  wrapper, background receive thread, stats (ok/rejected/rate), latest-pose mailbox +
  callback, UdpSender for simulator/tests; 3 loopback tests. 61/61 green.

- Phase 3, Task 3 — `tools/freed_simulator`: static/orbit/handheld profiles (layered-
  sinusoid handheld jitter), --port/--rate/--radius/--height/--period/--duration; paced
  send loop. Smoke-tested against an independent python checksum validator (60/60 ok).

- Phase 3, Task 4 — pose predictor (`src/tracking/pose_predictor.*`): timestamped ring
  buffer, linear extrapolation with angle unwrapping (±180° pan crossings), stale-data
  freeze horizon; latency offset applied at the query site. 7 hand-computed tests
  incl. the "latency offset shifts the prediction" acceptance property. 68/68 green.

- Phase 3, Task 5 — lens model (`src/tracking/lens_table.*`): interpolated CSV
  zoom→focal table (header/comment tolerant, clamped ends), fixed-focal fallback,
  focal_px_from_mm; `configs/example_lens.csv`; 6 tests. 74/74 green.

- Phase 3, Task 6 — `render_from_freed` in core/transforms: freed X→−Z, Y→−X, Z→+Y,
  pan/tilt/roll per freed-protocol.md; 3 hand-checked fixture groups + simulator-orbit
  "faces the origin" consistency test; architecture.md table + inventory updated.
  77/77 green.

- Phase 3, integration — tracked-camera preview: `--freed-port`/`--latency-ms`/
  `--lens-file`/`--sensor-height-mm`; listener → predictor → render_from_freed →
  renderer; per-frame zoom→focal intrinsics when a lens table is loaded; fly controls
  remain until first packet; HUD/log gain `trk <rate>Hz ok:<n> rej:<n>`. Host + CUDA
  builds clean, 77/77.

- Phase 3 wrap-up — `docs/verification/phase-3.md` written (all steps simulator-driven,
  two-terminal walkthrough, optional iOS VirtualProductionCamera section, not a merge
  gate). Fresh out-of-tree build: 0 warnings, 77/77 tests. PR opened.

- Phase 3, PR #4 iteration 1 — operator hit MSVC build errors: preview.hpp used
  std::string without <string> (GCC transitive include masked it); the §3 "unknown
  option --freed-port" was the stale Phase 2 exe after that build failure. Fix:
  <string> in preview.hpp + <memory> (make_unique) in preview.cpp. 77/77 both builds.

## In Progress

- (Phase 3 PR #4 awaiting operator re-verification from §1, then merge → Phase 4.)
