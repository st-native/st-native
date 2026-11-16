# M1a portability spike: V1–V3 on five targets

Date: **2026-11-13**. Scope: build the recompilation base at the planned revisions, run V1–V3 of
[PLATFORM_PORTABILITY.md §7](PLATFORM_PORTABILITY.md#7-minimal-validation-strategy-before-adr-0001-is-accepted-no-restructuring-needed),
and run our synthetic cartridge through the base by a reproducible path.

- **Round 1, local:** Windows x64, then Linux x64 in WSL 2.
- **Round 2, CI:** Linux x64, Linux arm64, macOS arm64 and Android arm64 on GitHub-hosted runners, from a **private**
  repository created with your approval (`st-native-m1a-spike`). It holds only `spike/m1a` and the workflow: our own
  scripts, patches, the synthetic cartridge and its reference hashes. Its assembly script refuses ROM/save
  extensions and any `.rom` other than the synthetic fixture.

Everything below was executed. Raw results are in `spike/m1a/results/` (git-ignored; CI runs under
`results/ci_run<N>/`). No ROM of the game was used, and nothing was made public.

## 1. Revisions (`spike/m1a/deps.lock.json`)

| Component | Revision |
|---|---|
| Engine: FFCCEOT-Recomp (origin of ntr-recomp / ntr-runtime) | `934c7434c9a6c2866fea17fa36098ef9c0652fc4` (2026-10-17; upstream HEAD on 2026-11-13) |
| melonDS (engine submodule) | `906e9ebb27da8c6a715cd7abab4abfe8a8d29427` (2026-08-24) |
| Fetched by the engine's CMake | SDL3 `release-3.4.18`, FreeType `VER-2-14-3`, RmlUi `6.3` |
| Our patches (applied by `fetch_engine.py`) | `0001-difftest-stub-load-module.patch` (§3.1), `0002-slider-include-climits.patch` (§3.5), `0003-fiber-android-aarch64-from-ph.patch` (§3.6), `0004-cmake-no-link-group-on-apple.patch` and `0005-fiber-macos-xopen-source.patch` (§3.7) |
| Reference only (not built) | Phantom Hourglass Recomp `27ec7aec` (dguillot-gh), source of patch 0003's context switch (its commit `db0d762`) |
| Toolchain (Windows) | clang-cl 22.1.8, lld-link, MSVC 19.44.35228 (VS 2022 Build Tools, Windows SDK 10.0.26100), CMake 4.4.2, Ninja 1.12.1; Python 3.14.3 + unicorn 2.1.4 |
| Toolchain (Linux x64) | WSL 2 (kernel 6.18), Arch Linux: clang/LLD 23.1.5, GCC/libstdc++ 16, CMake 4.5.0, Ninja 1.13.2, Python 3.14.10 + unicorn 2.1.4. The engine's reference is Ubuntu 22.04 with LLVM 22.1.8 (its Docker image); not that image |
| Toolchain (CI, Linux x64 and arm64) | GitHub `ubuntu-24.04` / `ubuntu-24.04-arm`: Ubuntu clang 18.1.3, CMake 3.31.6, Ninja 1.13.2, Python 3.12.3 + unicorn 2.1.4; packages from the engine's Docker file list; UI smoke under Xvfb (software rendering) |
| Toolchain (CI, macOS arm64) | GitHub `macos-15` (Darwin 24.6): Apple clang 17.0.0, CMake 4.4.5, Ninja 1.13.2, Python 3.15.2 + unicorn 2.1.4 |
| Toolchain (CI, Android arm64) | NDK 27.3.13750724 on `ubuntu-24.04`, `arm64-v8a`, API 28, `-DFFCC_BUILD_TESTS=OFF` |

## 2. Results

| Check | Windows x64 | Linux x64 | Evidence |
|---|---|---|---|
| **V1** engine configures, builds, unit tests pass | **Pass**: engine's `release` preset, 881 build steps in 3 min 15 s, `ctest` 10/10 (incl. UI smoke); again 10/10 after patch 0002 | **Pass with patch 0002**: `linux-release` preset (977 steps), `ctest` 10/10 (UI smoke on WSLg's display) | `results/v1_*.log`, `results/linux/v1_*.log` |
| **V2** fiber conformance (create, switch, 3,000-deep nesting across a switch, FP values across 100 switches bit-identical, per-fiber rounding mode, chains, 5,000 create/destroy) | **Pass** 7/7, Debug and Release identical | **Pass** 7/7 (`ucontext` path) with g++ 16 and clang 23, `-O0` and `-O2` | transcript hash **`be74b658`** on both platforms and all six builds |
| **V3** recompiler difftest (recompiled C vs Unicorn ARM946) | **Pass with patch 0001**: 2 seeds × 14 categories × 300 random instructions = 8,400, 0 mismatches. **Without the patch it crashes on every platform** (§3.1) | **Pass with patch 0001**: the same 8,400, 0 mismatches | `results/v3_*.txt`, `results/linux/v3_*.txt` |
| **Synthetic cartridge through the base** | **Runs**: recompiled (156 functions, 3,817 instructions, 0 warnings), built as a game module, run headless 600 frames with all ARM9 code recompiled (0 instructions on the interpreter). **Deterministic**: two runs, and `/O1` vs `/O2` modules, give identical per-frame state hashes | **Runs**, deterministic, and its 600 per-frame state hashes are **identical to Windows x64**: other OS, compiler (clang 23 vs clang-cl 22) and fiber implementation | `results/zpoc/`, `results/linux/zpoc/` |

