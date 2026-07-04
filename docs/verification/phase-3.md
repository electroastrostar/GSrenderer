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

✅ **PASS:** `100% tests passed, 0 tests failed out of 78` (25 new: FreeD packet codec
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

✅ **PASS — all five:**
1. The moment the simulator starts, the camera **snaps to the orbit** and circles the
   cube, always facing it, completing a revolution every ~12 s.
2. The motion is **smooth** — no stutter, no jumps, no frozen frames (plan acceptance:
   "smooth orbit with no visible stutter").
3. The title bar shows `trk 50 Hz` (±3) with `ok:` climbing and `rej:0`.
4. Let the orbit run for **at least two full revolutions** (~25 s) — it must stay locked
   on the cube the whole time (this specifically covers the ±180° pan wrap).
5. Stop the simulator (Ctrl+C in terminal 2): after ~0.5 s a log line says
   `tracking stale — fly camera resumes at the last tracked pose`, and WASD/mouse
   control works again from right where the camera stopped.

❌ **FAIL:** stutter/jumping (note the fps + trk numbers), the camera facing away from
the cube or orbiting off-axis (that's a `render_from_freed` sign bug — say which way it
faces), or `rej:` counting up (packet corruption — paste both terminals' output).

## 4. Latency offset shifts the pose (plan acceptance)

No restart needed — the offset is **live-adjustable** so you compare against what you're
looking at, not against memory. With the §3 orbit still running and the renderer window
focused:

1. Watch the title bar: it shows `lat 0ms lead +0.0deg`. The `lead` number is the
   renderer's own measurement of how far the predicted pose runs ahead of the newest
   raw packet.
2. Press **`]` (right bracket) eight times** — each press adds 50 ms of prediction
   (logged in the terminal too).
3. Press **`[`** to step back down to 0.

✅ **PASS — all three:**
1. Every `]` press makes the camera **visibly jump forward along the orbit**, instantly
   (and `[` jumps it back). That jump IS the latency offset shifting the pose.
2. The `lead` readout climbs with each press — at the default 12 s orbit each 50 ms
   step adds **~1.5°**, so 8 presses ≈ `lat 400ms lead +12.0deg` (±0.5°).
3. Back at `lat 0ms`, `lead` returns to ~+0.0deg.

(`--latency-ms N` still sets the starting value — that's what a config will pin on
stage, where real values are 10–80 ms.)
❌ **FAIL:** presses don't move the camera, or `lead` doesn't track `lat` (paste the
title-bar line).

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
ARKit tracking as FreeD over UDP. **Android:** no suitable free app exists — build the
companion app instead using the ready-made prompt brief in
`docs/android-freed-app-brief.md` (it pins this repo's exact wire format and test bytes;
the renderer command below is the same either way). Phone and PC must be on the same
network:

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

- [ ] Build warning-clean; `ctest` 78/78 (§1–2)
- [ ] Simulator orbit: camera circles the cube facing it, smooth, `trk ~50 Hz`, `rej:0`;
      freeze-then-fly on simulator stop (§3)
- [ ] `]`/`[` latency steps jump the camera along the orbit; `lead` tracks `lat`
      (~12° at 400 ms) and returns to ~0 (§4)
- [ ] Handheld sway visible; lens table loads and tightens the FOV (§5)
