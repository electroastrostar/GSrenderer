# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 1 — Asset Loading with
Full SH** (branch `claude/phase-0-scaffolding-qf5mw4`, restarted from `main` after the
Phase 0 PR #1 merged — session tooling pins the branch name; PR title carries the phase).

## Done

- **Phase 0 — complete.** Merged to `main` in PR #1 after operator verification passed on
  the dev machines (CUDA detection, MSVC build, 8/8 tests, frame-stamped logging).

- Phase 1, Tasks 1+2 — binary PLY loader (`src/loader/`) for INRIA-format 3DGS with SH
  degree detection 0–3, streamed reads, activations at load (exp/sigmoid/normalize),
  arbitrary property order; 10 loader tests incl. hand-checked fixture + malformed-file
  rejection. SoA packing already done as part of the loader (`SplatData`).

- Phase 1, Task 3 — `docs/splat-memory-layout.md`: SoA arrays, channel-major `sh_rest`
  ordering, color reconstruction formula, per-splat byte costs, intentional omissions.

## In Progress

- **Phase 1, Task 4 — `tools/asset_inspector`**: CLI printing splat count, SH degree,
  bounding box, memory footprint; tiny committed fixture asset with known values.
  - Exact next step: write `tools/asset_inspector/main.cpp` + CMake, generate
    `assets/fixtures/` test PLY via script, verify output, commit, push.

## Next
- Phase 1, Task 5 (stretch, optional) — `.spz` / `.splat` (antimatter15) support behind the
  same loader interface. Will defer unless time allows.
- Phase 1 acceptance — inspector output matches known values for a reference asset; loader
  tests cover degrees 0–3 and malformed files. `docs/verification/phase-1.md` + PR.
