# FreeD Protocol (D1 message)

What our tracking ingest speaks. FreeD is the BBC R&D camera-tracking wire format that
Mo-Sys StarTracker (and most VP trackers) emit as a UDP stream, typically to port 8001 at
the camera frame rate or higher. We consume the **type D1** ("camera position/orientation")
message only; poll/command messages (D2/DA/DB…) are not used by StarTracker's streaming
mode.

Implementation: `src/tracking/freed.hpp` (`parse_freed_d1` / `serialize_freed_d1`).
Reference: Vizrt/stYpe/Panasonic FreeD documentation; the layout below matches what
StarTracker emits.

## Packet layout — 29 bytes, all multi-byte fields big-endian

| Bytes | Field | Encoding | Wire unit |
|---|---|---|---|
| 0 | Message type | `0xD1` | — |
| 1 | Camera ID | uint8 | — |
| 2–4 | **Pan** | int24, two's complement | degrees × 32768 |
| 5–7 | **Tilt** | int24 | degrees × 32768 |
| 8–10 | **Roll** | int24 | degrees × 32768 |
| 11–13 | **X** | int24 | mm × 64 |
| 14–16 | **Y** | int24 | mm × 64 |
| 17–19 | **Z** (height) | int24 | mm × 64 |
| 20–22 | **Zoom** | uint24 | raw encoder counts |
| 23–25 | **Focus** | uint24 | raw encoder counts |
| 26–27 | Spare / user | uint16 | — |
| 28 | Checksum | see below | — |

- **Angles**: `value_deg = int24 / 32768.0` (15 fractional bits). Range ±256°.
- **Position**: `value_mm = int24 / 64.0` (6 fractional bits). Range ±131 km, resolution
  1/64 mm.
- **Zoom/focus**: opaque encoder counts — meaningful only through a lens calibration
  table (Phase 3 Task 5). Never interpret as physical units directly.
- **Checksum**: `(0x40 - sum(bytes[0..27])) mod 256`. A packet is dropped (and counted)
  on size, type, or checksum mismatch.

## Unit conversion boundary (CLAUDE.md rule 1)

`parse_freed_d1` converts wire units to **radians and meters** at the parse boundary;
nothing downstream ever sees degrees or millimeters. `serialize_freed_d1` (used by
`tools/freed_simulator`) is the exact inverse.

## FreeD space conventions

Right-handed, **Z up**, X/Y in the horizontal plane; positions are of the camera mount
point. Pan is a rotation about the vertical axis (positive = clockwise seen from above),
tilt positive = camera up, roll positive = clockwise from behind the camera. Rotation
composition order: pan, then tilt, then roll (Z → X' → Y'' intrinsic).

The mapping into our render world is the named convention function `render_from_freed`
in `src/core/transforms.hpp` — see `docs/architecture.md` §Coordinate Spaces. **Verify
axis signs against the real StarTracker on stage** (plan §6 watch item); the simulator
and unit tests pin today's documented assumption.
