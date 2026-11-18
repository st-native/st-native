# ADR-0008: A port by static recompilation; source reconstruction retired

- Status: **Accepted** (2026-11-14, maintainer's decision). Supersedes [ADR-0001](0001-engine-foundation.md) rev. 3.
- Related: [ADR-0002](0002-distribution-model.md) (build on the player's machine; accepted with this record),
  [ADR-0004](0004-function-boundary-provenance.md) (function boundaries), the maintainer's legal research of 2026-11-13 (private).

## Context

ADR-0001 rev. 3 made the project an AI-driven source reconstruction of Spirit Tracks, with the static recompiler as a
temporary scaffold to be removed (R7). The work since then (R0–R4, M1a, M1b local, M2's first milestone) showed two
things:

- **The recompiled scaffold alone already plays the game.** From the player's own USA rev. 0 dump, the module is
  generated and compiled on the player's PC and boots through the title screen and file creation to a new game, with
  0 instructions on the fallback interpreter and byte-identical rebuilds ([M1B_FIRST_BOOT.md](../../research/M1B_FIRST_BOOT.md)).
- **Publishing reconstructed game source has no legal footing** in the US, Brazil or the EU (a translation of the
  program, i.e. a derivative work), while a port that contains no game code and builds from the player's own dump
  follows the model courts accepted for emulators and that comparable projects use (legal research, 2026-11-13).

## Decision

1. **Spirit Tracks Native is a port by static recompilation.** The game runs as C generated from the player's own
   ROM by the recompiler, compiled on the player's machine into a game module, on the runtime (melonDS for the rest
   of the console). There is no plan to remove the recompiler.
2. **Source reconstruction is retired from the active plan.** No new functions are reconstructed in C++; there is no
   full decompilation; milestones that depended on it (R5 subsystem waves, R6 native SDK layer as a replacement for
   recompiled code, R7 scaffold removal) are dropped. R0–R4 are recorded as private historical research.
3. **Its artefacts stay private and local:** the 42 reconstructed functions, their manifest and verification results
   (`game/st/` except `config/` and `hooks/`), the reconstruction harness and bridge, and the research reports that
   quote game code. They are never published.
4. **Game-specific behaviour is written as hooks** (`hooks.txt` `replace`/`before`/`instead`, as the engine does for
   FFCC): small, targeted C/C++ functions in our own code that call into or adjust the recompiled game at named
   addresses. They are our code, not translations of the game's functions; each records how its address was found.
5. **The player selects their own ROM in the launcher** (USA/Australia rev. 0 only, checked by SHA-1). Generation and
   compilation of the game module happen on the player's computer (ADR-0002). The ROM, extracted assets, generated
   C, the compiled module and the reconstructed C++ never leave the player's or maintainer's machine.
6. **What is published:** the application, the runtime, the recompiler (the engine, with our patches), the build and
   verification tools, the synthetic tests and the documentation. Current destination: the private GitHub repository
   `st-native/st-native` (maintainer-requested migration, 2026-11-18); GitLab remains a backup. The maintainer reviews
   and authorizes publication of the source bundle.

## Consequences

- **Scope of game integration.** Enhancements are done in the app/runtime wherever possible; where the game's logic
  must change (e.g. a cutscene laid out for one screen), the change is a hook at a verified address
  ([ENHANCEMENTS.md](../../design/ENHANCEMENTS.md) lists which is which).
- **Platforms.** Every target needs a compiler on the player's device (ADR-0002's per-platform toolchain table);
  Windows x64 first.
- **The anti-piracy routines.** The Windows developer launcher explicitly uses `--functions ours --dsprot original`.
  Our extractor keeps the protected code as stored, and the game runs its own checks during play (patches 0008/0009).
  The legacy command-line defaults remain available for reference runs. See
  [DSPROTECT_ORIGINAL_PATH.md](../../research/DSPROTECT_ORIGINAL_PATH.md) for test coverage and open questions Q4/Q14.
- **Legal profile.** The published material contains no game code or data; the remaining questions (anti-circumvention,
  facts files, engine built for one game) go to counsel before publication.
- **Verification** stays per build (identical modules from clean folders) and per run (identical per-frame state
  hashes for a scripted session), now as product regression checks.

## Verification

- `tools/check_no_game_data.py --tree <publication bundle>` passes.
- A fresh clone builds the program without a ROM; with the ROM, the module builds reproducibly and the scripted boot
  replays identically (docs/BUILDING.md §6).
