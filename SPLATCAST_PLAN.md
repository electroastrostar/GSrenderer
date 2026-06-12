# Project Plan: Custom Gaussian Splat Renderer for ICVFX (NDI → SMPTE 2110)

**Version:** v2
**Owner:** James Hughes — Mirage VP Stage
**Purpose of this document:** Implementation plan for Claude Code. Work through phases in order. Each phase has deliverables and acceptance criteria. Do not skip ahead — later phases depend on validated output from earlier ones.

---

## 1. Project Summary

Build a standalone, real-time 3D Gaussian Splatting renderer that:

1. Loads standard 3DGS `.ply` / `.splat` assets with **full spherical harmonics retained** (degree 3, 45 SH color coefficients + DC per splat).
2. Renders from a virtual camera driven by **live camera tracking data** (Mo-Sys StarTracker via FreeD protocol, UDP).
3. Computes an **off-axis (asymmetric) projection frustum** so the output can serve as an inner-frustum texture for nDisplay on a curved LED volume.
4. Streams output **Phase A: via NDI** for prototyping, then **Phase B: via SMPTE 2110 (NVIDIA Rivermax)** for production, genlocked via PTP.
5. Is consumed by **UE5 nDisplay** as a media source on the inner frustum — nDisplay continues to own wall warp, outer frustum, and final output to the Novastar H9 chain.

### Why a custom renderer
- Full control over SH evaluation relative to the *physical tracked camera position* (correct view-dependent response on the volume).
- Lower per-frame latency than in-engine plugin paths.
- Future extensibility: relightable splat variants (normals + BRDF), custom sort strategies, holodeck R&D integration.

---

## 2. Target Environment

| Component | Detail |
|---|---|
| GPU | NVIDIA RTX A6000 (Ampere, 48GB), dev machine + render nodes |
| OS | Windows 11 (render nodes); dev may also build on Linux for tooling |
| Tracking | Mo-Sys StarTracker → FreeD (UDP, typically port 8001) |
| Engine integration | UE 5.x + nDisplay, Switchboard launch |
| Video out Phase A | NDI 6 SDK (Advanced SDK if available, else standard) |
| Video out Phase B | SMPTE ST 2110-20 via NVIDIA Rivermax SDK + ConnectX-6 DX NIC, PTP (ST 2059-2) |
| Sync | House genlock; Quadro Sync II on render nodes |
| Resolution targets | Prototype: 1920×1080 @ 24/25/30. Production: up to 4096×2160 @ 24/25/30 |

---

## 3. Repository Structure (target)

```
/                      (repo root — already initialized)
├── CLAUDE.md              # Claude Code project guidance (create in Phase 0)
├── docs/
│   ├── architecture.md
│   ├── freed-protocol.md
│   └── decisions/         # ADRs: one file per major technical decision
├── src/
│   ├── core/              # math, camera, frustum, timing
│   ├── loader/            # PLY/SPZ/splat loaders, SH data layout
│   ├── renderer/          # GPU rasterizer (sort, project, blend)
│   ├── tracking/          # FreeD UDP listener, pose filter/predictor
│   ├── output/
│   │   ├── ndi/           # Phase A
│   │   └── st2110/        # Phase B (Rivermax)
│   └── app/               # main loop, config, CLI
├── tools/
│   ├── freed_simulator/   # synthetic FreeD packet generator for desk testing
│   └── asset_inspector/   # dump splat counts, SH degree, bounds from a .ply
├── tests/
├── assets/                # small test splats only (large assets stay out of git)
└── configs/               # JSON/TOML run configs (stage geometry, NIC, ports)
```

---

## 4. Technology Decisions (defaults — record changes as ADRs)

- **Language:** C++20. CUDA for the rasterization core.
- **Rasterization core:** Start from the approach of the reference 3DGS CUDA rasterizer / `gsplat` design: frustum cull → project 3D covariance to 2D → tile binning → per-tile depth sort (GPU radix) → alpha-blended splatting. Write our own implementation but it is acceptable to study reference implementations for correctness.
- **Windowing/context (debug view):** GLFW + a simple preview window. Headless mode for production.
- **Interop:** Render in CUDA → CUDA/OpenGL or CUDA/D3D12 interop into a texture → readback or direct GPU send to output module. Avoid CPU round-trips where the SDK allows.
- **Config:** Single TOML config file per run: asset path, tracking source, lens intrinsics, output mode, frame rate, latency offset.
- **SH policy:** Always load and evaluate full SH up to the degree present in the asset. CLI flag to clamp degree (0/1/2/3) for perf testing.
- **Coordinate conventions:** Define ONE document (`docs/architecture.md`) fixing handedness, units (meters), and axis conventions for: splat asset space, FreeD/StarTracker space, renderer camera space, UE5/nDisplay space. Every transform between spaces gets a named function with a unit test. This is the #1 source of bugs in this class of project — be rigorous.

---

## 5. Phases

### Phase 0 — Project Scaffolding
**Goal:** Buildable empty project with CI hygiene.

