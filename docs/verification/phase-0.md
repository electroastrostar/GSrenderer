# Phase 0 — Operator Verification

Run these on **your** machines before merging the Phase 0 PR. Phase 0's acceptance criterion
is "`cmake --build` succeeds clean on the dev machine" — CI/container builds only cover
Linux + GCC with **no CUDA**, so the two things only you can verify are:

1. CUDA detection actually **enables** GPU targets on the A6000 machine.
2. The Windows / MSVC build is warning-clean (`/W4`) — nothing has compiled with MSVC yet.

Time required: ~10 minutes. Prerequisites: CMake ≥ 3.24, a C++20 compiler
(VS 2022 on Windows), CUDA Toolkit installed on the A6000 machine, network access on first
configure (FetchContent pulls spdlog + Catch2).

---

## 1. Get the branch

```bash
git fetch origin claude/phase-0-scaffolding-qf5mw4
git checkout claude/phase-0-scaffolding-qf5mw4
```

## 2. Configure — and check the CUDA line

**Linux / single-config generators:**

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
```

**Windows (Visual Studio generator — multi-config, no CMAKE_BUILD_TYPE needed):**

```bat
cmake -S . -B build
```

**PASS:** configure output contains (version may differ):

```
-- splatcast: CUDA 12.x found (arch 86) — GPU targets enabled
```

**FAIL:** you see the warning `no CUDA toolchain found — building CPU-only targets` on a
machine that has the CUDA Toolkit installed. (Check `nvcc --version` works in that shell;
on Windows, use a "x64 Native Tools" prompt or ensure CUDA's bin is on PATH.)

## 3. Build — must be warning-clean

```bash
# Linux
cmake --build build/release -j
```

```bat
:: Windows
cmake --build build --config Release -j
```

**PASS:** all targets build with **zero warnings or errors from our targets**
(`gsr_core`, `splatcast`, `gsr_tests`). Warnings from inside `_deps/` (spdlog/Catch2
sources) would be a bug too — report them — but are dependency issues, not project code.

## 4. Run the tests

```bash
# Linux
ctest --test-dir build/release --output-on-failure
```

```bat
:: Windows
ctest --test-dir build -C Release --output-on-failure
```

**PASS:** `100% tests passed, 0 tests failed out of 8`

## 5. Run the app — check the log format

```bash
# Linux
./build/release/src/app/splatcast
```

```bat
:: Windows
build\src\app\Release\splatcast.exe
```

**PASS:** two log lines, each carrying the frame stamp, and CUDA reported enabled on the
A6000 machine:

```
[2026-…] [app] [info] [frame 0][t_mono_us …] splatcast 0.1.0 starting (CUDA: enabled)
[2026-…] [app] [info] [frame 0][t_mono_us …] scaffolding only — nothing to render yet (Phase 0)
```

---

## Checklist (copy into the PR before merging)

- [ ] A6000 machine: configure shows `CUDA … found (arch 86) — GPU targets enabled`
- [ ] Windows 11: build completes with zero warnings/errors (MSVC `/W4`)
- [ ] (If you also build on Linux) zero warnings/errors with GCC/Clang
- [ ] `ctest`: 8/8 tests pass
- [ ] App runs; every log line shows `[frame N][t_mono_us T]`; `CUDA: enabled` on the A6000

If anything fails, paste the configure/build output into the PR or back into a Claude
session — fixing it is part of Phase 0, and the PR should not merge until this list is green.
