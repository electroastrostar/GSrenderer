# Architecture

> Skeleton (Phase 0). Sections marked **TBD** are filled in by the phase that implements them.
> The **Coordinate Spaces** section is the single source of truth for conventions — see the
> rules in `CLAUDE.md`.

## 1. System Overview

A standalone process that loads a 3DGS splat asset, renders it from a live-tracked physical
camera pose with a lens-matched (later: off-axis/overscanned) frustum, and streams frames to
UE5 nDisplay — Phase A over NDI, Phase B over SMPTE ST 2110-20 (Rivermax, PTP-locked).
nDisplay keeps ownership of wall warp, outer frustum, and final LED output.

## 2. Data Flow

```
 Mo-Sys StarTracker                                                  UE5 nDisplay
 (FreeD over UDP)                                                    (inner frustum)
        │                                                                  ▲
        ▼                                                                  │
 ┌──────────────┐   ┌─────────────┐   ┌──────────────┐   ┌─────────┐   ┌──────────┐
 │   tracking/  │──▶│ pose filter │──▶│   frustum    │──▶│ renderer│──▶│  output/ │
 │ FreeD parser │   │ + predictor │   │ (off-axis    │   │ (CUDA   │   │ NDI      │
 │ checksum,    │   │ ring buffer,│   │  projection, │   │ cull/   │   │ ST 2110  │
 │ pkt stats    │   │ latency ofs │   │  lens model) │   │ sort/   │   └──────────┘
 └──────────────┘   └─────────────┘   └──────────────┘   │ blend)  │
                                                          └─────────┘
                                                               ▲
                                                  ┌────────────┴───────────┐
                                                  │ loader/ (.ply/.splat,  │
                                                  │ full SH → SoA GPU bufs)│
                                                  └────────────────────────┘
```

Control plane (Phase 7): config (TOML) at startup; OSC/HTTP endpoint for load/start/stop/
latency-offset. Telemetry: per-frame timings, packet rate, late frames.

## 3. Coordinate Spaces & Conventions

**Units: meters. Angles: radians (internal).** Converted at I/O boundaries only.

| Space | Handedness | Up | Forward | Units (wire) | Defined by |
|---|---|---|---|---|---|
| Splat asset space | right-handed (COLMAP/INRIA) | **−Y** (asset +Y points down) | +Z | scene-dependent scale | Phase 1/2 (confirmed on real assets, PR #3) |
| Render world space | right-handed | +Y | −Z (camera at identity) | meters* | Phase 2 |
| FreeD / StarTracker space | right-handed | +Z | +X at pan 0 (pan CW-from-above, tilt up, roll CW-from-behind) | mm + degrees on wire (int24 fixed-point) | Phase 3 (`docs/freed-protocol.md`; **verify signs on stage**) |
| Renderer camera space | right-handed, Y-up, −Z forward | +Y | −Z | meters | Phase 2 |
| UE5 / nDisplay space | left-handed, Z-up, +X forward, **centimeters** | +Z | +X | cm | UE convention |

\* SfM-trained assets have arbitrary metric scale; stage alignment (scale + origin) is a
Phase 4 config concern. The preview treats asset units as meters.

### Transform inventory

Every inter-space transform is a named `X_from_Y` function in `src/core/` with a
hand-checked unit test (see `CLAUDE.md`):

| Function | From → To | Phase | Test |
|---|---|---|---|
| `world_from_asset` / `asset_from_world` (`core/transforms.hpp`) | splat asset ↔ render world (180° about X; SuperSplat-compatible default, `--no-flip` to disable) | 2 | `tests/test_transforms.cpp` |
| `render_from_freed` (`core/transforms.hpp`) | FreeD/StarTracker → render-world camera pose (freed X→−Z, Y→−X, Z→+Y; pan/tilt/roll per `freed-protocol.md`) | 3 | `tests/test_transforms.cpp` (incl. simulator-orbit consistency) |
| `view_from_world` (`core/camera.hpp`) | render world → camera view | 2 | `tests/test_camera.cpp` |
| `clip_from_view` (`core/camera.hpp`) | camera view → clip (off-axis capable) | 2 | `tests/test_camera.cpp` |
| `projection_frame_from_view` / `ewa_rotation_from_view` (`renderer/`) | view → EWA projection frame (y-down, +z fwd) | 2 | `tests/test_covariance.cpp`, `tests/test_ewa_frame.cpp` |

## 4. Subsystems

- **core/** — math, camera, frustum, timing, logging. *(logging: Phase 0; rest TBD)*
- **loader/** — binary PLY parsing (INRIA 3DGS), SH degree detection 0–3, SoA buffers with
  load-time activations. Layout: `docs/splat-memory-layout.md`. *(Phase 1; .spz/.splat TBD)*
- **renderer/** — CUDA pipeline: frustum cull → 2D covariance projection → 16×16 tile binning
  → per-tile radix sort by depth → front-to-back alpha blend. SH evaluated per splat per frame
  from the **physical camera position**. *(TBD Phase 2)*
- **tracking/** — FreeD UDP listener, validation/stats, pose ring buffer, prediction, lens
  model. *(TBD Phase 3)*
- **output/ndi/** — async GPU readback (double/triple-buffered), software frame pacer,
  timecode stamping. *(TBD Phase 5)*
- **output/st2110/** — Rivermax ST 2110-20 sender, PTP/RTP timestamping, GPUDirect when
  available, SDP generation. *(TBD Phase 6)*
- **app/** — main loop, TOML config, CLI, headless/preview modes.

## 5. Timing Model *(TBD — Phase 5/6)*

- Monotonic clock (`t_mono_us`) is the process-wide timebase; all log records carry it.
- Frame counter advances once per render-loop iteration.
- Phase B: frame start aligned to PTP epoch → RTP timestamps (ST 2059-2).

## 6. Open Questions

- Confirm FreeD position units/axis order as emitted by StarTracker firmware on stage (Phase 3).
- D3D12 vs OpenGL interop on the Windows render nodes (Phase 2 ADR).
- H9 chain pixel format expectations for 2110 (YCbCr-4:2:2 10-bit assumed; confirm before Phase 6).
