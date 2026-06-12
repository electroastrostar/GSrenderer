# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 0 — Project Scaffolding**
(branch `claude/phase-0-scaffolding-qf5mw4`).

## Done

- (nothing yet)

## In Progress

- **Phase 0, Task 1 — `CLAUDE.md`** with build commands, coding standards, and
  coordinate-convention rules.
  - Exact next step: write `CLAUDE.md` at the repo root, commit, push.

## Next

- Phase 0, Task 2 — CMake project: C++20, CUDA toolchain detection, Debug/Release configs.
- Phase 0, Task 3 — `docs/architecture.md` skeleton with data-flow diagram
  (tracker → pose filter → frustum → render → output).
- Phase 0, Task 4 — Logging (spdlog) with per-subsystem levels and frame-stamped
  format (`[frame N][t_mono_us]`).
- Phase 0, Task 5 — Unit test framework (Catch2) wired into CMake; hello-world test.
- Phase 0 acceptance check — clean `cmake --build` (Debug + Release), tests pass; open PR.
