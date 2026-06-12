# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 0 — Project Scaffolding**
(branch `claude/phase-0-scaffolding-qf5mw4`).

## Done

- Phase 0, Task 1 — `CLAUDE.md` (build commands, coding standards, coordinate-convention
  rules). Commit: see `phase0: add CLAUDE.md`.

## In Progress

- **Phase 0, Task 2 — CMake project**: C++20, CUDA toolchain detection, Debug/Release configs.
  - Exact next step: write root `CMakeLists.txt` + `src/core`, `src/app` targets with a stub
    main; verify both configs build; commit, push.

## Next

- Phase 0, Task 3 — `docs/architecture.md` skeleton with data-flow diagram
  (tracker → pose filter → frustum → render → output).
- Phase 0, Task 4 — Logging (spdlog) with per-subsystem levels and frame-stamped
  format (`[frame N][t_mono_us]`).
- Phase 0, Task 5 — Unit test framework (Catch2) wired into CMake; hello-world test.
- Phase 0 acceptance check — clean `cmake --build` (Debug + Release), tests pass; open PR.
