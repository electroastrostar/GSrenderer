# Phase 1 — Operator Verification (Asset Loading with Full SH)

Run these on **your** machines before merging the Phase 1 PR. What only you can verify:

1. Everything still builds warning-clean on Windows/MSVC and with CUDA present.
2. The **asset inspector reports correct values for a real 3DGS asset** — the container
   only tested against small synthetic fixtures.

**Time required:** ~15 minutes. **You will need:** one real 3DGS `.ply` you trained or
downloaded (any scene from Postshot, Nerfstudio/splatfacto, or the INRIA pipeline works).
If you have none handy, steps 1–5 still verify everything except the real-asset check —
leave that one box unchecked and note it in the PR.

> Machine setup (Git, VS 2022, CMake, CUDA) was done in Phase 0 — see
> `docs/verification/phase-0.md` §0–§1 if you're on a fresh machine. As in Phase 0:
> **Windows commands go in the "x64 Native Tools Command Prompt for VS 2022"** (Start menu →
> type `x64 Native Tools`); Linux commands in any terminal. Do all steps in one terminal
> window, in order.

## 1. Update the code

**Machine:** A6000 dev box (and optionally a render node). **Folder:** the repo —
`C:\dev\GSrenderer` (Windows) or `~/dev/GSrenderer` (Linux). In your terminal:

```
:: Windows
cd C:\dev\GSrenderer
git fetch origin
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
```

```bash
# Linux
cd ~/dev/GSrenderer
git fetch origin
git checkout claude/phase-0-scaffolding-qf5mw4
git pull origin claude/phase-0-scaffolding-qf5mw4
```

✅ **Expected:** ends with `Your branch is up to date with 'origin/claude/phase-0-scaffolding-qf5mw4'`
(or a fast-forward summary listing `src/loader/...` files).

## 2. Configure + build

Same terminal, same folder:

```
:: Windows
cmake -S . -B build
cmake --build build --config Release -j
```

```bash
# Linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

✅ **Expected (PASS):** configure still shows `CUDA … found (arch 86) — GPU targets enabled`
on the A6000 machine; build finishes having built `gsr_loader` and `asset_inspector` with
**zero warnings/errors for files under `src\`, `tests\`, or `tools\`**.
❌ **FAIL:** any warning/error naming those folders — copy it into the PR (§6).

## 3. Run the tests

Same terminal, same folder:

```
:: Windows
ctest --test-dir build -C Release --output-on-failure
```

```bash
# Linux
ctest --test-dir build --output-on-failure
```

✅ **Expected (PASS):** `100% tests passed, 0 tests failed out of 19`
(11 of those are new loader tests: SH degrees 0–3, hand-checked fixtures, malformed-file
rejection). ❌ **FAIL:** any failure — the output names the test; paste it into the PR (§6).

## 4. Inspect the committed test fixture — must match exactly

Same terminal, same folder:

```
:: Windows
build\tools\asset_inspector\Release\asset_inspector.exe assets\fixtures\cube_deg3.ply
```

```bash
# Linux
./build/tools/asset_inspector/asset_inspector assets/fixtures/cube_deg3.ply
```

✅ **Expected (PASS):** this output, byte-for-byte (except the path on the first line):

```
asset:      assets/fixtures/cube_deg3.ply
splats:     8
SH degree:  3 (45 rest + 3 DC coefficients per splat)
bounds min: [   -1.000,    -1.000,    -1.000] m (asset space)
bounds max: [    1.000,     1.000,     1.000] m (asset space)
memory:     1.8 KiB (CPU == GPU upload size)
```

❌ **FAIL:** any differing number, or an `error:` line.

## 5. Inspect a real asset — the values must match a reference viewer

Use a real 3DGS `.ply` on this machine (example below assumes
`C:\assets\myscene.ply` / `~/assets/myscene.ply` — substitute your path).

```
:: Windows
build\tools\asset_inspector\Release\asset_inspector.exe C:\assets\myscene.ply
```

```bash
# Linux
./build/tools/asset_inspector/asset_inspector ~/assets/myscene.ply
```

Then cross-check the numbers against something that already knows this asset:

- **Splat count:** open the same `.ply` in **SuperSplat** (free, in the browser:
  <https://superspl.at/editor> → drag the file in) and compare its splat count — or use the
  count reported by the tool that trained the scene. Must match **exactly**.
- **SH degree:** scenes trained with default INRIA/Postshot/Nerfstudio settings are
  **degree 3**. If you trained with a reduced SH setting, expect that degree instead.
- **Bounds:** sanity check only — the min/max extent should roughly match the physical size
  of the captured scene in meters (a room-scale scan shouldn't report 500 m).

✅ **PASS:** count matches exactly; degree is as trained; bounds are plausible.
❌ **FAIL:** count/degree mismatch, a crash, or an `error:` on a file SuperSplat opens fine.
Also try a deliberate bad input (e.g. point it at `CLAUDE.md`) — ✅ it must print a clean
`error: load_ply(...)` line and not crash.

## 6. Record your results on the PR

1. In a browser, open the Phase 1 PR (link is in the merge-request notification, or:
   GitHub repo → **Pull requests** tab) while **signed in**.
2. **Click the checkboxes** in the PR description directly — they save instantly. Fallback:
   `⋯` menu at the top-right of the description → **Edit** → change `- [ ]` to `- [x]` →
   **Update comment**.
3. For any FAIL: leave the box unchecked and paste the terminal output as a PR comment,
   with which machine and which asset file it happened on. **Don't merge until every box
   is checked** (the real-asset box may be deferred with a note if you had no asset handy).

### The checklist (mirrored in the PR description)

- [ ] Build is warning-clean for `src\`/`tests\`/`tools\`; CUDA still detected on the A6000 (§2)
- [ ] `ctest`: 19/19 tests pass (§3)
- [ ] Fixture inspection matches the expected output exactly (§4)
- [ ] Real asset: splat count matches SuperSplat/trainer exactly; SH degree as trained;
      bounds plausible; bad input rejected cleanly (§5)