### 2.1 All five targets (local runs, and CI run 5 `d414df5`)

| Target | V1 build + engine tests | V2 fibers (hash; expected `be74b658`) | V3 difftest | Cartridge, 600 frames |
|---|---|---|---|---|
| Windows x64 (local) | **Pass**, 10/10 | **Pass**, `be74b658` | **Pass**, 8,400/8,400 | runs; reference hashes |
| Linux x64 (local WSL, and CI) | **Pass**, 10/10 on both | **Pass**, `be74b658` | **Pass**, 8,400/8,400 | **identical to Windows** |
| Linux arm64 (CI) | **Pass** (build); tests 9/10: `ui_smoke` exceeds its 120 s limit, completes in 123 s (§3.10) | **Pass**, `be74b658` (`ucontext`) | **Pass**, 8,400/8,400 | **identical to Windows** |
| macOS arm64 (CI) | **Pass with patches 0004–0005** (build); tests 9/10: `ui_smoke` completes in 215 s (§3.10) | **Fails T5 only**, `6ae6ca29` (§3.8) | **Pass**, 8,400/8,400 | **identical to Windows** |
| Android arm64 (CI, NDK 27, API 28) | **Compiles and links with patch 0003**: `melonds_core`, `ntr_runtime`, `ntr-recomp`, `ntr_run`; engine tests not built (they need a device) | built, **not run on Android** (no device). The same assembly on arm64 Linux: **fails T5 only**, `6ae6ca29` | not run (needs a device) | not run (needs a device) |

- **The cartridge's 600 per-frame state hashes are the same on four targets:** Windows x64, Linux x64, Linux arm64
  and macOS arm64. The same holds across OS, CPU architecture, compiler (clang-cl 22, clang 18 and 23, Apple
  clang 17) and fiber implementation (Win32 fibers, glibc and Apple `ucontext`). Each target also ran it twice with
  identical results. All ARM9 code was recompiled; 0 instructions ran on the interpreter.
- **Reading the CI job status.** A green job means the script reached its end with the build, V3 and the cartridge
  passing. V2 and `ctest` verdicts are only in each job's `summary.txt`, and the table above is taken from those.
