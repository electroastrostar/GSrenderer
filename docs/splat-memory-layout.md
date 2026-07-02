# Splat Memory Layout (`gsr::loader::SplatData`)

Defined by `src/loader/splat_data.hpp` (Phase 1). This is the CPU-side structure-of-arrays
representation that Phase 2 uploads verbatim to GPU buffers — one `cudaMemcpy` per array, no
repacking planned (if Phase 2 profiling demands a different device layout, that conversion
lives in `src/renderer/` and this doc gets a section on it).

## Why SoA

The CUDA pipeline stages touch different subsets of attributes (cull: position only;
projection: position+scale+rotation; shading: SH). Separate tightly-packed arrays give each
stage coalesced loads without dragging unused attributes through the cache.

## Arrays

For `N = count` splats. All elements are `float` (4 bytes), little-endian, tightly packed.

| Array | Elements | Per-splat layout | Values |
|---|---|---|---|
| `position` | 3·N | `[x y z]` | meters, **asset space** (see `architecture.md` §Coordinate Spaces) |
| `scale`    | 3·N | `[sx sy sz]` | linear ellipsoid extents; `exp()` already applied to the file's log-scale |
| `rotation` | 4·N | `[w x y z]` | unit quaternion; normalized at load (degenerate → identity) |
| `opacity`  | 1·N | `[α]` | `[0,1]`; `sigmoid()` already applied to the file's logit |
| `sh_dc`    | 3·N | `[R G B]` | raw DC SH coefficients (**not** color; see below) |
| `sh_rest`  | 3·R·N | see below | raw higher-order SH coefficients |

`sh_degree` ∈ 0..3 and `R = rest_coeffs_for_degree(sh_degree)` = (degree+1)² − 1
(0, 3, 8, 15), so `sh_rest` holds 0/9/24/45 floats per splat.

Splat `i` owns `position[3i .. 3i+2]`, `rotation[4i .. 4i+3]`, `sh_rest[3R·i .. 3R·(i+1)-1]`,
etc. Total bytes = `byte_size(data)`; per-splat bytes by degree: 56 (deg 0), 92 (1), 152 (2),
236 (3).

## `sh_rest` channel-major ordering (matches the PLY file)

Within one splat, `sh_rest` keeps the INRIA file order `f_rest_0 .. f_rest_3R-1`, which is
**channel-major**: all R red coefficients, then all R green, then all R blue.

```
sh_rest[3R·i + c·R + k]   c = channel (0=R,1=G,2=B),  k = SH coeff index 1..R (0-based k=coeff k+1)
```

Degree-3 example (R=15): indices 0–14 = red coeffs Y₁₋₁…Y₃₃, 15–29 = green, 30–44 = blue.

## Color reconstruction (for Phase 2 shading)

```
color(view_dir) = 0.5 + kShC0 · sh_dc  +  Σₖ SH_basisₖ(view_dir) · sh_rest[k]   (per channel)
```

`kShC0 = 0.28209479177387814` (Y₀⁰) is exported by `splat_data.hpp`. The view direction is
from the **physical camera position** to the splat center (plan §Phase 4 point 3).

## What is intentionally NOT stored

- **Normals** (`nx/ny/nz` in INRIA files): always zero in practice; skipped at parse time.
- **Precomputed 3D covariance**: derived per frame on GPU from `scale`+`rotation` (cheaper
  than storing 6·N floats and required anyway for anisotropic projection).
- **Activation-free raw values**: `exp`/`sigmoid` are applied once at load so the hot path
  never does it. If a future tool needs to round-trip PLYs losslessly, it must invert these
  (`log(scale)`, `logit(opacity)`) — noted here so nobody assumes raw file values.
