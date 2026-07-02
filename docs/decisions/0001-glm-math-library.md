# 0001 — Use GLM as the host/device math library

- **Date:** 2026-07-02
- **Status:** accepted

## Context

Phase 2 needs vectors, matrices and quaternions on both the CPU (camera, tests, tooling) and
in CUDA kernels. Plan §4 fixes the language and rasterizer approach but names no math
library. Options: hand-rolled types (full control, but we'd re-derive and re-test
projection/quaternion math that has standard implementations) or GLM (header-only,
GL-convention matrices, works in CUDA device code, pinned via FetchContent like our other
deps).

## Decision

GLM (pinned tag), linked as `glm::glm` into `gsr_core` and available to `.cu` files.
Column-major, right-handed, OpenGL clip conventions — matching our renderer camera space
(right-handed, Y-up, −Z forward) and the GL interop path.

The coordinate-convention rules in `CLAUDE.md` are unchanged: GLM is plumbing, and every
transform between *named spaces* is still a `X_from_Y` function in `src/core/` with a
hand-checked unit test. GLM calls never appear as ad-hoc math at call sites of another space.

## Consequences

- Less bespoke math to maintain/test; camera & frustum code reads as standard GL.
- One more pinned dependency (header-only; no build-time cost beyond includes).
- CUDA kernels may still use raw `float3`/small inline helpers where GLM types would fight
  coalescing; the boundary is the kernel argument structs, documented in `src/renderer/`.
