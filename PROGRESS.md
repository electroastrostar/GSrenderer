# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 4 — Off-Axis Frustum
for the Volume** (branch `claude/phase-0-scaffolding-qf5mw4`, restarted from `main`
after Phase 3 PR #4 merged).

**Phase 4 scoping notes:** Mode A (lens-matched perspective — "what the physical camera
sees") is the recommended-first contract and is largely what Phases 2+3 already render;
this phase formalizes it, adds Mode B overscan, stage-origin alignment config, the
explicit SH-origin test, and a fiducial asset for line-up verification. The UE5
side-by-side acceptance needs a UE5 machine — verification doc will carry a UE5 path
plus renderer-only pixel checks (overscan crop measurement) that don't.

## Done

- **Phases 0–3 complete.** PRs #1–#4 merged after operator verification (see git log;
  Phase 3 closed after 4 verification iterations incl. wire-angle wrap, tracking-stale
  handback, live latency stepping with measured lead, extrapolation-horizon fix).

- Phase 4, Tasks 1+2 math — `with_overscan` (exact-center-crop contract, hand-checked
  96/54-px fixtures + projection superset property test) and Task 4 transform —
  `world_from_stage` (yaw about +Y then offset; hand-checked R_y(90°) fixture). 83/83.

- Phase 4, config + wiring — TOML run config (toml++ v3.4.0, `gsr_config` lib,
  `configs/example_run.toml`, 4 tests: overlay/defaults/example/malformed); CLI
  reworked: `--config` two-pass with flag overrides, `--overscan`, `--stage-yaw-deg`,
  `--stage-offset X,Y,Z`; preview renders the overscanned target (window = overscan
  size, base = exact center crop; lens fy still maps to base height) and applies
  world_from_stage to tracked poses. 86/86 both builds.

## In Progress

- **Phase 4, Task 3 + fiducial** — [gpu] SH-origin test (same view matrix, different
  camera_position_world → colors change) and `tools/make_fiducial_ply.py` → committed
  grid fiducial asset for line-up/overscan measurement.
  - Exact next step: implement both; commit.

## Next

- Phase 4, Task 3 (old) — explicit SH-origin test: identical view matrix, different
  camera_position_world → colors change ([gpu] test; field independence proves the
  physical-camera origin is what SH uses).
- Phase 4, fiducial — `tools/make_fiducial_ply.py` → committed grid/checkerboard asset
  (deg 0, small) with colored corner markers for line-up + overscan pixel measurement.
- Phase 4 wrap-up — architecture.md Mode A/B section; `docs/verification/phase-4.md`
  (overscan crop measurement renderer-only; UE5 line-up path for when a UE machine is
  available); PR.
