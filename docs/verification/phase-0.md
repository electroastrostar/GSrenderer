# Phase 0 — Operator Verification

Run these on **your** machines before merging the Phase 0 PR
(<https://github.com/electroastrostar/GSrenderer/pull/1>). Phase 0's acceptance criterion is
"`cmake --build` succeeds clean on the dev machine" — the automated build only covered
Linux + GCC with **no CUDA**, so the two things only you can verify are:

1. CUDA detection actually **enables** GPU targets on an A6000 machine.
2. The Windows / MSVC build is warning-clean — nothing has compiled with MSVC yet.

**Time required:** ~15 minutes (plus one-time installs if the machine is fresh).

---

## 0. One-time machine setup

Do this once per machine. If these are already installed, skip to §1.

**On Windows 11 (A6000 dev machine and/or a render node):**

| Install | From | Notes |
|---|---|---|
| Git for Windows | <https://git-scm.com/download/win> | Defaults are fine |
| Visual Studio 2022 Community | <https://visualstudio.microsoft.com> | In the installer, tick the **"Desktop development with C++"** workload |
| CMake 3.24+ | <https://cmake.org/download> (Windows x64 Installer) | Tick **"Add CMake to the system PATH"** during install |
| CUDA Toolkit 12.x | <https://developer.nvidia.com/cuda-downloads> | Install **after** Visual Studio, Express install is fine |

**On Linux (only if your A6000 dev box runs Linux):** you need `git`, `cmake` ≥ 3.24, `g++`,
and the CUDA Toolkit (`nvcc`). On Ubuntu: `sudo apt install git cmake g++` plus NVIDIA's CUDA
package for your distro.

Both platforms need **network access the first time you configure** (CMake downloads two
pinned libraries, spdlog and Catch2). After that it builds offline.

---

## 1. Open the right terminal

Every command in this document is typed into a terminal window. Which one matters:

- **Windows:** click **Start**, type `x64 Native Tools`, and open
  **"x64 Native Tools Command Prompt for VS 2022"**. Use this — not plain PowerShell/cmd —
  because it puts the MSVC compiler and CUDA on the PATH so CMake can find them.
- **Linux:** any terminal (GNOME Terminal, etc.).

Keep this one terminal window open and do **all** the following steps in it, in order.

## 2. Put the code on the machine

Pick a folder to keep code in. This document assumes:

- Windows: `C:\dev` (so the repo will live at `C:\dev\GSrenderer`)
- Linux: `~/dev` (so the repo will live at `~/dev/GSrenderer`)

If you use a different location, mentally substitute it everywhere below.

**If the repo is NOT yet on this machine** — in your terminal, type:

```
:: Windows
mkdir C:\dev
cd C:\dev
git clone https://github.com/electroastrostar/GSrenderer.git
cd GSrenderer
```

```bash
# Linux
mkdir -p ~/dev
cd ~/dev
git clone https://github.com/electroastrostar/GSrenderer.git
cd GSrenderer
```

**If the repo IS already on this machine** — go to it and update it:

```
:: Windows
cd C:\dev\GSrenderer
git fetch origin
```

```bash
# Linux
cd ~/dev/GSrenderer
git fetch origin
```

Then, either way, switch to the Phase 0 branch:

```
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
```

✅ **Expected:** the last line reads `Your branch is up to date with 'origin/claude/phase-0-scaffolding-qf5mw4'`
(or similar). ❌ If git asks for credentials it doesn't have, sign in via Git Credential
Manager (Windows pops up a browser login automatically).

> **From here on, every command is typed in this same terminal, from inside the repo folder**
> (`C:\dev\GSrenderer` or `~/dev/GSrenderer`). If you close the terminal, reopen it (§1) and
> `cd` back into the repo folder first.

## 3. Configure — and check the CUDA line

This step generates the build system into a `build` subfolder inside the repo. Type:

```
:: Windows (Visual Studio picks Debug/Release later, at build time)
cmake -S . -B build
```

```bash
# Linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

✅ **Expected (PASS):** scroll the output; on a machine with an NVIDIA GPU + CUDA Toolkit you
must see a line like (version number may differ):

```
-- splatcast: CUDA 12.4 found (arch 86) — GPU targets enabled
```

…and it ends with `-- Generating done` / `Build files have been written to: …`.

❌ **FAIL if** you instead see
`splatcast: no CUDA toolchain found — building CPU-only targets` **on a machine that has
CUDA installed.** First check: type `nvcc --version` in the same terminal. If that errors,
the terminal can't see CUDA — on Windows make sure you opened the **x64 Native Tools**
prompt (§1) and that the CUDA Toolkit installed successfully; then delete the `build` folder
and rerun this step.

❌ Also FAIL if configure stops with `CMake Error` (paste the full output into the PR — see §7).

## 4. Build — must be warning-clean

Same terminal, same folder. Type:

```
:: Windows
cmake --build build --config Release -j
```

```bash
# Linux
cmake --build build -j
```

This takes a few minutes the first time. ✅ **Expected (PASS):** it finishes with the last
targets built (`splatcast`, `gsr_tests`) and prints **no lines containing `warning` or
`error` for our code** (files under `src\` and `tests\`).

❌ **FAIL if** any warning/error mentions a file under `src\` or `tests\` — copy those lines
into the PR (§7). (Warnings pointing inside `build\_deps\…` are third-party library issues —
report them too, but they're a different bug.)

## 5. Run the tests

Same terminal, same folder. Type:

```
:: Windows
ctest --test-dir build -C Release --output-on-failure
```

```bash
# Linux
ctest --test-dir build --output-on-failure
```

✅ **Expected (PASS):**

```
100% tests passed, 0 tests failed out of 8
```

❌ **FAIL if** any test fails — the `--output-on-failure` flag prints the details; copy them
into the PR (§7).

## 6. Run the app — check the log format

Same terminal, same folder. Type:

```
:: Windows
build\src\app\Release\splatcast.exe
```

```bash
# Linux
./build/src/app/splatcast
```

✅ **Expected (PASS):** exactly two log lines, each carrying a `[frame N][t_mono_us T]`
stamp, and — on the A6000 machine — `CUDA: enabled`:

```
[2026-…] [app] [info] [frame 0][t_mono_us 82] splatcast 0.1.0 starting (CUDA: enabled)
[2026-…] [app] [info] [frame 0][t_mono_us 95] scaffolding only — nothing to render yet (Phase 0)
```

❌ **FAIL if** the stamp is missing, the app crashes, or an A6000 machine reports
`CUDA: disabled`.

## 7. Record your results on the PR

1. In a web browser, open <https://github.com/electroastrostar/GSrenderer/pull/1> and make
   sure you're **signed in** to GitHub (your account owns the repo, so you have the needed
   permissions).
2. The PR description (the first big text block at the top) contains the checklist below.
   **The checkboxes are directly clickable** — just click each empty box `☐` and GitHub
   saves it immediately; no edit mode needed.
   - If clicking doesn't work: click the **`⋯`** menu at the top-right corner of the
     description block → **Edit** → change each `- [ ]` to `- [x]` → press
     **Update comment**.
3. If any step FAILED: don't check that box. Instead scroll to the bottom of the PR page and
   add a comment — paste the terminal output of the failing step (select text in the
   terminal, right-click → Copy on Windows) and say which machine it was on. Fixing it is
   part of Phase 0; **do not merge until every box is checked.**
4. When all boxes are checked, press the green **Merge pull request** button.

### The checklist (mirrored in the PR description)

- [ ] A6000 machine: configure shows `CUDA … found (arch 86) — GPU targets enabled` (§3)
- [ ] Windows 11: build completes with zero warnings/errors for `src\`/`tests\` files (§4)
- [ ] `ctest`: 8/8 tests pass (§5)
- [ ] App runs; both log lines show `[frame N][t_mono_us T]`; `CUDA: enabled` on the A6000 (§6)
