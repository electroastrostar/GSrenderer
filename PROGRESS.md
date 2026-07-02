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

- Phase 1, Task 4 — `tools/asset_inspector` CLI (count, SH degree, bounds, memory);
  deterministic fixture `assets/fixtures/cube_deg3.ply` (+ generator script) pinned to its
  hand-derived values by `tests/test_fixture_asset.cpp`.

- Phase 1 acceptance — fresh out-of-tree Release build: 0 warnings/errors; 19/19 tests
  pass; fixture regenerates byte-identically. `docs/verification/phase-1.md` written.
  Task 5 stretch (.spz/.splat) **deferred** — same loader interface makes it a drop-in later.

## In Progress

- (nothing — Phase 1 complete; awaiting operator verification per
  `docs/verification/phase-1.md`, then PR merge)

## Next

- Phase 2 — Core Renderer (static camera); needs the A6000/CUDA machine for perf work.
- Deferred: `.spz` / `.splat` loader support (Phase 1 stretch) — pick up opportunistically.
