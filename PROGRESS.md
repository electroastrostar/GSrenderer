# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 1 — Asset Loading with
Full SH** (branch `claude/phase-0-scaffolding-qf5mw4`, restarted from `main` after the
Phase 0 PR #1 merged — session tooling pins the branch name; PR title carries the phase).

## Done

- **Phase 0 — complete.** Merged to `main` in PR #1 after operator verification passed on
  the dev machines (CUDA detection, MSVC build, 8/8 tests, frame-stamped logging).

## In Progress

- **Phase 1, Tasks 1+2 — binary PLY loader** for INRIA-format 3DGS output (position, scale,
  rotation quat, opacity, `f_dc_*`, `f_rest_*`) with SH degree detection (0–3) from the
  `f_rest_*` property count.
  - Exact next step: implement `src/loader/` (`splat_data.hpp`, `ply_loader.{hpp,cpp}`),
    unit tests incl. degree 0–3 + malformed-file rejection; build, test, commit, push.

## Next

- Phase 1, Task 3 — pack into structure-of-arrays GPU-ready buffers; document the memory
  layout in `docs/splat-memory-layout.md`.
- Phase 1, Task 4 — `tools/asset_inspector`: CLI printing splat count, SH degree, bounding
  box, memory footprint estimate; tiny committed fixture asset for verification.
- Phase 1, Task 5 (stretch, optional) — `.spz` / `.splat` (antimatter15) support behind the
  same loader interface. Will defer unless time allows.
- Phase 1 acceptance — inspector output matches known values for a reference asset; loader
  tests cover degrees 0–3 and malformed files. `docs/verification/phase-1.md` + PR.
