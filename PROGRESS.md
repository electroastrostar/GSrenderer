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

- Phase 2, groundwork — GLM math dependency (ADR 0001); `gsr::core` camera model
  (view_from_world, off-axis-capable clip_from_view from pixel intrinsics) with 9
  hand-checked unit tests. 28/28 tests green.

- Phase 2, Task 2 — SH evaluation degrees 0–3 (`src/renderer/sh.hpp`, host/device-shared)
  with 6 hand-computed CPU tests incl. channel-major layout + view-dependence property.

- Phase 2, Task 1a — covariance math (`src/renderer/covariance.hpp`): quat+scale → 3D
  covariance, EWA 2D projection, conic+radius, named view→projection-frame adapter; 8
  hand-checked CPU tests. 42/42 green.

## In Progress

- **Phase 2, Task 1b — CUDA pipeline** (`src/renderer/`): preprocess (cull + project +
  SH) → 16×16 tile binning → CUB radix sort on [tile|depth] keys → front-to-back blend.
  Compiles only when CUDA detected; CANNOT be compiled in this container — first real
  build happens on the A6000 during verification.
  - Exact next step: `splat_renderer.{hpp,cu}` + kernels; commit, push (unverified compile,
    flagged in PR).

## Next

- Phase 2, Task 1b — CUDA pipeline (`src/renderer/`): preprocess (cull + project) →
  16×16 tile binning → CUB radix sort on [tile|depth] keys → front-to-back blend kernel.
- Phase 2, Task 3 — GLFW preview window + CUDA→GL interop (PBO), free-fly WASD camera.
  Built only when CUDA is detected (preview needs the GPU anyway).
- Phase 2, Task 4 — frame timing: CUDA-event ms per stage (cull/sort/blend), window-title
  HUD + once-per-second frame-stamped log line.
- Phase 2, Task 5 — perf gate on A6000 with a 3–6M splat asset (operator step).
- Phase 2 acceptance — visual match vs SuperSplat/SIBR on same asset; ≥60 fps @1080p/3M
  on A6000; SH view-dependence visible on orbit. `docs/verification/phase-2.md` + PR.
