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

## In Progress

- **Phase 4, Task 1+2a — overscan-capable intrinsics (Mode A/B)**: named
  `with_overscan(Intrinsics, fraction)` in core/camera producing the expanded
  asymmetric frustum (same fx/fy/optical center, wider image); CLI `--overscan PCT`;
  hand-checked tests (pixel-offset fixtures).
  - Exact next step: implement + tests; commit.

## Next

- Phase 4, Task 4 — stage alignment: `world_from_stage` pose transform (yaw about up +
  offset, meters) in core/transforms with hand-checked tests; config entries.
- Phase 4, config — TOML run config (toml++ pinned via FetchContent, per plan §4
  default): asset/size/fov/tracking/lens/overscan/stage-offset; CLI overrides; tests.
- Phase 4, Task 3 — explicit SH-origin test: identical view matrix, different
  camera_position_world → colors change ([gpu] test; field independence proves the
  physical-camera origin is what SH uses).
- Phase 4, fiducial — `tools/make_fiducial_ply.py` → committed grid/checkerboard asset
  (deg 0, small) with colored corner markers for line-up + overscan pixel measurement.
- Phase 4 wrap-up — architecture.md Mode A/B section; `docs/verification/phase-4.md`
  (overscan crop measurement renderer-only; UE5 line-up path for when a UE machine is
  available); PR.
