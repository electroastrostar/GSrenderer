# Phase 2 — Operator Verification (Core Renderer, Static Camera)

Run these on the **A6000 dev machine** before merging the Phase 2 PR. This phase's code was
**compile-verified with nvcc but has never executed on a GPU** — you are performing the
first real run. If anything crashes or renders garbage, that's exactly what this check
exists to catch: paste the output/screenshot into the PR and it gets fixed in this phase.

**Time required:** ~30 minutes. **You will need:** a real 3DGS `.ply` scene, ideally
**3M+ splats** for the performance gate (step §6). A phone photo of the screen is fine for
reporting visual issues.

> Terminal reminder (same as Phases 0/1): **Windows commands go in the "x64 Native Tools
> Command Prompt for VS 2022"** (Start → type `x64 Native Tools`); Linux in any terminal.
> All steps in one terminal window, in order, from the repo folder
> (`C:\dev\GSrenderer` / `~/dev/GSrenderer`).

## 1. Update the code

```
:: Windows — in C:\dev\GSrenderer
cd C:\dev\GSrenderer
git fetch origin
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
```

```bash
# Linux — in ~/dev/GSrenderer
cd ~/dev/GSrenderer
git fetch origin
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
```

✅ Ends with `up to date` or a fast-forward listing `src/renderer/...` files.

## 2. Configure + build

Delete the old build folder first — this phase changed how the CUDA architecture is set,
and a stale cache would keep the wrong value:

```
:: Windows
rmdir /s /q build
cmake -S . -B build
cmake --build build --config Release -j
```

```bash
# Linux
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

✅ **PASS:** configure prints **`CUDA … found (arch 86) — GPU targets enabled`** (86, not
52), and the build finishes `gsr_renderer`, `splatcast`, `gsr_tests`, `asset_inspector`
with zero warnings/errors for files under `src\`, `tests\`, `tools\`.
❌ **FAIL:** any nvcc or MSVC error — copy the whole error block into the PR (§7).

## 3. Host test suite

```
:: Windows
ctest --test-dir build -C Release --output-on-failure
```

```bash
# Linux
ctest --test-dir build --output-on-failure
```

✅ **PASS:** `100% tests passed, 0 tests failed out of 44`.

## 4. GPU integration tests — first real kernel execution

These are hidden from the default test run because they need a CUDA device; run them
explicitly:

```
:: Windows
build\tests\Release\gsr_tests.exe "[gpu]"
```

```bash
# Linux
./build/tests/gsr_tests "[gpu]"
```

✅ **PASS:** `All tests passed` (2 test cases: frame coverage + SH view-dependence).
❌ **FAIL:** assertion failures or CUDA errors — paste the full output into the PR (§7).

## 5. Visual check — fixture, then a real scene vs SuperSplat

**5a. Fixture sanity** (known-good tiny asset):

```
:: Windows
build\src\app\Release\splatcast.exe assets\fixtures\cube_deg3.ply
```

```bash
# Linux
./build/src/app/splatcast assets/fixtures/cube_deg3.ply
```

✅ **PASS:** a window opens showing **8 small colored blobs arranged as the corners of a
cube** on black. Controls: **WASD** move, **Q/E** down/up, **hold right mouse + drag** to
look, **scroll wheel** = fly speed up/down (shown as `spd` in the title bar), **Shift** =
5× boost, **Esc** quits. Fly around; the blobs must stay glued to their 3D positions (no
swimming/jumping).

**5b. Real scene, compared against a reference viewer.** Use your real asset (example
paths — substitute yours):

```
:: Windows
build\src\app\Release\splatcast.exe C:\assets\myscene.ply
```

```bash
# Linux
./build/src/app/splatcast ~/assets/myscene.ply
```

The camera spawns framing the densest 90% of the scene (far-away background/sky splats are
ignored for framing), and fly speed auto-scales to the scene — if movement still feels too
slow or fast, **scroll the mouse wheel** to adjust it (`spd` in the title bar).

The scene must appear **right side up**: COLMAP-trained assets are y-down and the preview
applies the same default flip SuperSplat does. If a particular asset was exported already
y-up and renders inverted, add `--no-flip` to the command.

Then open **the same `.ply`** in SuperSplat (<https://superspl.at/editor>, drag the file
in) and navigate both to roughly the same viewpoint.

✅ **PASS — all four:**
1. The scene is recognizably **the same scene** with the same colors/detail (small
   differences in tone mapping are OK; missing chunks, wrong colors, or "static noise"
   are not).
2. **Orbit around a shiny/reflective part of the scene: its color/highlight visibly
   shifts with view angle** in our renderer (this is the SH acceptance check).
3. No flicker or splat "popping" while moving slowly.
4. Splats near the screen edges don't smear or explode.

❌ **FAIL:** photo/screenshot of both viewers + the asset name into the PR (§7).

## 6. Performance gate — ≥60 fps at 1080p with a 3M+ splat asset

Use an asset with **at least 3 million splats** (check with
`build\tools\asset_inspector\Release\asset_inspector.exe <file>` — the `splats:` line).
Run at exactly 1080p (the default):

```
:: Windows
build\src\app\Release\splatcast.exe C:\assets\bigscene.ply --width 1920 --height 1080
```

```bash
# Linux
./build/src/app/splatcast ~/assets/bigscene.ply --width 1920 --height 1080
```

The **window title** updates 4×/s with:
`splatcast — <fps> fps | cull+proj <ms> | sort <ms> | blend <ms> | GPU total <ms> | <n> pairs`
(the same numbers log to the terminal once per second).

✅ **PASS:** with the scene filling the window at a typical viewing distance, the title
shows **≥ 60 fps sustained** (vsync is off by default, so this is true renderer speed).
Note the fps and per-stage ms in the PR — they gate what optimizations Phase 2 needs.
❌ **FAIL:** < 60 fps — still check the box list honestly (leave it unchecked), and paste
the title-bar numbers + splat count into the PR; sort/blend timings tell us where to
optimize.

Notes after the first perf report (laptop 3080, 49.3 fps, 9.8M pairs):

- **The formal gate is the A6000** — a laptop-3080 number is advisory (the A6000 has
  ~1.7× its throughput). Record whichever machine you measured on in the PR comment.
- The binning optimization landed after that report (opacity-aware extents + exact
  tile/ellipse overlap) should show **substantially fewer pairs** in the HUD and higher
  fps on the same scene/viewpoint. Also re-glance §5a/5b visuals: splat edges must show
  **no new clipping or popping** (extents for opaque splats actually grew slightly —
  softer edges, not harder).

## 7. Record your results on the PR

Same procedure as before: open the Phase 2 PR in a browser (signed in), **click the
checkboxes** in the description directly (fallback: `⋯ → Edit`, `- [ ]` → `- [x]`,
**Update comment**). For failures, paste terminal output / photos as a PR comment with the
machine + asset used. **Merge only when every box is checked.**

- [ ] Clean build; configure shows `arch 86` (§2)
- [ ] Host tests 44/44 (§3)
- [ ] GPU tests `[gpu]` all pass (§4)
- [ ] Fixture shows 8 stable cube-corner blobs; controls work (§5a)
- [ ] Real scene matches SuperSplat; view-dependent color confirmed on orbit (§5b)
- [ ] ≥60 fps @1080p with 3M+ splats — record fps + stage ms in the PR (§6)
