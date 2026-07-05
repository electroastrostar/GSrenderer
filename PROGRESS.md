# Progress

Tracking file per SPLATCAST_PLAN.md §7.1. Current phase: **Phase 5 — NDI Output
(PROTOTYPE MILESTONE)** (branch `claude/phase-0-scaffolding-qf5mw4`, restarted from
`main` after Phase 4 PR #5 merged; Phase 4 §6 UE5 line-up deferred to a UE session,
recorded separately).

**Phase 5 constraints:** the NDI SDK is proprietary (registration download) — CMake
DETECTS it (like CUDA): real sender compiles when `NDI_SDK_DIR`/default install path has
the SDK; a clearly-labeled header stub (`third_party/ndi_stub/`) lets the container
compile-check the exact same sender code. Never a fake runtime pass: without the real
SDK the app refuses `--ndi` with a clear error. The UE5-receiver acceptance needs the
operator's second machine.

## Done

- **Phases 0–4 complete.** PRs #1–#5 merged after operator verification. Phase 4 §6
  (UE5 line-up) pending a UE machine — not a merge gate.

- Phase 5, Task 3 — frame pacer (slot grid, no 24fps drift, late frames skip slots);
  fake-clock tests.
- Phase 5, Task 2 — AsyncReadback (`src/renderer/readback.cu`): 3 pinned slots,
  cudaMemcpyAsync on a non-blocking stream, event-timed readback ms, ring-full drops
  counted.
- Phase 5, Task 1 — NDI sender: SDK detected like CUDA (NDI_SDK_DIR/defaults), honest
  compile-check stub (`third_party/ndi_stub`, initialize()=false so a stub build can
  never pretend to stream); RGBA frames + 100ns timecode from mono_us; `--ndi NAME
  --fps F` + `[ndi]` config; preview loop: render → flash(F) → readback.begin/acquire →
  send → pacer sleep; HUD gains `ndi <fps> late: rb: drop:`. 91/91 host+CUDA (stub).

- Phase 5, Tasks 4+5 — `docs/ue5-ndi-setup.md` (receiver plugin → media texture →
  ICVFX camera media input, media sharing, free-running-NDI expectations per plan §6.6)
  and the flash-frame latency procedure (F key + slow-mo phone count) in
  `docs/verification/phase-5.md` (desk gate §1–5 with Studio Monitor; stage/UE5
  milestone §6 recorded separately). Fresh acceptance build 0 warnings, 91/91.

## In Progress

- (nothing — Phase 5 code+docs complete; awaiting operator desk verification per
  `docs/verification/phase-5.md`, then merge; §6 stage test = PROTOTYPE milestone.)
