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

## In Progress

- **Phase 5, Task 3 first — frame pacer** (`src/output/frame_pacer.*`): pure-logic
  software pacer for 24/25/30 fps (next-deadline scheduling, late-frame counter,
  jitter stats) — unit-testable with a fake clock.
  - Exact next step: implement + tests; commit.

## Next

- Phase 5, Task 2 — async GPU readback (`src/renderer/readback.*`, CUDA): pinned-memory
  triple buffer + cudaMemcpyAsync + events; measured readback ms; [gpu] test.
- Phase 5, Task 1 — NDI sender (`src/output/ndi/`): BGRA frames + monotonic timecode;
  SDK detection in CMake + container stub for compile checks; app `--ndi NAME` +
  `[ndi]` config (fps, name); late-frame logging.
- Phase 5, Task 4 — `docs/ue5-ndi-setup.md` (NDI Media plugin → Media Texture →
  nDisplay inner-frustum media input, ICVFX media sharing).
- Phase 5, Task 5 — end-to-end latency measurement procedure (flash-frame test, F key)
  documented in the verification doc.
- Phase 5 wrap-up — `docs/verification/phase-5.md` (NDI Studio Monitor desk test first,
  then UE5 second-machine test, 30-min soak, latency procedure), PR.