- **CI history.** Runs 1–4 found the problems in §3.6–§3.9 and in our scripts; each fix is in its own commit of the
  private repository:
  - run 1: unsupported `macos-14` image, Android `ucontext`, AArch64 FMA;
  - run 2: macOS link group;
  - run 3: macOS `ucontext` declarations;
  - run 4: our macOS module flags, and our diagnosis needing `timeout`.

## 3. Blockers found and what each needed

### 3.1 V3 cannot run upstream (fixed by a 30-line patch)

- **Symptom.** `tests/difftest/difftest.py` dies with "access violation writing 0x0" in `dt_run`, on whatever
  category runs first.
- **Smallest case.** One function, `ands lr, ip, #0x1400000 ; bx lr`:
  ```
  python spike/m1a/v3_min_repro.py --insn e21ce505 --r12 0x723280c3 --lr 0xbc937d7e
  ```
  Run it in a VS x64 developer shell with LLVM on PATH, on an unpatched checkout. The default
  `mov r0,#1 ; bx lr` passes, because it calls no runtime service.
- **Cause.** Since upstream commit `0875cdd` ("The recompiled game is a loadable module", 2026-10-09), generated
  code is a game module: `funcs.h` defines `NTR_GAME_MODULE`, and every runtime service (`ntr_jump`, memory slow
  paths…) goes through the `ntr_host` table, which only `ntr_game_load` fills. `tests/difftest/stub.c` (last changed
  2026-10-07) never calls it, so the first service call jumps through a null pointer. Tests whose instruction
  needs no service pass, which is why the first ~36 did.
- **Fix.** The stub fills an `NtrHostApi` with its own services and calls `ntr_game_load` once. What is compared is
  unchanged. Worth offering upstream; that needs your approval, as it is outward-facing.

### 3.2 The engine's only host is game-specific

The engine's app (`ffcc.exe`) refuses any ROM but FFCC's two dumps, by SHA-1, and needs FFCC's hooks. The runtime
library itself is game-agnostic. **Answer:** `spike/m1a/host/ntr_run.cpp`, a ~100-line headless host that:

- loads any ROM and recompiled module;
- runs N frames;
- writes each frame's state hash (main RAM, both work RAMs, CPU clock) and how many instructions ran on the
  interpreter.

It is added to the engine's own build through `CMAKE_PROJECT_FFCCEOTRecomp_INCLUDE`, without changing any engine
file. It is also the seed of a Spirit Tracks host.

### 3.3 The ROM parser needs the logo checksum

`common/src/rom.cpp` (used by both the recompiler and the runtime) requires `0xCF56` at header offset `0x15C`. Our
synthetic ROM has no logo. `spike/m1a/zpoc_rom.py` writes **only the 2-byte checksum**, never the logo bitmap, and
recomputes the header CRC. The R0 sample stays unchanged.

### 3.4 Not blocking this round, recorded

- **The fallback interpreter cannot run the boot thread.**
  - Smallest case: `ntr_run zpoc.nds - 1 out.txt`, i.e. no module, so every ARM9 instruction should be interpreted.
  - Result: "fatal: guest thread started at 02000000 ran past its exit function 02000000".
  - Cause: the boot thread's return address equals its entry, and the interpreter stops before the first
    instruction. It is designed for small hand-written blocks.
  - So a whole-program "original ARM" baseline does not exist in this engine. For a free-running loop like our
    cartridge's it would not be comparable frame by frame anyway: work per frame depends on cycle accounting. The
    comparison against the original therefore stays per instruction (V3).
- **Clang's two-instruction jump table is not recognised.** For `add rB, pc, #imm ; ldr pc, [rB, rI, lsl #2]` the
  recompiler emits a computed jump into its own function, and its report counts 0 jump tables. That would fall back
  to the interpreter if executed; it was not executed here, because the cartridge's object list starts empty.
  Spirit Tracks uses Metrowerks branch tables, a form the engine documents as handled. A GCC/clang-built V4 test
  cartridge must avoid or exercise this.

### 3.5 Linux x64 (WSL 2, Arch Linux)

