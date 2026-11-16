# M1b (local) and M2's first milestone: Spirit Tracks boots to a new game on Windows x64

Date: **2026-11-13**. Machine: the maintainer's Windows 11 x64 PC, with the maintainer's own dump (USA/Australia rev. 0,
`BKIE`, SHA-1 `1f0921b5…`). Everything below was executed. Everything derived from the dump stayed in the git-ignored
`build/`, and no screenshot of the game left this computer:

- generated C, objects and modules;
- saves;
- screenshots and per-frame hash logs.

Nothing was published, no repository was created, and nothing was pushed. How to repeat it: [BUILDING.md](../BUILDING.md).

**Result (Verified).** From power-on the recompiled game reaches every step of the milestone:

1. the opening scene;
2. the title screen;
3. creation of the cartridge save;
4. the file menu;
5. name entry;
6. a saved file;
7. **a new game started** (the prologue, then the first in-game scene).

Along the way:

- **no fatal error**, and **0 instructions on the fallback interpreter** in 15,000 frames;
- **the same per-frame state hashes on every run**, including with modules built independently;
- **the game module is reproducible:** two clean builds are byte-identical in all 489 artifacts.

## 1. Revisions

| Component | Revision |
|---|---|
| Engine (FFCCEOT-Recomp: ntr-recomp, ntr-runtime) | `934c7434c9a6c2866fea17fa36098ef9c0652fc4`, melonDS `906e9ebb…`, SDL3 `release-3.4.18`, FreeType `VER-2-14-3`, RmlUi `6.3` (`spike/m1a/deps.lock.json`, unchanged since M1a) |
| Our engine patches | 0001–0005 from M1a (validated on five targets), plus **0006** (§2.1): `spike/m1a/patches/` |
| dsd | 0.12.1, SHA-256 `5917df26…0932` (checked by the build) |
| Toolchain | clang-cl / lld-link 22.1.8, MSVC 19.44, Windows SDK 10.0.26100, CMake 4.4.2, Ninja (VS 2022), Python 3.14.3 |

## 2. The Spirit Tracks configuration

### 2.1 Functions: dsd, imported by the engine's tool, with one fix (patch 0006)

- **How the list is made.** `dsd init` analyses the dump, and the engine's `tools/import_dsd.py` turns its lists into
  the recompiler's `functions.txt`. The result has 32,524 functions in 114 modules: main, ITCM, DTCM and 111
  overlays with code.
- **The bug: encrypted functions were not marked.** dsd marks the 41 encrypted DS Protect functions in overlay 1, and
  the importer is meant to flag them `!dsprot` so that the recompiler never translates their encrypted bytes as code.
  But dsd 0.12.1 writes the property as `dsprot=0x68` or `dsprot=(0xac,0x2e8b)`, while the importer looks for
  `dsprot_range`. **Without a fix, none of the 41 would have been flagged.**
- **Patch 0006** accepts both forms. It is worth offering upstream, with your approval.
- **Where the list lives.** `functions.txt` is generated from the user's dump on every build, not stored in the
  repository.

### 2.2 Hooks (`game/st/config/bkie/hooks.txt`)

The engine already provides the replacements a NitroSDK game needs. This round found where each one goes in Spirit
Tracks.

**How the addresses were found (Verified).** We looked for instruction shapes in our own R1 extraction, then checked
each match against the runtime's replacement, step by step. All of them are function starts in dsd's list.

| Routine | Address | How it was identified |
|---|---|---|
| `OS_InitContext` | main `0x0202708C` | stores pc+4 at +0x40 and the stack at +0x44; SP aligned below the stack; initial CPSR 0x1F, or 0x3F for Thumb |
| `OS_SaveContext` | main `0x020270F8` | CP context at +0x48; CPSR, SVC stack, r0 = 1, r0–r14, pc+4 |
| `OS_LoadContext` | main `0x02027144` | the reverse, ending in an exception return. Same layout as FFCC's (Load = Save + 0x4C) |
| `OS_IrqHandler` | ITCM `0x01FFBD70` | IME/IE/IF, acknowledges the lowest pending IRQ. Its literals give the IRQ table `0x027E0198` (DTCM) and `OS_IrqHandler_ThreadSwitch` `0x01FFBDC8` (the next routine) |
| DS Protect entry points | ov001 `0x020C0E14`, `…0E8C`, `…0F04`, `…0F7C`, `…0FF4`, `…106C` | the encrypted functions that unencrypted code calls (calls from ov001, ov026 and ov084) |

