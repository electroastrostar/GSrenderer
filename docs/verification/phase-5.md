# Phase 5 — Operator Verification (NDI Output — PROTOTYPE MILESTONE)

Two stages: a **desk test** on one machine with NDI Studio Monitor (§1–§5, the merge
gate), then the **stage/second-machine test** with UE5 nDisplay (§6–§7, the plan's
milestone acceptance — record on the PR when performed, may follow the merge).

**One-time setup (render machine):** install the **NDI 6 SDK** (free, registration:
<https://ndi.video/for-developers/ndi-sdk/>) to its default path
(`C:\Program Files\NDI\NDI 6 SDK`), and **NDI Tools** (<https://ndi.video/tools>) for
Studio Monitor. If the SDK lives elsewhere, set the `NDI_SDK_DIR` environment variable.

> Terminals as always: x64 Native Tools prompt, repo folder. Delete the build folder
> first so CMake re-detects the SDK.

## 1. Rebuild with the SDK detected

```
:: Windows — terminal 1, in C:\dev\GSrenderer
cd C:\dev\GSrenderer
git fetch origin && git checkout claude/phase-0-scaffolding-qf5mw4 && git pull origin claude/phase-0-scaffolding-qf5mw4
rmdir /s /q build
cmake -S . -B build
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

✅ **PASS:** configure prints **`NDI SDK found at ... — NDI output enabled`** (not the
"STUB" or "not found" lines) alongside the CUDA line; build warning-clean; `91/91` tests.
❌ **FAIL:** `NDI SDK not found` warning → check the SDK install path / set `NDI_SDK_DIR`,
delete `build`, reconfigure. Paste the configure output otherwise.

Also copy the NDI runtime DLL next to the exe (or ensure it's on PATH) if the app later
fails to start: `C:\Program Files\NDI\NDI 6 SDK\Bin\x64\Processing.NDI.Lib.x64.dll` →
`build\src\app\Release\`.

## 2. Stream starts and appears on the network

```
:: terminal 1
build\src\app\Release\splatcast.exe assets\fixtures\grid_fiducial.ply --ndi splatcast --fps 25
```

✅ **PASS:** log shows `NDI source 'splatcast' created: 1920x1080 @ 25/1` and
`NDI streaming 'splatcast' at 25 fps`; the HUD gains `ndi 25 fps late:N rb X.XXms drop:N`.
❌ **FAIL:** startup error mentioning the SDK (→ §1 DLL note) — paste the log.

## 3. Desk receive with Studio Monitor

Open **NDI Studio Monitor** (NDI Tools) on the SAME machine (or any machine on the LAN):
pick `<HOSTNAME> (splatcast)` from its menu.

✅ **PASS — all four:**
1. Studio Monitor shows the fiducial grid live; fly camera movement (WASD) appears in
   the monitor with sub-second delay.
2. HUD `late:` stays near 0 while the scene renders normally (a few late frames at
   startup are fine); `rb` (readback) reads **< 3 ms** at 1080p; `drop:` stays 0.
3. Studio Monitor's stream info (right-click → info/overlay) reports ~25 fps.
4. Run 10 minutes: `late:` growth is negligible (< 1/min), memory stable (Task Manager).

## 4. Pacing actually paces

Quit and rerun with `--fps 24`, then `--fps 30`. ✅ **PASS:** Studio Monitor's reported
rate follows (≈24 / ≈30), and the HUD fps in tracked/free run sits AT the target (the
pacer sleeps the loop — you should no longer see 200+ fps).

## 5. End-to-end latency measurement (flash-frame, plan Task 5)

With the stream running and Studio Monitor visible **next to** the splatcast window
(both on screen at once):

1. Film both windows with your phone's **slow-motion camera** (240 fps if available).
2. Press **F** in the splatcast window — it emits exactly one white frame (log:
   `FLASH frame emitted`), visible in both the preview and the stream.
3. In the slow-mo footage, count video frames between the preview window flashing and
   Studio Monitor flashing. latency_ms = frame_count × (1000 / camera_fps).
4. Repeat 5×, note min/median/max in the PR. Desk expectation: **40–120 ms**
   (NDI encode + net + decode); >200 ms suggests Wi-Fi or a busy GPU — note conditions.

✅ **PASS:** measured and recorded (the number itself doesn't gate — it's the Phase 6
comparison baseline; plan acceptance is "measured latency documented").

## 6. STAGE / SECOND MACHINE — UE5 nDisplay receive (milestone acceptance)

Needs the UE machine: follow `docs/ue5-ndi-setup.md`, then:

- ✅ UE displays the stream inside the nDisplay config (editor preview or on the wall).
- ✅ With the FreeD simulator (or the real tracker at the stage) driving splatcast,
  camera motion shows **correct parallax** on the wall/viewport.
- ✅ Repeat the §5 flash measurement against the UE viewport — record it.
- ✅ **30+ minute sustained run** at target rate: `late:`/`drop:` negligible, no leak
  (renderer memory flat), no drift visible.

Record these on the PR when performed (checklist below has a separate box). The plan's
**PROTOTYPE COMPLETE** milestone = this section green at the stage.

## 7. Record results on the PR

- [ ] SDK detected; build + 91/91 (§1)
- [ ] Stream up; Studio Monitor receives; `rb < 3 ms`, `late`/`drop` ~0 over 10 min (§2–3)
- [ ] Pacer holds 24/25/30 (§4)
- [ ] Flash latency measured and posted (§5)
- [ ] STAGE: UE5 nDisplay receive + parallax + 30-min soak + latency vs UE (§6 —
      may be checked after merge at the stage session)