- **Environment.** With your approval the WSL Arch system was fully upgraded (`pacman -Syu`: 114 packages
  upgraded) and the build tools installed (48 packages: CMake, Ninja, clang, LLD, X11/XKB/ALSA/Pulse libraries,
  Xvfb). That ran as root through `wsl -u root`: `sudo` asks for a password, which was not used. Log:
  `results/linux_wsl_upgrade.log`.
- **Our host did not link (our fault).** GNU ld reads each static library once, and melonDS calls back into the
  runtime. The engine links its own program as a rescanned group (`LINK_GROUP:RESCAN`); `host/m1a_targets.cmake`
  now does the same.
- **The engine did not compile: a missing include (engine bug).** `game/src/ui/slider.hpp` uses `INT_MIN` without
  `<climits>`. MSVC's headers, and the older libstdc++ of the engine's Ubuntu 22.04 image, include it
  transitively; GCC 16's libstdc++ does not. Building with `-k 0` showed it is the only such error.
  `0002-slider-include-climits.patch` adds the line. It is worth offering upstream (with your approval).

### 3.6 Android: no `ucontext` in bionic (fixed by porting a context switch)

- **Symptom.** `runtime/src/fiber.cpp` does not compile with the NDK: `getcontext`, `makecontext` and
  `swapcontext` are undeclared (bionic does not provide them).
- **Fix.** `0003-fiber-android-aarch64-from-ph.patch` adds an `__ANDROID__ && __aarch64__` branch: Phantom Hourglass
  Recomp's AArch64 context switch (saves x19–x30, sp and d8–d15). It is ported with attribution (dguillot-gh,
  commit `db0d762`, GPL-3.0) in the code and in this report.
- **How it was exercised without a device.** The same assembly runs on any AArch64 CPU. On the arm64 Linux runner,
  `run_posix.sh` enables that branch (`sed`) and runs V2 with it. T1–T4, T6 and T7 pass; T5 fails (§3.8).

### 3.7 macOS: two build fixes

**First: CMake has no link-group rescan for Apple's linker.**

- **Symptom (CI run 2).** Configure stops with: Feature 'RESCAN', specified through generator-expression
  '$<LINK_GROUP>' to link target …, is not supported for the 'CXX' link language. It is raised twice: once for the
  engine's own program (`game/CMakeLists.txt:121`), and once for our host (`host/m1a_targets.cmake`).
- **Cause.** CMake defines the `RESCAN` feature only for GNU-style linkers. Apple's ld64 needs none, because it
  resolves symbols across archives in any order.
- **Fix.** The group is applied `if(NOT MSVC AND NOT APPLE)`: `0004-cmake-no-link-group-on-apple.patch` does this in
  the engine, and `host/m1a_targets.cmake` in our host. On Linux the generated link line is unchanged (checked in
  `build.ninja`).
- **Upstream does not build for macOS.** It has no macOS preset, and its CI builds only on `ubuntu-24.04` and
  `windows-2022`. A first macOS build hitting something like this was expected.

**Second: the `ucontext` routines are hidden on macOS.**

- **Symptom (CI run 3, after the first fix).** Configure passes and the build stops at step 695 of 751. Building with
  `-k 0` shows a single failing file, `runtime/src/fiber.cpp`. The SDK's `ucontext.h` raises "The deprecated ucontext
  routines require _XOPEN_SOURCE to be defined", then `getcontext`, `makecontext` and `swapcontext` are undeclared.
  V3 already passed in that run, since the recompiler does not depend on the runtime.
- **Fix.** `0005-fiber-macos-xopen-source.patch` defines `_XOPEN_SOURCE 600` at the top of `fiber.cpp`, before any
  header, on Apple only. The V2 test had always been built on macOS with the same definition, and it compiled.
- **The routines stay deprecated on macOS.** They still work, as V2 shows, apart from §3.8. Replacing them with a
  context switch of our own is the longer-term answer (a risk the plan already listed).

