# Android FreeD Tracker App — Prompting Brief

A companion Android app that streams the phone's ARCore pose as FreeD D1 packets, for use
as a handheld tracker against `splatcast --freed-port 8001`. This file is the brief to
hand to the Claude session that builds it: §1 explains what matters and why, §2 is a
**ready-to-paste prompt**. Everything below is grounded in this repo's implementation —
`docs/freed-protocol.md` (wire format), `tests/test_freed.cpp` (hand-checked bytes),
`src/core/transforms.hpp` (conventions).

## 1. What to consider when prompting (and why)

1. **Pin the wire format — never let the model recall FreeD from memory.** FreeD is niche
   and models will confidently invent field orders. Paste the full packet table (included
   in §2) into the prompt. The two subtle points models get wrong: all multi-byte fields
   are **big-endian int24**, and **angles wrap** into [−180°, 180°) rather than clamping
   (we hit exactly this bug in PR #4).

2. **Demand encoder unit tests against known bytes FIRST**, before any UI or networking.
   The fixtures in §2 come from this repo's `tests/test_freed.cpp` and were derived on
   paper. If the app's encoder reproduces them byte-for-byte, it will interoperate; if
   not, nothing else matters. This gives the app session an objective inner loop that
   needs no renderer.

3. **ARCore, not raw sensors.** Raw gyro/accelerometer has no absolute position and
   drifts within seconds. ARCore (Google Play Services for AR) provides drift-corrected
   6DoF world-tracked pose in meters at ~30 Hz — that's the only viable source. The
   phone must be ARCore-compatible.

4. **The coordinate mapping is the #1 bug source** — same lesson as this whole project.
   ARCore world: right-handed, **Y up**, camera faces −Z. Our FreeD space: right-handed,
   **Z up**, pan-zero faces +X, pan positive = clockwise from above, tilt positive = up,
   roll positive = clockwise from behind, composed pan → tilt → roll. The prompt in §2
   gives a starting axis map and requires hand-derived unit tests for it. Expect it to
   be wrong on the first physical try anyway, which is why…

5. **…the app must have axis-calibration toggles in the UI**: per-axis sign flips for
   pan/tilt/roll and X/Y/Z, plus a **"zero here"** button that captures the current
   ARCore pose as the FreeD origin. Ten-second toggle beats a rebuild loop every time a
   sign is mirrored. (Mirrored motion on first run is NOT failure — it's calibration.)

6. **Objective definition of done** — put it in the prompt so the session knows when to
   stop: (a) encoder tests byte-match the fixtures; (b) the Python validator script (in
   §2, the same one used to validate this repo's simulator) reports valid packets at
   ~30 Hz; (c) walking around visibly moves the splatcast camera with parallax.

7. Practical details worth stating: Kotlin, single activity, sideloaded debug tool (no
   Play Store); UDP datagrams to a configurable IP:port (default 8001); send on every
   ARCore frame update (~30 Hz — comfortably inside splatcast's 0.5 s staleness window);
   keep the screen awake while streaming; CAMERA + INTERNET permissions; phone and PC on
   the same Wi-Fi. Nice-to-haves that exercise splatcast: camera-ID field, **zoom/focus
   sliders** mapped to raw 0–1048575 (drives the `--lens-file` FOV path live), on-screen
   pose readout + packets/sec counter.

8. Session logistics: if the session can reach GitHub, point it at this repo for
   `docs/freed-protocol.md`; otherwise §2 is self-contained. Final scene alignment
   (scale/origin to the splat world) is splatcast's job in Phase 4 — the app only ships
   honest meters and wrapped degrees.

## 2. Paste this as your prompt

````text
Build a minimal Android debug tool in Kotlin: a single-activity app that streams the
phone's ARCore camera pose over UDP as FreeD D1 packets, so the phone acts as a handheld
camera tracker for a 3D renderer listening on my PC.

WORKFLOW REQUIREMENT: implement and unit-test the packet encoder FIRST (pure Kotlin, no
Android dependencies), against the byte fixtures below. Only proceed to ARCore/UI/network
after those tests pass byte-for-byte.

## FreeD D1 packet — exact wire format (do not use your own recollection of FreeD)

29 bytes total; all multi-byte fields BIG-ENDIAN; int24 = two's complement 3 bytes.

| Bytes | Field       | Encoding | Wire unit               |
|-------|-------------|----------|-------------------------|
| 0     | Message type| 0xD1     | —                       |
| 1     | Camera ID   | uint8    | —                       |
| 2–4   | Pan         | int24    | degrees × 32768         |
| 5–7   | Tilt        | int24    | degrees × 32768         |
| 8–10  | Roll        | int24    | degrees × 32768         |
| 11–13 | X           | int24    | millimeters × 64        |
| 14–16 | Y           | int24    | millimeters × 64        |
| 17–19 | Z (height)  | int24    | millimeters × 64        |
| 20–22 | Zoom        | uint24   | raw counts (0–1048575)  |
| 23–25 | Focus       | uint24   | raw counts (0–1048575)  |
| 26–27 | Spare       | zeros    | —                       |
| 28    | Checksum    | uint8    | (0x40 − sum(bytes[0..27])) mod 256 |

Angles are periodic: WRAP them into [−180, 180) degrees before encoding — never clamp.
Positions clamp at the int24 range.

## Required encoder unit tests (hand-derived reference bytes)

- pan  = +90.0°  → bytes 2–4  = 2D 00 00
- tilt = −45.0°  → bytes 5–7  = E9 80 00
- X    = +1.5 m  → bytes 11–13 = 01 77 00
- Z    = −0.5 m  → bytes 17–19 = FF 83 00
- checksum: a 28-byte prefix of {0xD1, then 27 zero bytes} → checksum byte = 0x6F
- pan = +190° encodes identically to pan = −170° (wrap test)

## Pose source and coordinate conversion

Use ARCore (Google Play Services for AR) motion tracking — NOT raw IMU sensors. Read the
camera pose each frame update (~30 Hz) and convert to the receiver's FreeD conventions:

- ARCore world: right-handed, Y up, camera looks along −Z of its local frame.
- Receiver FreeD space: right-handed, Z up; pan = 0 faces +X; pan positive = clockwise
  seen from above; tilt positive = camera up; roll positive = clockwise viewed from
  behind the camera; rotation composition order pan → tilt → roll.
- Starting axis map (verify with tests, then trust the in-app flips for fine-tuning):
  FreeD X = −(ARCore Z), FreeD Y = −(ARCore X), FreeD Z = ARCore Y.
- Derive pan/tilt/roll by decomposing the ARCore orientation quaternion in that mapped
  frame. Write unit tests with hand-derived cases (identity pose → pan/tilt/roll all 0;
  a 90° left turn → pan −90°; looking 45° down → tilt −45°).

## UI (single screen, debug-tool plain)

- Target IP (text), port (default 8001), camera ID (default 1), Start/Stop toggle.
- "Zero here" button: captures the current ARCore pose as the FreeD origin (subsequent
  packets are relative to it).
- Six sign-flip toggles: pan / tilt / roll / X / Y / Z (applied before encoding) — the
  receiver's conventions may differ per device, and these make calibration interactive.
- Zoom and Focus sliders mapped to raw 0–1048575 (defaults 524288).
- Live readout: current pan/tilt/roll (deg), X/Y/Z (m), packets sent, send rate (Hz).
- Keep the screen awake while streaming.

## Runtime

- UDP datagrams (one packet per ARCore frame update) on a background thread.
- Permissions: CAMERA (ARCore) + INTERNET. Handle ARCore tracking-lost states by pausing
  sends and showing status. Sideload build via Android Studio; no Play Store polish.

## Definition of done (all three, in order)

1. Encoder unit tests pass byte-for-byte against the fixtures above.
2. This Python script on the PC reports valid packets at ~30 Hz while the app streams:

   import socket
   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(("0.0.0.0", 8001))
   good = bad = 0
   while True:
       data, addr = s.recvfrom(512)
       ok = len(data) == 29 and data[0] == 0xD1 and (0x40 - sum(data[:28])) & 0xFF == data[28]
       good += ok; bad += not ok
       if (good + bad) % 30 == 0: print(f"valid={good} invalid={bad} from {addr}")

3. With the renderer running on the PC (`splatcast.exe <asset.ply> --freed-port 8001`),
   physically walking around moves the rendered camera with correct parallax. Mirrored
   axes at this step are fixed with the in-app flip toggles, not code changes.
````
