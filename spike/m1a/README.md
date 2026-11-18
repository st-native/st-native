# M1a portability spike

Builds the recompilation base (FFCCEOT-Recomp's ntr-recomp / ntr-runtime) at pinned revisions and runs the M1a
checks. Report: [docs/research/M1A_SPIKE.md](../../docs/research/M1A_SPIKE.md). Third-party code is fetched into
`.tools/upstream/` (git-ignored); raw results go to `results/` (git-ignored).

| File | What |
|---|---|
| `deps.lock.json` | exact upstream revisions (engine commit, melonDS submodule, tags the engine's CMake fetches) |
| `fetch_engine.py` | clone at the locked commits, verify them, apply `patches/` |
| `patches/0001-difftest-stub-load-module.patch` | makes the engine's recompiler difftest (V3) run again (it crashes upstream since the game-module ABI change) |
| `patches/0002-slider-include-climits.patch` | a missing `<climits>` that breaks the engine's build with GCC 16's libstdc++ (Linux) |
| `patches/0003-fiber-android-aarch64-from-ph.patch` | fibers on Android (bionic has no `ucontext`): Phantom Hourglass Recomp's AArch64 context switch (dguillot-gh, commit `db0d762`, GPL-3.0), ported with attribution |
| `patches/0004-cmake-no-link-group-on-apple.patch` | the engine's `$<LINK_GROUP:RESCAN,...>` stops CMake's configure on macOS (no RESCAN for Apple's linker, which needs none): applied on Linux only |
| `patches/0005-fiber-macos-xopen-source.patch` | macOS declares its (deprecated) `ucontext` routines only with `_XOPEN_SOURCE`: defined at the top of the engine's `fiber.cpp`, on Apple only |
| `patches/0006-import-dsd-dsprot-0-12.patch` | the engine's `tools/import_dsd.py` flags DS Protect's encrypted functions only in an older dsd's syntax; dsd 0.12.1 writes `dsprot=` (M1b, [report](../../docs/research/M1B_FIRST_BOOT.md) §2.1) |
| `patches/0007-runtime-ram-hash.patch` | `Frame::ram_hash`: the per-frame hash of the emulated memory without the CPU clock, next to `state_hash`, so replays can tell a timing difference from a memory difference (RISKS 33) |
| `patches/0008-recompiler-interpret-ranges.patch` | `interpret.txt`: guest code ranges the recompiler never translates; calls into them run on the runtime's interpreter (the game's anti-piracy block, run as the game decrypts it; [report](../../docs/research/DSPROTECT_ORIGINAL_PATH.md)) |
| `patches/0009-runtime-interpreter-calls-recompiled.patch` | interpreted code that reaches a recompiled or hooked function runs its recompiled version (the runtime's thread and IRQ replacements stay in force); code that starts in an interpreted range stays purely interpreted |
| `patches/0010-runtime-lone-overlay-candidate-residency.patch` | the runtime used an overlay's function whenever it was the only one at an address, even when another overlay was resident; it now checks residency (an engine bug, worth offering upstream) |
| `patches/0011-runtime-wireless-group.patch` | `NTR_WIRELESS_GROUP=<name>` gives a process a local-wireless group of its own (Windows). Copies running at once otherwise take consecutive instances with different MAC addresses, which the game sees, so scripted runs side by side played differently from a run alone; `st_native --wireless-group` sets it |
| `patches/0012-recompiler-arm-offset-switch.patch` | ARM switches whose index is computed just before the jump (`cmp rY,#n; bgt default; subs rX,rY,#k; addpl pc,pc,rX,lsl #2`) are bounded by `n - k + 1`; the matcher only knew `cmp` on the index itself, so such a table fell back to "every later word of the function", which can be too short (one entry where fourteen were needed: `fatal: jump table ... unexpected target` on entering Hyrule Castle's grounds) or far too long. A recompiler bug, worth offering upstream |
| `patches/0017-graphics-context-inspection.patch` | read-only graphics-context diagnostics: projection summaries and display registers, cached on demand. No game-specific scene policy or protected-code inspection; used by the local [dual-screen survey](../../docs/design/DUAL_SCREEN_VALIDATION.md) |
| `patches/0018-cinematic-frustum-span.patch` | generic explicit vertical frustum union and centered-perspective world policy; preserves legacy symmetric-camera defaults. Game profiles remain outside the engine |
| `patches/0019-frame-foreground-planes.patch` | optional read-only software foreground planes for host composition; display-register queries can skip projection parsing. No extra render pass or game-specific policy |
| `fixtures/zpoc_synthetic.rom`, `fixtures/zpoc_hashes_windows_x64.txt` | the synthetic cartridge as built on Windows (our own code; SHA-1 `faacbe28…`) and its 600 per-frame state hashes there, the cross-platform reference |
| `run_posix.sh`, `run_linux.sh`, `run_android.sh` | Linux/macOS: everything (fetch, V1, V2, V3, cartridge vs the Windows reference); Android: NDK compile + link |
| `ci/m1a.yml`, `ci/make_ci_repo.py` | the CI workflow and the script that assembles the private CI repository (only this folder) |
| `build_windows.cmd` | V1: configure, build, `ctest` with the engine's `release` preset |
| `fiber_conformance.cpp`, `run_v2_windows.cmd` | V2: fiber conformance test against the engine's `runtime/src/fiber.*`; prints a transcript hash that must match on every platform |
| `run_v3_windows.cmd`, `v3_by_category.cmd`, `v3_min_repro.py` | V3: the engine's difftest; per-category run; one-function reproduction of the upstream crash |
| `zpoc_rom.py`, `make_zpoc_config.py`, `zpoc_config/` | the synthetic cartridge (poc/sample, our own code) as the engine accepts it, and its recompiler config |
| `host/ntr_run.cpp`, `host/m1a_targets.cmake` | a generic headless host for the runtime (any ROM, N frames, per-frame state hashes), added to the engine's build without changing engine files |
| `run_zpoc_windows.cmd` | the cartridge end to end: recompile, build the module, build the host, run twice with the module and once without |