### 3.8 Finding: the rounding mode is not per fiber on macOS or on the Android path

- **What V2 T5 checks.** A fiber sets the rounding mode upward. After switching back, the main fiber must still see
  round-to-nearest.
- **Result.**
  - **macOS** (`ucontext` from libSystem), at `-O0` and `-O2`: main sees upward.
  - **The Android assembly** (patch 0003, run on arm64 Linux): the same.
  - **Unaffected:** Windows (`CreateFiberEx` with `FIBER_FLAG_FLOAT_SWITCH`), and glibc's `ucontext` on x64 and
    arm64.
  - The transcript hash is `6ae6ca29` instead of `be74b658`. All other checks pass, including bit-identical FP
    values across 100 switches (T4).
- **Cause.** The floating-point control register (FPCR on AArch64) is not part of the switched context. PH's
  assembly saves registers only; Apple's `swapcontext` evidently does the same.
- **Impact today: none found.**
  - The DS's ARM9 has no FPU, so recompiled game code never reads or sets a rounding mode.
  - Nothing in the engine's runtime, recompiler, game layer, or the melonDS sources it builds (no JIT) changes the
    rounding mode (searched for `fesetround`, `fesetenv`, `_controlfp`, `_mm_setcsr` and FPCR).
  - A host library that changed it inside a guest fiber would be the only way to trigger it.
- **Smallest case.** The V2 test itself, `spike/m1a/fiber_conformance.cpp` T5:
  - on macOS: `run_posix.sh`;
  - on arm64 Linux, the Android branch: `run_posix.sh` (it applies the `sed`).
- **Fix (proposed, not applied).**
  - Android branch: save and restore FPCR (`mrs`/`msr fpcr`) in the switch.
  - macOS: the same assembly, or `fegetenv`/`fesetenv` around `swapcontext`.
  - Both are our own change to engine code. Neither blocks R4, which runs on Windows x64.
- **Decision (maintainer, 2026-11-13): a known failure, left open.** The check stays active and is not weakened. The
  fix is listed as a first task in [CONTRIBUTING.md](../../CONTRIBUTING.md).

### 3.9 AArch64 compilers fuse multiply-add unless told not to

- **What happened.** In CI run 1, V2 on arm64 Linux gave transcript hash `2beb81d1` while every x64 build gave
  `be74b658`. The fibers were not at fault.
- **Cause.** Clang on AArch64 contracts `a*b+c` into one `fmadd`, which rounds once instead of twice, and the
  transcript prints FP results. Both the reference computation and the one across switches were fused alike, so T4
  still passed; only the printed values differ.
- **Fix.** The test is built with `-ffp-contract=off` everywhere. It then gives `be74b658` on arm64 too.
- **Lesson for later.** The recompiled game is integer-only, so generated code is unaffected. Any native-side FP that
  must be bit-identical across platforms (for example a future replay that hashes host-computed floats) needs
  `-ffp-contract=off` as well.

### 3.10 Slow but not hung: the UI smoke test on the arm64 runners

- On `ubuntu-24.04-arm` (Xvfb, software rendering) and on `macos-15` (a virtual machine without a GPU), `ui_smoke`
  hits the engine's fixed 120 s limit. It is the only `ctest` failure on either (9/10).
- **Slow, not hung.** `run_posix.sh` then reruns the same UI script with a 900 s limit and counts its screenshots:
  - Linux arm64: exit 0 after 119–123 s (three runs);
  - macOS arm64: exit 0 after 215 s;
  - all 3 of the script's screenshots taken on both.
- On the x64 runner the test passes within the limit. This is an engine test-timeout matter, not a portability
  defect, and we do not change it. On a real Mac with a GPU it would need checking again.

## 4. Platforms

