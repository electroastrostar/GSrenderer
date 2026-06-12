# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 0 — Project Scaffolding**
(branch `claude/phase-0-scaffolding-qf5mw4`).

## Done

- Phase 0, Task 1 — `CLAUDE.md` (build commands, coding standards, coordinate-convention
  rules). Commit: see `phase0: add CLAUDE.md`.

- Phase 0, Task 2 — CMake project (C++20, CUDA detection w/ graceful CPU-only fallback,
  Debug/Release; `gsr_core` lib + `splatcast` app stub).
- Phase 0, Task 5 — Catch2 v3 wired into CMake via FetchContent + `catch_discover_tests`;
  hello-world test passing (done alongside Task 2 — the test target was part of the scaffold).

- Phase 0, Task 3 — `docs/architecture.md` skeleton (data-flow diagram, coordinate-space
  table, transform inventory, subsystem map) + `docs/decisions/` ADR template.

- Phase 0, Task 4 — Logging: `gsr::log` (spdlog) with per-subsystem levels and frame-stamped
  format `[frame N][t_mono_us T]`; 6 unit tests; app stub now logs through it.

- Phase 0 acceptance — fresh out-of-tree Release configure/build: 0 warnings/errors;
  8/8 tests pass in Debug and Release; hello-world test green. PR opened.

## In Progress

- (nothing — Phase 0 complete, awaiting PR review)

## Next

- Phase 1 — Asset Loading with Full SH (new branch after Phase 0 PR merges).