Tasks:
1. Create `CLAUDE.md` with build commands, coding standards, and the coordinate-convention rules.
2. CMake project, C++20, CUDA toolchain detection, Debug/Release configs.
3. `docs/architecture.md` skeleton with the data-flow diagram (tracker → pose filter → frustum → render → output).
4. Logging (spdlog or similar) with per-subsystem levels and a frame-stamped log format (`[frame N][t_mono_us]`).
5. Unit test framework (Catch2 or GoogleTest) wired into CMake.

**Acceptance:** `cmake --build` succeeds clean on the dev machine; a hello-world test passes.

---

### Phase 1 — Asset Loading with Full SH
**Goal:** Load standard 3DGS assets into GPU-ready buffers, SH intact.

Tasks:
1. Binary PLY loader for INRIA-format 3DGS output: position, scale, rotation (quat), opacity, `f_dc_*`, `f_rest_*` (SH coefficients).
2. Detect SH degree from property count; support degrees 0–3.
3. Pack into structure-of-arrays GPU buffers. Document the memory layout in `docs/`.
4. `tools/asset_inspector`: CLI that prints splat count, SH degree, bounding box, memory footprint estimate.
5. Optional (stretch): `.spz` and `.splat` (antimatter15) format support behind the same loader interface.

**Acceptance:** Inspector output matches known values for a reference asset; loader unit tests cover degree 0–3 assets and malformed-file rejection.

---

### Phase 2 — Core Renderer (Static Camera)
**Goal:** Correct, performant splat rendering to a preview window with a free-fly debug camera.

Tasks:
1. CUDA pipeline: frustum cull → 2D covariance projection → 16×16 tile binning → per-tile radix sort by view depth → front-to-back alpha blending.
2. SH evaluation per splat per frame using the view direction from camera position to splat center.
3. CUDA→GL (or D3D12) interop to display in the preview window; free-fly WASD camera for visual verification.
4. Frame timing HUD: ms per stage (cull/sort/blend), total frame time, splat count rendered.
5. Perf gate: profile with a 3–6M splat asset.

**Acceptance:** Visually matches a reference viewer (e.g., the INRIA SIBR viewer or SuperSplat) on the same asset within reasonable tolerance; ≥60 fps at 1080p with a 3M splat asset on the A6000; view-dependent color visibly responds to camera orbit (confirms SH evaluation).

---

### Phase 3 — FreeD Tracking Ingest + Pose Filtering
**Goal:** Virtual camera driven by live StarTracker data.

Tasks:
1. UDP listener parsing FreeD D1 packets: pan/tilt/roll, X/Y/Z, zoom/focus raw values. Document the packet layout in `docs/freed-protocol.md`.
2. Checksum validation, dropped-packet stats, packet-rate monitor.
3. `tools/freed_simulator`: generates synthetic FreeD streams (static pose, orbit, handheld-noise profile) so all of this is testable without the stage.
4. Pose filter: timestamped ring buffer + configurable prediction (start with linear velocity extrapolation; latency offset in ms set via config). One-euro filter optional for jitter.
5. Lens model: map FreeD zoom/focus raw values through a lens calibration table (CSV/JSON; format compatible with what Mo-Sys exports) to focal length / FOV. If no table is present, allow fixed intrinsics from config.
6. Transform FreeD space → renderer camera space using the convention functions from Phase 0.

**Acceptance:** With the simulator running an orbit profile, the preview renders a smooth orbit with no visible stutter; latency offset measurably shifts the predicted pose; unit tests on packet parsing against captured/synthetic byte fixtures.

---

### Phase 4 — Off-Axis Frustum for the Volume
**Goal:** Output frame is a correct inner-frustum render usable by nDisplay.

Tasks:
1. Implement asymmetric (off-axis) projection matrix construction from: physical camera pose, lens intrinsics, and a target projection plane/region.
2. Two operating modes:
   - **Mode A (recommended first):** Render a standard perspective view matching the physical camera's lens exactly — nDisplay treats it as the inner-frustum texture and handles placement/warp. This is the simpler contract: our output = "what the physical camera lens sees" of the splat scene.
   - **Mode B (later, optional):** Renderer computes the frustum overscan region itself (configurable overscan %) to match nDisplay inner-frustum overscan settings.
3. SH evaluation must use the **physical camera position** as the view origin (it already does if Phase 2/3 are correct — add an explicit test).
4. Config entries for stage origin offset (tracker origin → stage origin → UE world origin alignment).

**Acceptance:** Given an identical camera pose fed to both this renderer and a UE5 scene with matching intrinsics, rendered perspective lines up (verify with a checkerboard/grid splat or fiducial asset); overscan percentage verified by pixel measurement.

---

### Phase 5 — NDI Output (Prototype Streaming)
**Goal:** nDisplay consumes the renderer's output live.

