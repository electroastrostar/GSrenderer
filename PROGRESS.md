# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 2 — Core Renderer,
static camera** (branch `claude/phase-0-scaffolding-qf5mw4`, restarted from `main` after
the Phase 1 PR #2 merged — session tooling pins the branch name; PR title carries the phase).

**Phase 2 development constraint:** authored in a container with no GPU. CUDA toolkit 12.0
was installed in-container, so all CUDA/GLFW code is COMPILE-verified (nvcc arch 86, zero
warnings) — but never EXECUTED. Kernel correctness/perf is validated on the A6000 via the
hidden "[gpu]" tests + visual check during operator verification.

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

- Phase 2, Task 1b — CUDA pipeline (`splat_renderer.cu`): preprocess (cull+project+SH) →
  tile binning → CUB radix sort → front-to-back blend; per-stage cudaEvent timings;
  grow-only device buffers; hot path returns nullptr + logs (no exceptions).
- Phase 2, Tasks 3+4 — GLFW preview with CUDA→GL PBO interop, WASD/mouse fly camera,
  window-title HUD + 1 Hz frame-stamped timing log; CLI flags (--width/--height/--fov/
  --sh-clamp/--vsync). Hidden "[.gpu]" integration tests for the A6000.

- Phase 2 wrap-up — `docs/verification/phase-2.md` written (first-GPU-run warning, [gpu]
  tests, SuperSplat visual match, perf gate w/ HUD numbers). Fresh out-of-tree host build:
  0 warnings, 44/44 tests. CUDA side compile-verified (arch 86, 0 warnings), NOT executed.

- Phase 2, PR #3 iteration 1 — operator report: real scene spawns "infinitely far",
  WASD ineffective (fixture OK). Cause: raw bounds inflated by SfM outlier splats +
  fixed 2 m/s speed in arbitrary-scale scenes. Fix: compute_robust_bounds (percentile,
  tested), camera framing + fly speed derived from robust radius, scroll-wheel speed
  control with `spd` HUD readout. 46/46 tests, CUDA build clean.

- Phase 2, PR #3 iteration 2 — operator report: scene renders upside down (framing now
  OK). Cause: COLMAP assets are y-down vs our y-up render world; fixture was symmetric so
  invisible in §5a. Fix per convention rules: named `world_from_asset`/`asset_from_world`
  (180° about X, involution) in core/transforms with hand-checked tests; preview composes
  view_from_asset + SH camera position in asset space; `--no-flip` escape hatch;
  architecture.md coordinate table + transform inventory updated. 49/49 tests.

- Phase 2, PR #3 iteration 3 — perf: operator measured 49.3 fps @1080p on a laptop 3080
  (advisory; A6000 projection ~85-90 fps, gate likely met) with 9.8M pairs. Landed the
  approved binning optimization: opacity-aware alpha-cutoff extents (cull < 1/255) +
  exact tile/ellipse overlap test shared by count & emission loops (offsets-parity guard
  test). Deep perf work (plan §6.3) deferred until an A6000 measurement fails the gate.
  53/53 tests, CUDA build clean.

## In Progress

- (Phase 2 PR #3 awaiting operator perf re-test of §6, then merge → Phase 3.)

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