The six DS Protect entry points are replaced by the engine's `ntr_dsprot_not_detected`, as in the engine's FFCC
configuration. Their bodies were not read, decrypted or analysed. One call from ov017 to an encrypted address was
excluded: ov017 occupies the same addresses as ov001, so that call stays inside ov017.

The IRQ hook needs this game's two addresses. `game/st/hooks/st_hooks.cpp` supplies them to the runtime's
`ntr::nitro::irq_handler`; it is modelled on the engine's `cfie_hooks.cpp`.

### 2.3 Regions

**None needed so far.** Code copied at run time would show up as instructions on the fallback interpreter, and in
15,000 frames the interpreter ran **0**.

### 2.4 The recompiler's report (Verified)

- **Totals:** 32,527 functions (32,524 seeded, 3 discovered), 1,133,085 instructions, 1,081 jump tables. 17 functions
  have warnings, and there are no global warnings.
- **8 undefined instructions reached** in three functions: 4 in ov112, and 4 in ov001 next to the DS Protect block.
  None was executed (the interpreter count stays 0); they need a look when those paths run.
- **14 branches that leave their overlay** (tail calls into another module).
- **Two probe-word warnings:** "overlay ov026 cannot be told apart from ov023" and "… from ov031". Overlays that
  share addresses are identified at run time by probe words, so a call into one of them could reach the wrong one.
  This was harmless up to the first scene. It is open (CONTRIBUTING, first task 5).
- **ov001's probe words lie outside the encrypted block,** so DS Protect's own memory traffic cannot hide ov001.

## 3. Reproducible module build

`tools/build/st_build.py` builds into a new folder. Its steps:

1. check the SHA-1;
2. run dsd, then delete the extraction;
3. write the config;
4. run `ntr-recomp`;
5. compile and link with `clang-cl` / `lld-link`, using the engine's game-module flags;
6. write `manifest.txt`, the SHA-256 of every artifact.

`tools/build/compare_builds.py` compares two builds stage by stage and names the first artifact that differs.

**Run 1, with exactly the engine's flags: the module differs.**

- **Identical:** every dsd list (352), the config (3), the generated C (66 files) and the report.
- **Different:** **all 65 objects and the module.**
- **The first divergent artifact is `obj/func_table.obj`, at byte offset 0x4:** 2 bytes of the COFF object's
  `TimeDateStamp`.
- Every object differs only in those bytes, and the DLL only at offset `0x80`, the PE header's `TimeDateStamp`.
- **Cause:** clang-cl and lld-link write the build time there, as MSVC does, unless given `/Brepro`.
- Evidence (manifests and comparison): `build/repro/run1/`.

**Run 2, with `/Brepro` added to the compile and the link (`st_build.py`, the only change): every artifact identical.**

| Build | Recompiler | Module SHA-256 | vs A |
|---|---|---|---|
| A (clean folder) | built by `build_program.cmd` | `6b3b3a41ca99c57e7c34f886fcd6ba51916acf58625457134601e0d34c5aab05` | — |
| B (clean folder) | same | same | 489 of 489 identical |
| C (clean folder) | rebuilt by the full build with `--tests` | same | 489 of 489 identical |

Each build takes about 75 s: dsd 6 s, recompiler 3 s, compile 63–67 s on all cores, link 0.2 s. The module is
49,076,736 bytes; the generated C is about 100 MB.

The engine build that produced C also ran the engine's unit tests with our programs and patches: **10 of 10 passed**,
including the UI smoke test.

## 4. The program and the boot

**The program.** `app/st_native.cpp` (`st_native`) is our own, about 550 lines. It is added to the engine's build
without changing engine files (`app/st_targets.cmake`, which defers `app/st_program.cmake` to the end of the engine's
configure, so our targets get its C++20 and warning settings).

- **ROM and module:** checks the dump's SHA-1, and loads the module with our hooks.
- **Threads:** the runtime runs on a game thread and the window on the main thread, as in the engine's own program.
- **Picture:** melonDS's software renderer, both screens stacked, integer scaling.
- **Input:** keyboard, mouse as the touch screen, gamepad by button position.
- **Sound:** 48 kHz.
- **Pacing:** 59.83 Hz, with Tab to fast-forward.
- **Save:** written atomically (temporary file, then rename).
- **Scripted mode:** a script of key presses and touches by frame, per-frame state hashes, and screenshots, with or
  without a window.

