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
| Internet access, the first time | Step 2 installs Capstone and step 3 fetches the engine and its dependencies at pinned revisions |
| Disk | about 3 GB for the engine build, 220 MB per module build folder |

## 2. Python dependencies (once)

In the project folder (`cmd` or PowerShell):

```bash
python -m venv .venv
```
```bash
.venv\Scripts\python -m pip install capstone==5.0.7
```

The launcher uses Capstone for our function discovery. For the historical `--functions dsd` comparison builds,
you can additionally install dsd:

```bash
.venv\Scripts\python poc\fetch_tools.py --only dsd
```

This puts [dsd](https://github.com/AetiasHax/ds-decomp) 0.12.1 (MIT) in `.tools\dsd-v0.12.1\`; the build checks its
SHA-256.

## 3. The program (no ROM needed)

```bash
tools\build\build_program.cmd
```

The script:

1. clones the engine (FFCCEOT-Recomp) at the commit in `spike\m1a\deps.lock.json` into `.tools\upstream\ffcc`;
2. checks melonDS's revision and applies our patches (`spike\m1a\patches\`);
3. configures the engine's `release` preset (Ninja, clang-cl, lld-link) and builds the programs into
   `.tools\upstream\ffcc\build\release\`:
   - `recompiler\ntr-recomp.exe`, the recompiler;
   - `st_native.exe`, the program;
   - `st_launcher.exe`, the SDL3 / RmlUi launcher;
   - `ntr_run.exe`, the headless host for the synthetic cartridge.

From a fresh clone this took 131 s on the validation PC. `tools\build\build_program.cmd --tests` also builds
everything else and runs the engine's unit tests: 10 tests, including a UI smoke test that opens a window for a few
seconds.

### Play with the Windows developer launcher

After steps 2 and 3, open `.tools\upstream\ffcc\build\release\st_launcher.exe` directly. You can also use
[Launch.cmd](../Launch.cmd) or create a Windows shortcut to the executable. Choose your `.nds` file using **Browse…**.
The launcher verifies its SHA-1 and enables **Play** for the supported USA/Australia revision 0 dump. The window
can be resized or maximized. Settings move below the ROM controls in a narrow window. Display mode, game window
scale and game language are saved for the next launch. **Link** is the suggested name on the game's new-save
screen. On that screen, type with the PC keyboard, use **Backspace** to delete, or select the whole name with
**Ctrl+A**. The first typed character replaces the suggested name. Click the game's **OK** to confirm.

**Play** prepares the module on the first run using `--functions ours --dsprot original`. Our build keeps the
protected code as stored in the ROM, and the game executes its own checks at runtime. Later launches verify the
ROM and reuse the locally cached module when the ROM, game configuration, recompiler, module ABI and toolchain match.
The **Compilation** section shows how many builds are cached and how much disk space they use. During a build,
the progress bar counts completed stages out of six. The live status shows elapsed time, the discovery pass and
its scan counts, and the count of C files compiled. **Show steps** expands the timing history and **Open log**
opens the latest build log. The individual phase logs and timing file are kept beside it when compilation succeeds.

**Compile** or **Recompile** builds a fresh module without launching the game, even when one is cached. A build
replaces its previous module after it finishes successfully. **Remove cache…** clears launcher-built modules
after confirmation. The ROM, save files, settings and build logs are stored separately.

The game opens in the same window. **Screen layout** defaults to **Large + side**, with a large touch screen and
the full top screen beside it. Portrait windows use the stacked arrangement in this mode. **Classic DS** stacks
both screens without a gap, **Side by side** gives both screens the same size, and **PiP overlay** places
the top screen in the upper-right corner. The mouse starts touches only on the DS touch screen.

The launcher groups these options into **Screens**, **Window** and **Game**. **Esc** and **F1** open the pause menu
with **Screens** and **Window** tabs. **Presentation → Automatic (PC)** composes the narrated prologue's text over
its art and renders the pre-title opening with a central
camera fitted to the window aspect. The projection restores the scene area hidden between the DS screens and
renders it as one continuous picture without stretching to widescreen. It returns to the selected layout at the title. **Original DS** keeps the
two original views. **F2** overrides the cinematic view for the current scene.
**F11** switches between windowed and fullscreen play. Preparation failures point to their log under
`build/launcher/runs/`. Logs and cached modules are local game-derived artifacts.

This alpha runs from the source checkout with OpenGL 3.3 and the installed Python, Visual Studio Build Tools
and LLVM. Packaging the toolchain for a standalone distribution is part of [ADR-0002](architecture/DECISIONS/0002-distribution-model.md).

Developers can check the launcher at four sizes, verify scrolling and overlapping controls, and test file rejection
with `st_launcher.exe --smoke-test`, without a ROM. Pass an existing output directory after `--smoke-test` to save
previews of the empty launcher in desktop and narrow layouts. To exercise preparation and cache reuse directly, run
`.venv\Scripts\python -I platform\windows\launcher.py --rom "C:\path\to\your\Spirit Tracks.nds" --prepare-only`.
Use `st_launcher.exe --game-smoke <rom>` to test module preparation or reuse, then open and close the pause menu
with both shortcuts in windowed and fullscreen sessions and verify that the launcher redraws in the same window.
The test keeps the blank save in memory.

## 4. The game module (command line)

To build the same module configuration as the launcher:

```bash
tools\build\st_build.cmd "C:\path\to\your\Spirit Tracks.nds" build\st --functions ours --dsprot original
```

The output folder must be new or empty. The build takes 75–120 s and writes everything into `build\st\`:

1. **Checks the dump** (SHA-1).
2. **Runs our extractor and function discovery** on the local ROM.
3. **Writes `config\`** from that discovery and the project's configuration for game code `BKIE`.
4. **Recompiles the ARM9 code to C** (`gen\`, about 100 MB), and writes `recomp_report.txt`.
5. **Compiles and links the module** `st_game.dll` (about 49 MB) with the engine's game-module flags, plus `/Brepro`.
6. **Writes `manifest.txt`,** the SHA-256 of every artifact.

### Compare with historical builds

The legacy command-line defaults (`dsd` and `noop`) reproduce the M1b reference. dsd decrypts the checks as a side
effect of extraction, and `noop` can cause the game to remove the train's controls. The launcher uses our
function list and runs the game's original checks. This combination was verified through the first train ride
to Castle Town and a save and reload there. See [DSPROTECT_ORIGINAL_PATH.md](research/DSPROTECT_ORIGINAL_PATH.md) §5.

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
5. **Name keyboard.** Keep Link or edit the name and click **OK**. Click **Yes** to "Is this what you want to name
   your character?". The launcher forwards physical typing to the game's editor on this screen. The CLI option
   `--player-name <name>` sets the suggested name for replays. Use `--player-name melonDS` for older replay hashes.
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
| A forbidden cursor remains over the game | Fixed in the current launcher: menu controls can request an unavailable cursor while disabled; the game now explicitly owns the arrow cursor, restoring menu/text cursors on pause or return. The ROM-free launcher smoke test covers this handoff. |

### Frame pacing diagnostics

The launcher and Esc/F1 pause menu expose independent options under **Screen features**: unified cutscenes,
subtitles over artwork, and hiding the decorative secondary screen in verified save/setup menus. The switches are
saved across launches. See [Dual-screen UX](design/DUAL_SCREEN_UX.md#independent-screen-features-2026-11-18).

For local graphics-context surveys with advanced saves, see
[Continuous dual-screen scene survey](design/DUAL_SCREEN_VALIDATION.md). `--scene-profile <local.csv>` records
numeric graphics metadata; `--scene-stride 1` samples every frame. Keep saves and output below ignored `build/`.

The current Windows alpha primes about 50 ms of source audio at startup and after pause/resume, and wakes the
window when a new frame is ready. This handles short scheduling delays; sustained slow rendering can still starve
audio. The extra single-screen camera remains a software pass. See [Frame pacing](design/FRAME_PACING.md) for measured
limits, the numeric `--frame-profile`/`ST_NATIVE_FRAME_PROFILE` diagnostics and a reproducible comparison procedure.
