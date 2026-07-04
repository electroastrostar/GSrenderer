# Phase 4 — Operator Verification (Off-Axis Frustum / Inner-Frustum Contract)

Run on your GPU machine (3080 laptop fine). Uses the new **grid fiducial asset** and the
simulator — plus an **optional UE5 section** for the full line-up acceptance when you have
a UE machine (deferred to the stage session otherwise, like the tracker sign check).

**Time required:** ~20 minutes (§1–§5; §6 UE5 extra).

> Same terminal rules as before (x64 Native Tools prompt, repo folder). Two terminals for
> the tracked steps.

## 1. Update + build + tests

```
:: Windows — terminal 1, in C:\dev\GSrenderer
cd C:\dev\GSrenderer
git fetch origin
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

```bash
# Linux — terminal 1
cd ~/dev/GSrenderer
git fetch origin && git checkout claude/phase-0-scaffolding-qf5mw4 && git pull origin claude/phase-0-scaffolding-qf5mw4
cmake --build build -j && ctest --test-dir build --output-on-failure
```

✅ **PASS:** zero warnings/errors for our files; `100% tests passed, 0 tests failed out of 87`.

## 2. GPU tests (incl. the new SH-origin proof)

```
:: Windows
build\tests\Release\gsr_tests.exe "[gpu]"
```

```bash
# Linux
./build/tests/gsr_tests "[gpu]"
```

✅ **PASS:** `All tests passed` (3 cases — the new one proves SH evaluates from the
physical camera position field).

## 3. Fiducial sanity + config file

```
:: Windows
build\src\app\Release\splatcast.exe --config configs\example_run.toml assets\fixtures\grid_fiducial.ply
```

```bash
# Linux
./build/src/app/splatcast --config configs/example_run.toml assets/fixtures/grid_fiducial.ply
```

✅ **PASS:** a **4×4 m grey grid wall** (8×8 cells of 0.5 m) with corner markers
**RED top-left, GREEN top-right, BLUE bottom-left, YELLOW bottom-right**, white center —
upright and square (markers in the right corners = coordinate conventions verified
end-to-end). The config file loads without error (the log shows the asset from the CLI
overriding the config's asset).

## 4. Overscan pixel measurement (plan acceptance)

Render the fiducial twice **at a fixed camera** (fly camera spawn is deterministic — do
NOT touch the controls; just launch, screenshot, close):

```
:: Windows — shot A (base), then shot B (10% overscan)
build\src\app\Release\splatcast.exe assets\fixtures\grid_fiducial.ply --width 1280 --height 720
build\src\app\Release\splatcast.exe assets\fixtures\grid_fiducial.ply --width 1280 --height 720 --overscan 10
```

```bash
# Linux
./build/src/app/splatcast assets/fixtures/grid_fiducial.ply --width 1280 --height 720
./build/src/app/splatcast assets/fixtures/grid_fiducial.ply --width 1280 --height 720 --overscan 10
```

Screenshot each window (Win+Shift+S / your tool) and open both in any image viewer with
a pixel readout (even Paint's status bar).

✅ **PASS — all three:**
1. Shot B's window/image is **1408×792** (1280×720 + 10%; startup log prints
   `overscan 10.0%: rendering 1408x792 (base 1280x720 is the center crop)`).
2. Shot B shows **more scene** at the edges; the grid appears the **same size** in both
   (a grid cell measures the same number of pixels in A and B — that's the "rays
   unchanged" contract).
3. Crop shot B by exactly **64 px left/right and 36 px top/bottom** → it matches shot A
   (marker positions agree within a pixel or two).

❌ **FAIL:** different grid-cell pixel size between shots, or the crop not matching —
paste both screenshots.

## 5. Stage alignment shifts the tracked camera

Terminal 2: `freed_simulator --port 8001 --profile static --radius 4` (static profile).
Terminal 1, run twice and compare (screenshot each):

```
:: Windows
build\src\app\Release\splatcast.exe assets\fixtures\grid_fiducial.ply --freed-port 8001
build\src\app\Release\splatcast.exe assets\fixtures\grid_fiducial.ply --freed-port 8001 --stage-offset 0,0,2 --stage-yaw-deg 15
```

✅ **PASS:** run 2's viewpoint is visibly **2 m closer** (grid larger) and **rotated 15°**
(grid skewed/turned) vs run 1 — the same tracker data, re-based by the stage alignment.
❌ **FAIL:** no difference between runs.

## 6. OPTIONAL — UE5 line-up (full plan acceptance; needs a UE machine)

When you have UE5 available (stage session is fine):

1. Import a 4×4 m plane with an 8-cell checkerboard/grid material at the UE origin,
   facing +X, center at 1.5 m height offset 0 — or recreate the fiducial's geometry.
2. Create a CineCamera with intrinsics matching the renderer run: filmback height =
   `--sensor-height-mm` (default 24), focal length = the lens table value (or from
   `fov_deg`: focal = 12 / tan(fov/2) mm for a 24 mm filmback).
3. Drive both from the simulator (UE: LiveLink FreeD source, same port; splatcast §5
   command) or place both cameras at the same fixed pose.
4. Screenshot both viewports and overlay them (50% opacity in any editor).

✅ Grid lines and corner markers coincide within a few pixels across the frame — centers
AND corners (corner agreement is what verifies the off-axis/edge geometry).
Mirrored or offset → note which axis; adjust `--stage-yaw-deg`/`--stage-offset` first;
if a mirror can't be fixed by yaw/offset, report it (convention bug, not calibration).
**Not a merge gate** — record results when you get UE time.

## 7. Record results on the PR

Click the checkboxes on the Phase 4 PR (fallback `⋯ → Edit`); paste screenshots/output
for failures. Merge when §1–§5 boxes are green (§6 records separately).

- [ ] Build + 87/87 tests; `[gpu]` 3/3 (§1–2)
- [ ] Fiducial upright with correct corner colors; config file loads (§3)
- [ ] Overscan: 1408×792 output, same grid-cell pixel size, crop matches base (§4)
- [ ] Stage offset/yaw visibly re-bases the tracked camera (§5)
