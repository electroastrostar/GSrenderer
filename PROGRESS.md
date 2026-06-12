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

## In Progress

- **Phase 0, Task 3 — `docs/architecture.md` skeleton** with data-flow diagram
  (tracker → pose filter → frustum → render → output).
  - Exact next step: write `docs/architecture.md` + `docs/decisions/README.md`; commit, push.

## Next

- Phase 0, Task 4 — Logging (spdlog) with per-subsystem levels and frame-stamped
  format (`[frame N][t_mono_us]`), plus unit tests.
- Phase 0 acceptance check — clean `cmake --build` (Debug + Release), tests pass; open PR.
