# Contributing

This project is not public yet. These notes are for the first collaborators and will grow before publication.
The primary repository is [st-native/st-native on GitHub](https://github.com/st-native/st-native).
Use pull requests there; the previous GitLab repository is retained as a backup.

Read first: [README.md](README.md), [docs/BUILDING.md](docs/BUILDING.md), [AGENTS.md](AGENTS.md) (it binds AI
assistants and humans alike).

## Ground rules

- **No game data, ever.** Never commit, attach or paste any of these:
  - ROMs and anything extracted from them;
  - recompiler output (`build/`), game modules, saves;
  - screenshots or recordings of the game;
  - disassembly or decompiled game code.
  
  The project does not reconstruct game code (ADR-0008): changes to the game's behaviour are small hooks in our own
  code at verified addresses.
- **Do not use zeldaret/st.** Do not read or use it, its issues or its wiki: its maintainers do not accept AI-agent
  use, and this project is AI-assisted (ADR-0004).
- **Keep licences and attribution.** Code from FFCCEOT-Recomp or Phantom Hourglass Recomp keeps its attribution:
  repository, commit and author.
- **One focused change at a time,** with its tests. Say what you ran, and what you could not test and why.
- **Label claims in documents** Verified, Likely, Unknown or Proposed.

## Tests anyone can run (no ROM)

All of these use our own synthetic cartridge or generated data, never the game.

| What | Command | Expected |
|---|---|---|
| Python unit tests (codecs, discovery, cache and patch-stack checks) | `.venv\Scripts\python -I -m unittest discover -s tests -t .` | All applicable tests pass (12 tests, one skipped in the current Windows environment) |
| The engine/app unit tests, built with our programs and patches | `tools\build\build_program.cmd --tests` | All configured tests pass (17 in the current Windows build) |
| V2: the fiber context switch, against the engine's `fiber.cpp` | `spike\m1a\run_v2_windows.cmd` | transcript hash `be74b658` in Debug and Release |
| V3: recompiled C against a reference ARM946 (Unicorn), 8,400 random instructions | `spike\m1a\run_v3_windows.cmd 300 1` and `… 300 2` | 0 mismatches |
| The synthetic cartridge through the recompiler and runtime, 600 frames | `spike\m1a\run_zpoc_windows.cmd 600` | identical per-frame state hashes on every run, equal to `spike\m1a\fixtures\zpoc_hashes_windows_x64.txt` |
| The same on Linux or macOS | `bash spike/m1a/run_posix.sh` | as above; V2 on macOS fails check T5 (known, below) |

**Known failure, left active on purpose: V2 check T5** (the rounding mode is not kept per fiber) fails on macOS and on
the Android context switch. The engine never changes the rounding mode, so nothing depends on it today
([M1A_SPIKE.md §3.8](docs/research/M1A_SPIKE.md)). Do not disable the check; a fix is welcome (first tasks, below).

**With your own ROM** (Windows x64): [docs/BUILDING.md](docs/BUILDING.md) §6 covers two clean module builds that
must be identical, and the scripted boot to a new game (`tests/rom/boot_new_game.txt`) that must give identical
per-frame hashes on every run. `tests/rom/first_scene.txt` goes on into the first scene (17,400 frames), and
`tests/rom/early_gameplay.txt` into the first minutes of play (31,000 frames); `tests/rom/castle_town.txt` takes the
first train ride to Castle Town and saves there (52,400 frames, needs a `--dsprot original` module and a separate test
save), and `tests/rom/castle_town_load.txt` reloads that save. Run scripted comparisons one at a time: each copy of
`st_native` running at once gets its own local-wireless instance and MAC address, so their memory differs from frame
7. A hook or
enhancement is checked the same way: off, the hashes equal the original's; on, the difference is documented. Report
results as counts and hashes only.

## First tasks

Each one is self-contained and needs no ROM unless marked.

1. **T5: keep the rounding mode per fiber** on the Android context switch (save and restore FPCR in the assembly of
   patch 0003) and on macOS. Done when V2 gives `be74b658` on arm64 Linux with the Android path, and in CI on macOS.
2. **Validate `st_native` on Linux with a local ROM.** GitHub Actions sets `ST_NATIVE_BUILD_APP=1` to include the
   CLI program and app tests in the Linux x64/arm64 build. Check those results, then verify that a local scripted
   real-game boot gives the same hashes as on Windows. The launcher is still Windows-only. (ROM)
3. **Reproducible module build on Linux** (`tools/build/st_build.py` is Windows-only today: clang-cl, lld-link,
   `.dll`). Use the engine's Linux module flags, and check two clean builds are identical. (ROM)
4. **A function list that decrypts nothing, at dsd's level.** `st_build.py --functions ours` uses our extractor and
   discovery instead of dsd (whose extractor decrypts the anti-piracy routines). Compare its list with a dsd build
   by the scripted boot: it reaches the new game, but runs about a million instructions on the interpreter (the runtime
   names each `interpreter: first entry`). Turn those into seeds by improving our discovery
   (docs/research/DSPROTECT_ORIGINAL_PATH.md).
   (ROM)
5. **Retire the probe-word warnings** ("overlay ov026 cannot be told apart from ov023/ov031"). Find out from the
   recompiler's choice of words whether these overlays are interchangeable at run time, or whether a call could reach
   the wrong one. (ROM: report addresses only)
6. **A presentation option:** screens side by side, or a gap between them, in `app/st_native.cpp`, following
   [docs/design/DUAL_SCREEN_UX.md](docs/design/DUAL_SCREEN_UX.md).
7. **Gamepad check:** the mapping in `app/st_native.cpp` is untested on real controllers. Report which ones work.
