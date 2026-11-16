# Building and running

**Validated path: Windows x64 only** (Windows 11, 2026-11-13). Every step below was run as written:

- from a fresh clone of the engine;
- the game module twice, into two clean folders, with identical results;
- the game booted from power-on to a new game, three times with identical per-frame state hashes.

Details and evidence: [docs/research/M1B_FIRST_BOOT.md](research/M1B_FIRST_BOOT.md). Linux, macOS and Android have
build results for the engine only ([M1A_SPIKE.md](research/M1A_SPIKE.md)). The program (`st_native`) has not been
built on them yet.

The program plays **your own dump** of the game. Nothing in this repository contains game code or data. The steps
derive files from your dump into `build/` (git-ignored): the recompiled C, the game module, saves and screenshots.
Keep them to yourself: never commit, upload or share them.

## 1. What you need

| | Validated with |
|---|---|
| Your dump of *Spirit Tracks*, USA/Australia, revision 0 (game code `BKIE`) | SHA-1 `1f0921b5e99db9d92d24710f1b004e4f3c997b73`, 134,217,728 bytes. Other dumps are refused (only this release has addresses so far) |
| Windows 10/11 x64 | Windows 11 Pro 26300 |
| Visual Studio 2022 Build Tools, workload "Desktop development with C++" (Windows SDK, and the Ninja it bundles) | MSVC 19.44, Windows SDK 10.0.26100 |
| LLVM, installed at `C:\Program Files\LLVM` (clang-cl, lld-link) | 22.1.8 |
| CMake ≥ 3.28, Git, Python ≥ 3.11 | CMake 4.4.2, Git 2.53, Python 3.14.3 |
| Internet access, the first time | step 2 downloads dsd; step 3 clones the engine and its dependencies (melonDS, SDL3, FreeType, RmlUi) at pinned revisions |
| Disk | about 3 GB for the engine build, 220 MB per module build folder |

## 2. Python and dsd (once)

In the project folder (`cmd` or PowerShell):

```bash
python -m venv .venv
```
```bash
.venv\Scripts\python poc\fetch_tools.py --only dsd
```

