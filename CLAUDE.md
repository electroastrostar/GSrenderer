# CLAUDE.md — Project Guidance

Standalone real-time 3D Gaussian Splatting renderer for ICVFX: loads 3DGS assets with full
spherical harmonics, renders from a FreeD-tracked camera with an off-axis frustum, and streams
to UE5 nDisplay (Phase A: NDI; Phase B: SMPTE 2110 via Rivermax).

**Read `SPLATCAST_PLAN.md` first.** Work phases in order; follow §7 Working Agreements
(commit per task, keep `PROGRESS.md` current, PR per phase). On session resume, follow §7.2:
read `PROGRESS.md` and recent git log before doing anything else.

## Build Commands

```bash
# Configure (Debug or Release)
cmake -S . -B build/debug   -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build/release -j

# Run tests
ctest --test-dir build/release --output-on-failure

# Run the app
./build/release/src/app/splatcast
```

- CUDA is **detected, not required**: on machines without `nvcc` (e.g. CI containers) the build
  proceeds CPU-only with a warning. On the A6000 dev/render machines CUDA targets are enabled
  automatically (arch 86, Ampere). The rasterization core (Phase 2+) requires CUDA.
- `-DGSR_BUILD_TESTS=OFF` disables tests (default ON).
- Dependencies (spdlog, Catch2) resolve via CMake FetchContent, pinned to exact tags; a network
  connection is needed on first configure only.

## Coding Standards

- **C++20**, CUDA C++17 for `.cu` files. No compiler extensions.
- Warnings: `-Wall -Wextra -Wpedantic` on our targets (`/W4` on MSVC); keep the build
  warning-clean.
- Namespace: everything lives under `gsr::`, one nested namespace per subsystem
  (`gsr::core`, `gsr::tracking`, …).
- Naming: `snake_case` for functions/variables/files, `PascalCase` for types,
  `kCamelCase` for constants, `UPPER_SNAKE` for macros (avoid macros).
- Errors: exceptions at load/config time; **no exceptions on the per-frame hot path** —
  use status returns/logging there.
- Headers use `#pragma once`. Includes ordered: own header, project, third-party, std.
- Every public function in `src/core/` and `src/tracking/` gets a unit test (plan §7).
- Any deviation from a §4 default in the plan requires an ADR in `docs/decisions/`.

## Coordinate-Convention Rules (the #1 bug source — be rigorous)

`docs/architecture.md` §Coordinate Spaces is the **single source of truth** for handedness,
units, and axis conventions of every space (splat asset, FreeD/StarTracker, renderer camera,
UE5/nDisplay). Rules:

1. **Units are meters, angles are radians** everywhere internally. Convert at I/O boundaries
   only (FreeD wire format, lens files, config), and say so in the parsing function's name or
   doc comment.
2. Every transform between two named spaces is a **named function** in `src/core/`, following
   the convention `X_from_Y(...)` (reads right-to-left: takes a Y-space quantity, returns
   X-space). Example: `render_from_freed(pose)`.
3. **Every such function gets a unit test** with at least one hand-checked numeric fixture
   (a known pose in, a known pose out — derived on paper, not from the code).
4. **Never inline an ad-hoc axis swap, sign flip, or transpose** outside these functions. If a
   matrix needs massaging, that's a missing or wrong convention function — fix it there.
5. Changing any convention requires updating `docs/architecture.md` and the affected tests in
   the same commit.

## Logging

Use `gsr::log::get("<subsystem>")` (wraps spdlog) — never raw `printf`/`cout` outside `main`'s
earliest startup. Every record carries the frame stamp `[frame N][t_mono_us T]`; the main loop
advances the frame counter via `gsr::log::advance_frame()`. Per-subsystem levels via
`gsr::log::set_level("tracking", spdlog::level::debug)`.

## Testing

- Framework: Catch2 v3, discovered via CTest (`catch_discover_tests`).
- Test files live in `tests/`, named `test_<unit>.cpp`.
- Hardware-dependent paths (tracker, NIC) are tested against simulators/mocks
  (`tools/freed_simulator`); integration tests that need real hardware are tagged and marked
  pending — **never fake a pass** (plan §7).
