# Phase 3 — Operator Verification (FreeD Tracking Ingest + Pose Filtering)

Run these on your machine with a GPU (the laptop 3080 is fine — nothing here is
perf-gated). **No tracker is needed**: every step uses `tools/freed_simulator`, which
generates real FreeD packets over UDP exactly as a Mo-Sys StarTracker would send them.
The final section is an *optional* bonus using a phone as a live tracker.

**Time required:** ~20 minutes.

> Terminal reminder: **Windows commands go in the "x64 Native Tools Command Prompt for
> VS 2022"**; all steps in one window from the repo folder (`C:\dev\GSrenderer` /
> `~/dev/GSrenderer`). This phase needs **two** terminals at once (renderer + simulator)
> — open a second one the same way when §3 says so.

## 1. Update + build

```
:: Windows — terminal 1, in C:\dev\GSrenderer
cd C:\dev\GSrenderer
git fetch origin
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
cmake --build build --config Release -j
```

```bash
# Linux — terminal 1, in ~/dev/GSrenderer
cd ~/dev/GSrenderer
git fetch origin
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
cmake --build build -j
```

✅ **PASS:** builds `gsr_tracking`, `freed_simulator`, and `splatcast` with zero
warnings/errors for `src\`/`tests\`/`tools\` files.

## 2. Run the tests

```
:: Windows — terminal 1
ctest --test-dir build -C Release --output-on-failure
```

```bash
# Linux — terminal 1
ctest --test-dir build --output-on-failure
```

✅ **PASS:** `100% tests passed, 0 tests failed out of 77` (24 new: FreeD packet codec
byte fixtures, UDP loopback, pose prediction, lens table, `render_from_freed`).

## 3. Tracked orbit — the main acceptance check

**Terminal 1** — start the renderer with the cube fixture, listening for tracking:

```
:: Windows
build\src\app\Release\splatcast.exe assets\fixtures\cube_deg3.ply --freed-port 8001
```

```bash
# Linux
./build/src/app/splatcast assets/fixtures/cube_deg3.ply --freed-port 8001
```

The window opens with the fly camera (no tracking yet); the log shows
`tracked-camera mode: FreeD on UDP 8001`.

**Terminal 2** (open a new terminal of the same kind, `cd` to the repo folder) — start
the simulator orbiting at 4 m radius:

```
:: Windows
build\tools\freed_simulator\Release\freed_simulator.exe --port 8001 --profile orbit --radius 4 --height 1.5
```

```bash
# Linux
./build/tools/freed_simulator/freed_simulator --port 8001 --profile orbit --radius 4 --height 1.5
```

✅ **PASS — all four:**
1. The moment the simulator starts, the camera **snaps to the orbit** and circles the
   cube, always facing it, completing a revolution every ~12 s.
2. The motion is **smooth** — no stutter, no jumps, no frozen frames (plan acceptance:
   "smooth orbit with no visible stutter").
3. The title bar shows `trk 50 Hz` (±3) with `ok:` climbing and `rej:0`.
4. Stop the simulator (Ctrl+C in terminal 2): the camera freezes in place (stale-data
   hold) and fly controls work again after ~0.2 s.

❌ **FAIL:** stutter/jumping (note the fps + trk numbers), the camera facing away from
the cube or orbiting off-axis (that's a `render_from_freed` sign bug — say which way it
faces), or `rej:` counting up (packet corruption — paste both terminals' output).

## 4. Latency offset shifts the pose (plan acceptance)

Keep the simulator running. Quit the renderer (Esc) and relaunch it with a large,
obvious prediction offset:

```
:: Windows
build\src\app\Release\splatcast.exe assets\fixtures\cube_deg3.ply --freed-port 8001 --latency-ms 400
```

```bash
# Linux
./build/src/app/splatcast assets/fixtures/cube_deg3.ply --freed-port 8001 --latency-ms 400
```

✅ **PASS:** the orbit still runs smoothly but the camera rides **visibly ahead** of
where it just was at the same clock time — with a 12 s revolution, 400 ms is a clear
~12° lead. Try `--latency-ms 0` vs `--latency-ms 400` back-to-back; the pose shift must
be obvious. (Real stage values will be 10–80 ms; 400 is for visibility.)
❌ **FAIL:** no visible difference between 0 and 400 ms.

## 5. Handheld profile + lens table

Restart the simulator in handheld mode (terminal 2, Ctrl+C then):

```
:: Windows
build\tools\freed_simulator\Release\freed_simulator.exe --port 8001 --profile handheld --radius 4
```

```bash
# Linux
./build/tools/freed_simulator/freed_simulator --port 8001 --profile handheld --radius 4
```

And the renderer with the example lens table (terminal 1):

```
:: Windows
build\src\app\Release\splatcast.exe assets\fixtures\cube_deg3.ply --freed-port 8001 --lens-file configs\example_lens.csv
```

```bash
# Linux
./build/src/app/splatcast assets/fixtures/cube_deg3.ply --freed-port 8001 --lens-file configs/example_lens.csv
```

✅ **PASS:** the view shows a gentle handheld sway/tremor (position and angles), and the
log line at startup says the lens table loaded. The simulator's default zoom (524288)
maps to 50 mm through the example table — the cube should look noticeably tighter than
the 60° default FOV run in §3.
❌ **FAIL:** no sway, a crash on `--lens-file`, or no FOV change vs §3.

## 6. OPTIONAL — phone as a live FreeD tracker

If you have an iPhone/iPad: the App Store app **"VirtualProductionCamera"** streams
ARKit tracking as FreeD over UDP. Phone and PC must be on the same network:

1. Find your PC's IP: `ipconfig` (Windows, "IPv4 Address") / `ip addr` (Linux).
2. In the app, set the target IP to your PC and port to `8001`, protocol FreeD, and start.
3. Run the renderer exactly as in §3 (no simulator this time).

✅ Walking around with the phone should move the camera through the splat scene with
correct parallax. Axis signs may differ from the StarTracker's — if motion is mirrored,
note which axis; that calibration belongs to the stage session, not this phase. This
section is informational only — **not** a merge gate.

## 7. Record your results on the PR

As before: open the Phase 3 PR, **click the checkboxes** in the description (fallback:
`⋯ → Edit`). Paste terminal output/video for failures as PR comments. Merge when the
required boxes (not §6) are green.

- [ ] Build warning-clean; `ctest` 77/77 (§1–2)
- [ ] Simulator orbit: camera circles the cube facing it, smooth, `trk ~50 Hz`, `rej:0`;
      freeze-then-fly on simulator stop (§3)
- [ ] `--latency-ms 400` visibly leads the 0 ms pose (§4)
- [ ] Handheld sway visible; lens table loads and tightens the FOV (§5)
