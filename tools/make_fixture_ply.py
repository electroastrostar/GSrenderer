#!/usr/bin/env python3
"""Regenerate assets/fixtures/cube_deg3.ply — the tiny deterministic test asset.

8 degree-3 splats on the corners of a 2 m cube centred on the origin. The values are chosen
so the asset_inspector output is predictable (see docs/verification/phase-1.md):

    splats:     8
    SH degree:  3 (45 rest + 3 DC coefficients per splat)
    bounds min: [-1.000, -1.000, -1.000]
    bounds max: [ 1.000,  1.000,  1.000]
    memory:     1.8 KiB   (8 splats x 236 bytes = 1888 B)

Run from the repo root:  python3 tools/make_fixture_ply.py
"""
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "assets" / "fixtures" / "cube_deg3.ply"

REST = 45  # degree 3: 15 coeffs x 3 channels
props = ["x", "y", "z", "nx", "ny", "nz"]
props += [f"f_dc_{i}" for i in range(3)]
props += [f"f_rest_{i}" for i in range(REST)]
props += ["opacity"] + [f"scale_{i}" for i in range(3)] + [f"rot_{i}" for i in range(4)]

header = (
    "ply\n"
    "format binary_little_endian 1.0\n"
    "comment deterministic Phase 1 test fixture - regenerate with tools/make_fixture_ply.py\n"
    "element vertex 8\n"
    + "".join(f"property float {p}\n" for p in props)
    + "end_header\n"
)

corners = [(x, y, z) for x in (-1.0, 1.0) for y in (-1.0, 1.0) for z in (-1.0, 1.0)]

rows = bytearray()
for i, (x, y, z) in enumerate(corners):
    row = [x, y, z, 0.0, 0.0, 0.0]                      # position, normals (ignored)
    row += [0.5 * (i + 1), 0.25, -0.25]                 # f_dc RGB
    row += [0.01 * k + 0.1 * i for k in range(REST)]    # f_rest, distinct per splat
    row += [0.0]                                        # opacity logit -> 0.5 after sigmoid
    row += [-2.995732274, -2.995732274, -2.995732274]   # ln(0.05) -> 5 cm scale
    row += [1.0, 0.0, 0.0, 0.0]                         # identity quaternion (w x y z)
    rows += struct.pack(f"<{len(row)}f", *row)

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes(header.encode("ascii") + bytes(rows))
print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")
