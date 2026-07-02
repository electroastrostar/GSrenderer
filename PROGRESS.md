# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 2 — Core Renderer,
static camera** (branch `claude/phase-0-scaffolding-qf5mw4`, restarted from `main` after
the Phase 1 PR #2 merged — session tooling pins the branch name; PR title carries the phase).

**Phase 2 development constraint:** authored in a container with no GPU/`nvcc`. All math
(SH eval, covariance, camera) is CPU-unit-tested here; CUDA kernels + GLFW preview compile
only behind CUDA detection and get their first real build on the A6000 during operator
verification — expect an iteration loop on the PR.

## Done

- **Phase 0 — complete.** Merged in PR #1 (scaffolding, logging, tests).
- **Phase 1 — complete.** Merged in PR #2 (PLY loader w/ full SH, SoA buffers,
  asset_inspector). Stretch task (.spz/.splat) deferred.

## In Progress

- **Phase 2, groundwork — math dependency + camera model.** GLM (pinned FetchContent) as
  host/device math library (ADR 0001), `gsr::core` camera: view matrix from pose,
  perspective projection from intrinsics, hand-checked unit tests.
  - Exact next step: ADR 0001 + GLM in cmake/Dependencies.cmake + `src/core/camera.{hpp,cpp}`
    + tests; build, test, commit, push.

## Next

- Phase 2, Task 2 — SH evaluation (degrees 0–3) as host/device-shared header, CPU unit
  tests against hand-computed values (view dir = camera position → splat center).
- Phase 2, Task 1a — covariance math: quat+scale → 3D covariance; EWA projection to 2D
  conic; host/device-shared, CPU unit tests.
- Phase 2, Task 1b — CUDA pipeline (`src/renderer/`): preprocess (cull + project) →
  16×16 tile binning → CUB radix sort on [tile|depth] keys → front-to-back blend kernel.
- Phase 2, Task 3 — GLFW preview window + CUDA→GL interop (PBO), free-fly WASD camera.
  Built only when CUDA is detected (preview needs the GPU anyway).
- Phase 2, Task 4 — frame timing: CUDA-event ms per stage (cull/sort/blend), window-title
  HUD + once-per-second frame-stamped log line.
- Phase 2, Task 5 — perf gate on A6000 with a 3–6M splat asset (operator step).
- Phase 2 acceptance — visual match vs SuperSplat/SIBR on same asset; ≥60 fps @1080p/3M
  on A6000; SH view-dependence visible on orbit. `docs/verification/phase-2.md` + PR.