**The boot, scripted (`tests/rom/boot_new_game.txt`).** From a blank save, with the engine's defaults (ARM9 at 4x,
fast cartridge reads, English):

| Frame | Step | Seen |
|---|---|---|
| 60–900 | Nintendo screen, opening scene (3D on both screens) | yes |
| 1000 | Start skips the opening | title screen, "Touch the screen!" |
| 1100 | touch | "Creating a save file..." (≈ 6 s), then "File created."; the 1 MB save is written |
| 1850 | touch | Select File: two "New Game!" slots |
| 2000 | file 1 | name keyboard, filled with the console's name |
| 2200–2800 | OK, Yes, Right, Yes | "Saving...", "Save complete." |
| 3400–3600 | file 1, Start | file menu (Start / Copy / Erase), then Adventure / Battle / Tag Mode / Options |
| 3800 | Adventure | **the new game starts:** the prologue's story panels |
| 10,500–12,000 | (the prologue ends by itself) | first in-game scene: a dialogue in Link's house, waiting for input (a separate 15,000-frame run) |

**Reproducibility (Verified).**

- **Three runs, identical:** module A twice and module B once, without a window.
- **Per frame:** all 6,200 state hashes are identical across the three runs. Each hash covers main RAM, both work
  RAMs and the CPU clock.
- **Pictures:** the 10 screenshots are identical byte for byte.
- **Interpreter:** 0 instructions.
- **Speed:** about 31 s for 6,200 frames, roughly 3.3 times real time on this machine.

**Window (partly verified).**

- **Verified:** 600 frames ran in about 10 s, so the pacing holds 59.8 Hz. Sound opened without error, the save file
  was created, and the frames handed to the window are the same pictures.
- **Not verified by me:** I did not look at the window itself, did not hear the sound, and did not press a gamepad.
  The mouse-to-touch mapping was checked in code only. The guide in BUILDING.md §5 is the human check.

## 5. Findings and known issues

| | Status |
|---|---|
| The engine's import misses dsd 0.12.1's encryption flags | fixed by patch 0006 (§2.1) |
| Module builds are not byte-reproducible with the engine's flags (timestamps) | fixed with `/Brepro` in our build (§3) |
| Top screen black during the first in-game dialogue | **Likely normal** (2026-11-14): the top screen is black in those cutscene dialogues and shows the map once play starts (`tests/rom/early_gameplay.txt`); still to compare with melonDS |
| Save creation takes about 6 s the first time | Likely the DS's own pace: the engine's fast saving is an FFCC hook that we did not port |
| The name keyboard proposes "melonDS" | the console nickname of melonDS's generated firmware; the player types a name. The runtime has a `console_name` option, not used yet |
| RTC fixed at melonDS's default date | needed for identical replays; the game's clock is not the real one |
| ov026 / ov023 / ov031 probe-word warnings; 8 undefined instructions | not reached so far (§2.4) |
| V2 check T5 (rounding mode per fiber) fails on macOS and the Android path | **known failure, check kept active** ([M1A_SPIKE.md §3.8](M1A_SPIKE.md)) |

## 6. What M1b's definition still lacks

- **The private repository and CI** for the program. You asked for local review only, so none was created.
- **A Linux build of the module and program, and reproducibility there.** The engine builds on Linux (M1a);
  `st_build.py` is Windows-only.
- **A second person or a clean VM.** In their place, BUILDING.md was followed step by step in a separate copy that
  held only the files a repository would hold (no `.tools`, `build`, `.venv` or private files), on the same PC:
  - **step 2:** venv and dsd download, 11 s (SHA-256 verified);
  - **step 3:** a fresh engine clone, all six patches applied, build, 131 s;
  - **step 4:** the module, 123 s: **the same SHA-256 `6b3b3a41…`, all 489 artifacts identical to build A**;
  - **the scripted boot:** 6,200 frames with **the same per-frame hashes** and the same 10 screenshots as the
    reference run, and 0 instructions on the interpreter.
  
  The copy was then deleted.