This puts [dsd](https://github.com/AetiasHax/ds-decomp) 0.12.1 (MIT) in `.tools\dsd-v0.12.1\`; the build checks its
SHA-256. The build steps themselves need only Python's standard library.

## 3. The program (no ROM needed)

```bash
tools\build\build_program.cmd
```

The script:

1. clones the engine (FFCCEOT-Recomp) at the commit in `spike\m1a\deps.lock.json` into `.tools\upstream\ffcc`;
2. checks melonDS's revision and applies our patches (`spike\m1a\patches\0001`–`0006`);
3. configures the engine's `release` preset (Ninja, clang-cl, lld-link) with our programs added, and builds three of
   them into `.tools\upstream\ffcc\build\release\`:
   - `recompiler\ntr-recomp.exe`, the recompiler;
   - `st_native.exe`, the program;
   - `ntr_run.exe`, the headless host for the synthetic cartridge.

From a fresh clone this took 131 s on the validation PC. `tools\build\build_program.cmd --tests` also builds
everything else and runs the engine's unit tests: 10 tests, including a UI smoke test that opens a window for a few
seconds.

## 4. The game module (from your ROM)

```bash
tools\build\st_build.cmd "C:\path\to\your\Spirit Tracks.nds" build\st
```

The output folder must be new or empty. The build takes 75–120 s and writes everything into `build\st\`:

1. **Checks the dump** (SHA-1).
2. **Runs dsd's analysis.** dsd extracts the ROM, then `dsd init` writes the function lists. The extraction is deleted
   at once. It contains every game file, and dsd decrypts the anti-piracy routines while extracting (legal question
   Q14 is open). Only dsd's function lists are kept.
3. **Writes `config\`:**
   - `functions.txt`, from dsd's lists by the engine's `tools/import_dsd.py`;
   - our `hooks.txt` and `regions.txt`, from `game\st\config\bkie\`.
4. **Recompiles the ARM9 code to C** (`gen\`, about 100 MB), and writes `recomp_report.txt`.
5. **Compiles and links the module** `st_game.dll` (about 49 MB) with the engine's game-module flags, plus `/Brepro`.
6. **Writes `manifest.txt`,** the SHA-256 of every artifact.

### Experimental build options

- `--functions ours`: the function list comes from our own extractor and discovery instead of dsd, so nothing is
  decrypted (dsd's extractor decrypts the anti-piracy routines as a side effect). Boots to the new game; the
  functions it still lacks run on the interpreter (slower, other timing).
- `--dsprot original`: runs the game's own anti-piracy checks on the runtime's interpreter instead of replacing them.
  Reaches the new game like the default build (patches 0008/0009), and is **needed past the first train ride**: with
  the replacements the game removes the train's controls. With `--functions ours`, it plays from power-on to Castle
  Town, saves and reloads (`tests/rom/castle_town.txt`, [DSPROTECT_ORIGINAL_PATH.md](research/DSPROTECT_ORIGINAL_PATH.md) §5).

## 5. Play: from the title screen to a new game

```bash
.tools\upstream\ffcc\build\release\st_native.exe "C:\path\to\your\Spirit Tracks.nds" --module build\st\st_game.dll
```

A window opens with both screens, top above bottom, and the console window shows the program's log.

| DS | Keyboard | Gamepad (by position) |
|---|---|---|
| Touch screen | left mouse button on the bottom screen | — |
| D-pad | arrow keys | D-pad or left stick |
| A / B / X / Y | X / Z / S / A | right / bottom / top / left face button |
| L / R | Q / W | shoulder buttons |
| Start / Select | Enter / Backspace | Start / Back |
| (program) fast forward, full screen | Tab (hold), F11 | — |

The guide below starts from a new save, the first time you run the program. Every step was checked with the scripted
run in §6.

1. **Nintendo screen, then the opening scene** with the train. Press **Enter** (Start) to skip it.
2. **Title screen**, "Touch the screen!". Click the bottom screen.
3. **"Creating a save file..."** The first time only, this takes about 6 seconds. Then "File created.": click the
   message.
4. **Select File**, with two slots showing "New Game!". Click slot **1**.
5. **Name keyboard.** The console's name is filled in: type your own on the on-screen keyboard if you like, then click
   **OK**. Click **Yes** to "Is this what you want to name your character?".
6. **Stylus hand.** Click **Right** (or Left), then **Yes**.
7. **"Saving..."**, then "Save complete.": click the message.
8. **Select File** again. Slot 1 now has your name and three hearts: click it, then click **Start**.
9. **Adventure / Battle / Tag Mode / Options.** Click **Adventure**.
10. **The prologue** starts ("This is a tale from long ago..."): **the new game has begun.** After it, the first scene
    (a dialogue in Link's house) waits for a click or **X** (A).

The save is written atomically, as `BKIE.sav` in `%APPDATA%\st-native\st-native\`, each time the game saves. To start
over from a blank cartridge, pass `--no-save`, or delete that file. Every option is listed at the top of
`app\st_native.cpp`.

## 6. Checking your build

**Two clean module builds give identical files.**

```bash
tools\build\st_build.cmd "C:\path\to\your\Spirit Tracks.nds" build\st-a
```
```bash
tools\build\st_build.cmd "C:\path\to\your\Spirit Tracks.nds" build\st-b
```
```bash
.venv\Scripts\python -I tools\build\compare_builds.py build\st-a build\st-b
```

`compare_builds.py` compares every artifact by stage: dsd lists, config, generated C, report, objects, module. Expected
output: "every artifact is identical". Otherwise it names the first artifact that differs, and where it differs (line,
byte offset, PE header field).

**Reproducible boot.** `tests\rom\boot_new_game.txt` scripts §5 from power-on to the prologue. It runs without a
window, as fast as the PC allows (about 30 s for 6,200 frames), from a blank save. Each line of the hash file holds the frame number, the state hash (main RAM, work RAMs and the
CPU clock), the instructions run on the fallback interpreter, and the same memory's hash without the clock. The run
also writes ten screenshots:

```bash
.tools\upstream\ffcc\build\release\st_native.exe "C:\path\to\your\Spirit Tracks.nds" --module build\st-a\st_game.dll --no-save --headless --script tests\rom\boot_new_game.txt --hashes build\boot1.txt --shots build\boot1
```

Run it again into `build\boot2.txt`, then compare the two:

```bash
fc /b build\boot1.txt build\boot2.txt
```

Expected: `FC: no differences encountered`, 6,200 lines, and 0 instructions on the fallback interpreter (the
program's last line).
Compare the pictures in `build\boot1\` with the steps in the script's comments.

## 7. Troubleshooting

| Message | Meaning |
|---|---|
| `'vswhere.exe' is not recognized` | harmless: printed by Visual Studio's environment script |
| `SHA-1 … is not a known Spirit Tracks dump` | another release, a trimmed or modified dump, or not the game |
| `missing tool: …` | step 2 or 3 was skipped, or LLVM is not at `C:\Program Files\LLVM` |
| `the game module needs the hook …` | the module and the program come from different revisions: rebuild both |
| `the runtime stopped: …` | a fatal error in the emulated console. Report the message, the frame, and the steps that led there (no screenshots or files from the game) |