| Platform | Tested | Pending for M1a |
|---|---|---|
| Windows x64 | V1, V2, V3, cartridge (local) | V4–V7 |
| Linux x64 | V1, V2, V3, cartridge (local WSL 2 Arch Linux, and CI on Ubuntu 24.04, the runner image of the engine's own CI) | V4–V7. Not the engine's Docker image (Ubuntu 22.04, LLVM 22) |
| Linux arm64 | V1, V2, V3, cartridge (CI) | V4–V7 |
| macOS arm64 | V1, V2, V3, cartridge (CI) | V2 T5 (known failure, §3.8); V4–V7; a run on a real Mac |
| Android arm64 | V1 compile + link (CI); V2 assembly on arm64 Linux | **V2 on a device or emulator**; V4–V7 (V4 must "compile and run" there) |

**Gate status.** M1a's gate is V1–V3 passing on all five targets and V4 on three. It is **not met yet**:

- **V1–V3 pass in full on three targets:** Windows x64, Linux x64 and Linux arm64.
- **macOS arm64:** V1 and V3 pass, and the cartridge matches. V2 fails one check (T5, the rounding mode). It is a
  known failure, left open with the check active (§3.8). The gate counts macOS as passing V2 only once T5 is fixed.
- **Android:** V1 (compile + link) passes. V2 has run only through its assembly on arm64 Linux, also failing T5. An
  arm64 Android emulator (or a device) is needed.
- **V4–V7 have not started on any target.**

## 5. What this means for R4

R4 runs on Windows x64, where V1–V3 and the cartridge all pass. Nothing on the other targets blocks it.

- **Ready.**
  - Deterministic execution: same build twice, differently optimised modules, and Windows vs Linux all give identical
    per-frame hashes.
  - A function-replacement mechanism in the engine: `hooks.txt` `replace`.
  - Per-frame state hashes and a hash-diff workflow (`tools/scenario.py compare` in the engine).
  - Input recording and replay (`--record-input`, `--input-script`) in the engine's app.
  - A host that runs any ROM headless (`ntr_run`).
- **Needed before R4 (with the dump; M1b/M2 work).**
  - A Spirit Tracks config (`functions/hooks/regions`): from `dsd` via the engine's `tools/import_dsd.py`, plus
    Spirit Tracks' NitroSDK context-switch hooks (`OS_SaveContext`/`LoadContext`/`InitContext`) and DSProtect
    handling.
  - `ntr_run` grown into a Spirit Tracks host, with input replay and hooks.
  - **An adapter between the runtime's CPU/memory (`NtrCpu`, melonDS buffers incl. DTCM) and our guest-layout
    reconstructions.** Today these assume one flat memory image.
  - A call counter on the replacement, to confirm the substitute ran.
  - **A RAM-only hash next to `state_hash`.** `state_hash` includes the CPU clock, and a native replacement changes
    cycle accounting, so a timing-only difference would otherwise look like a divergence.

## 6. Reproduce (Windows x64)

Prerequisites: VS 2022 Build Tools (C++), LLVM 22 at `C:\Program Files\LLVM`, CMake ≥ 3.28, the project's `.venv`
(`tools/re/requirements.txt`), and the synthetic sample built once (`python poc/run_poc.py`).

Linux x64 (any distribution with clang, CMake, Ninja, Python 3, the X11/XKB/ALSA/Pulse development files; WSL works):
`bash spike/m1a/run_linux.sh [work dir] [frames]` runs everything below and compares the cartridge with Windows.

Linux arm64 and macOS arm64: `bash spike/m1a/run_posix.sh [work dir] [frames] [results name]`. Android:
`bash spike/m1a/run_android.sh [work dir] [API level]`, with `ANDROID_NDK_HOME` set. In CI, `spike/m1a/ci/m1a.yml`
runs all four. `python spike/m1a/ci/make_ci_repo.py` assembles the private repository it runs in, from this
folder only.

```
python spike/m1a/fetch_engine.py
spike\m1a\build_windows.cmd
spike\m1a\run_v2_windows.cmd
spike\m1a\run_v3_windows.cmd 300 1
spike\m1a\run_zpoc_windows.cmd 600
```