Tasks:
1. NDI sender module: BGRA (or UYVY for bandwidth) frames at project frame rate, with timecode stamped from the monotonic clock.
2. GPU→NDI path: use async readback (double/triple-buffered PBO or CUDA pinned memory) so readback doesn't stall the render loop. Measure and log readback cost.
3. Frame pacing: render loop locked to target rate (24/25/30) with a software pacer; log late frames.
4. UE5 side (document, don't code in this repo): NDI Media plugin → Media Texture → nDisplay inner-frustum media input. Write the setup steps in `docs/ue5-ndi-setup.md` including the ICVFX camera media-sharing configuration.
5. End-to-end latency measurement procedure: flash-frame test (renderer flashes a frame on keypress, measure arrival at UE viewport with a high-speed phone camera). Document results.

**Acceptance:** UE5 on a second machine displays the splat stream inside an nDisplay config; sustained target frame rate ≥30 min without drift/leak; measured end-to-end latency documented; tracked camera motion (simulator or real) shows correct parallax on the wall.

**Milestone: PROTOTYPE COMPLETE — stage test at Mirage.**

---

### Phase 6 — SMPTE 2110 Output (Production Streaming)
**Goal:** Replace NDI with uncompressed, PTP-locked ST 2110-20 essence.

Tasks:
1. Rivermax SDK integration: ST 2110-20 sender, YCbCr-4:2:2 10-bit (match the H9 chain's expectations; confirm before implementing), 1080p then UHD.
2. PTP (ST 2059-2) clock discipline: read system PTP clock (NIC-disciplined); align frame start to RTP timestamps derived from PTP epoch.
3. GPUDirect path if available (CUDA buffer → NIC without host copy); otherwise pinned-memory staging.
4. SDP file generation for the receiver side.
5. UE5 side documentation: Rivermax Media plugin input, nDisplay media source config, PTP/genlock relationship with Quadro Sync (`docs/ue5-2110-setup.md`).
6. Soak test: 8-hour run, monitor for packet pacing violations (use NIC counters / `rivermax` stats), dropped frames, thermal behavior.

**Acceptance:** UE5 receives 2110 stream via Rivermax plugin inside nDisplay; stream passes basic ST 2110-20 timing analysis; zero dropped frames over soak test; latency ≤ NDI path measurement.

---

### Phase 7 — Hardening & Operations
**Goal:** Stage-ready tool, not a demo.

Tasks:
1. Headless service mode: no preview window, starts from config, OSC or HTTP control endpoint for load-asset / start / stop / set-latency-offset.
2. Crash resilience: watchdog that restarts the send pipeline on failure; asset hot-reload.
3. Telemetry: Prometheus-style metrics endpoint or rolling CSV (frame time, sort time, packet rate, late frames) for the Mirage ops dashboard.
4. Write SOP draft for stage operation (startup order: PTP lock → tracker verify → renderer → nDisplay) in Mirage SOP format for later inclusion in the SOP library.
5. Multi-asset scene support (multiple splat files with individual transforms) — stretch.

**Acceptance:** Operator can run a full session from config + control endpoint without touching code; SOP draft reviewed.

---

## 6. Risks & Watch Items

1. **Coordinate-space mismatches** (tracker vs. splat vs. UE). Mitigate with the convention doc + per-transform unit tests from day one.
2. **Readback stalls in the NDI phase** masking true renderer performance — always profile render and readback separately.
3. **Sort cost at high splat counts** — if 4K/6M splats misses budget, options: SH degree clamp at distance, splat LOD/culling by projected size, sort every N frames with refinement.
4. **Rivermax licensing + ConnectX NIC procurement lead time** — order hardware during Phase 2, not Phase 6.
5. **FreeD zoom/focus mapping** without a proper lens file gives wrong FOV — schedule a lens calibration pass on the stage before the Phase 5 milestone test.
6. **NDI frame timing is free-running** — do not chase sync bugs in Phase 5 that are inherent to NDI; note them and defer to Phase 6.

---

## 7. Working Agreements for Claude Code

- One phase per branch; PR per phase with the acceptance criteria checked off in the description.
- Every public function in `core/` and `tracking/` gets a unit test.
- Any deviation from a default in §4 requires an ADR in `docs/decisions/`.
- Version tags: `v0.x` during phases 0–4, `v1.0` at Phase 5 milestone, `v2.0` at Phase 6 completion.
- When a hardware-dependent task can't be tested (no NIC, no tracker), build against the simulator/mock and mark the integration test as pending — never fake a pass.

### 7.1 Commit Cadence & Progress Tracking *(added v2)*

- Commit and push after **every completed task**, not just at phase end. Commit messages: `phaseN: <task summary>`. The remote branch is the source of truth — never let more than one task's work sit unpushed.
- Maintain `PROGRESS.md` at the repo root with three sections: **Done** (task-level, with commit hashes), **In Progress** (current task + exact next step), **Next** (remaining tasks this phase). Update it and push **before** starting each new task, and again immediately before any risky/long-running operation.

### 7.2 Session Resume Protocol *(added v2)*

If a session is interrupted (usage limit, disconnect, crash) and later resumed — or a fresh session picks up the work:

1. Read `PROGRESS.md` and `git log --oneline -15` on the current phase branch **before doing anything else**.
2. Trust the repo over conversation memory. Do **not** redo tasks listed as Done; do not re-plan the phase.
3. Verify the working tree builds (`cmake --build`) before continuing. If it doesn't, fixing the build is the first task.
4. Resume from the "exact next step" recorded in `PROGRESS.md` → In Progress.
