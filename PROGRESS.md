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

## In Progress

- **Phase 3, Task 1 — FreeD D1 packet codec** (`src/tracking/freed.{hpp,cpp}`): parse +
  serialize + checksum, wire-unit conversion (deg/32768, mm/64 → radians/meters) at the
  I/O boundary; `docs/freed-protocol.md`; unit tests against hand-built byte fixtures.
  - Exact next step: implement codec + doc + tests; build, test, commit, push.

## Next

- Phase 3, Task 2 — UDP listener (`src/tracking/udp_listener.*`, Winsock/POSIX):
  background receive thread, checksum validation, dropped-packet stats, rate monitor.
- Phase 3, Task 3 — `tools/freed_simulator`: static / orbit / handheld-noise profiles,
  configurable port + rate, shares the codec.
- Phase 3, Task 4 — pose filter: timestamped ring buffer + linear-velocity prediction
  with configurable latency offset (ms); unit tests with hand-computed fixtures.
- Phase 3, Task 5 — lens model: zoom/focus raw → focal length via CSV table
  (interpolated), fixed-intrinsics fallback; unit tests.
- Phase 3, Task 6 — `render_from_freed` convention transform in `src/core/` (+
  architecture.md coordinate table row + hand-checked tests).
- Phase 3, integration — preview gains `--freed-port` tracked-camera mode +
  `--latency-ms`; smooth orbit from simulator = acceptance.
- Phase 3 wrap-up — `docs/verification/phase-3.md` (simulator-based, no tracker
  needed; optional phone-app section), PR.
