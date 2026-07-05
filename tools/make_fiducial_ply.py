#!/usr/bin/env python3
"""Regenerate assets/fixtures/grid_fiducial.ply — the Phase 4 line-up asset.

A 4x4 m grid wall (0.5 m cells, opaque grey splats every 5 cm) with colored corner
markers and a white center marker, built in ASSET space (COLMAP y-down) so it appears
upright through the default world_from_asset flip:

  world top-left = RED, top-right = GREEN, bottom-left = BLUE, bottom-right = YELLOW.

Used for: UE5 perspective line-up (same pose + intrinsics in both renderers must overlay)
and overscan pixel measurement (grid cell size in px at a known distance/FOV is exactly
computable). Degree-0 splats keep the file tiny.

Run from the repo root:  python3 tools/make_fiducial_ply.py
"""
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "assets" / "fixtures" / "grid_fiducial.ply"

C0 = 0.28209479177387814
def dc(rgb):  # color -> DC SH coefficient (color = 0.5 + C0*dc)
    return [(c - 0.5) / C0 for c in rgb]

HALF = 2.0          # wall spans [-2, 2] m
CELL = 0.5          # grid cell size
STEP = 0.05         # splat spacing along lines
SCALE = 0.018       # splat radius ~1.8 cm (log encoded)
OPACITY_LOGIT = 6.0 # sigmoid(6) ~ 0.9975

splats = []  # (x, y, z, rgb, scale)

def add(x, y, rgb, scale=SCALE):
    splats.append((x, y, 0.0, rgb, scale))

grey = (0.85, 0.85, 0.85)
n_lines = int(2 * HALF / CELL) + 1  # 9
n_pts = int(2 * HALF / STEP) + 1    # 81
for i in range(n_lines):
    coord = -HALF + i * CELL
    for j in range(n_pts):
        t = -HALF + j * STEP
        add(t, coord, grey)          # horizontal line (constant asset y)
        add(coord, t, grey)          # vertical line

# Corner + center markers (asset y-down: world top = asset -y). 10 cm markers.
add(-HALF, -HALF, (1.0, 0.1, 0.1), 0.10)  # world top-left  RED
add(+HALF, -HALF, (0.1, 1.0, 0.1), 0.10)  # world top-right GREEN
add(-HALF, +HALF, (0.1, 0.1, 1.0), 0.10)  # world bottom-left BLUE
add(+HALF, +HALF, (1.0, 1.0, 0.1), 0.10)  # world bottom-right YELLOW
add(0.0, 0.0, (1.0, 1.0, 1.0), 0.10)      # center WHITE

props = ["x", "y", "z", "nx", "ny", "nz", "f_dc_0", "f_dc_1", "f_dc_2",
         "opacity", "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3"]
header = (
    "ply\nformat binary_little_endian 1.0\n"
    "comment Phase 4 line-up fiducial - regenerate with tools/make_fiducial_ply.py\n"
    f"element vertex {len(splats)}\n"
    + "".join(f"property float {p}\n" for p in props)
    + "end_header\n"
)

import math
rows = bytearray()
log_scale = math.log(SCALE)
for (x, y, z, rgb, scale) in splats:
    row = [x, y, z, 0.0, 0.0, 0.0]
    row += dc(rgb)
    row += [OPACITY_LOGIT, math.log(scale), math.log(scale), math.log(scale)]
    row += [1.0, 0.0, 0.0, 0.0]
    rows += struct.pack(f"<{len(row)}f", *row)

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes(header.encode("ascii") + bytes(rows))
print(f"wrote {OUT}: {len(splats)} splats, {OUT.stat().st_size} bytes")
