# Rules for AI coding agents

This project (working name **Spirit Tracks Native**, code identifier `st-native`; the public name is not decided — do
not introduce another name) is an unofficial cross-platform **port by static recompilation** of *The Legend of Zelda:
Spirit Tracks*, developed with AI assistance under human review (ADR-0008). Read `docs/architecture/ARCHITECTURE.md`
and the ADRs in `docs/architecture/DECISIONS/` before changing code.

## Never
- Commit, upload, print or paste game data: ROMs, extracted files or assets, generated recompiler C, built game
  modules, saves, save states, game screenshots, disassembly or decompiled game code, reconstructed game source. Run
  `python -I tools/check_no_game_data.py --staged` before every commit, and `--tree` on every publication bundle.
- Reconstruct game functions in C/C++ or extend the retired reconstruction (ADR-0008). Its artefacts stay private
  (below); do not copy them into the app, the runtime, the hooks or anything published.
- Read, analyse or summarise **zeldaret/st** (its maintainers reject AI-agent use; ADR-0004). Do not fetch it, its
  issues or its wiki. If a task seems to need it, stop and ask the maintainer.
- Copy code from repositories without a compatible licence (Shipwright, the Spirit Tracks D-Pad Patch) or of unknown
  provenance (Phantom Hourglass Recomp's embedded blow sample).
- Push, publish, create repositories, accounts or releases, or change hosting settings (GitHub, GitLab). The
  maintainer creates the GitLab.com account and publishes after reviewing the local bundle (docs/PUBLISHING.md).
- Claim a test passed that was not run. Say what could not be tested and why.

## Always
- Keep the engine game-agnostic; Spirit Tracks code goes in `game/st/` (config and hooks); OS-specific code goes
  behind `platform/` or the engine's internal shims. Engine changes are patches in `spike/m1a/patches/`, each small,
  explained and worth offering upstream.
- Never call a graphics API from layout, settings, UI logic or game code; go through the present backend interface.
- Keep every target platform buildable (Windows x64, Linux x64, Steam Deck, Android ARM64, macOS Apple Silicon); do not
  add a dependency or feature that rules one out without an ADR.
- Do in the app or the runtime whatever does not need the game's logic; when it does, write a small hook
  (`hooks.txt` `replace`/`before`/`instead`) or read verified game state (docs/design/ENHANCEMENTS.md). Treat game
  addresses as facts to verify against the recompiled code and a local ROM; record how each was found next to it.
- Check every hook and enhancement with a scripted replay (`tests/rom/*.txt`, `st_native --hashes`, `tools/build/compare_runs.py`): off must give
  the original per-frame hashes; on, a documented difference. A hook that replaces a game function must charge the
  recompiled function's guest cycles, or replays diverge for timing alone (RISKS 33).
- Make focused changes with tests; label claims Verified / Likely / Unknown / Proposed in docs.
- Preserve upstream attribution when porting code from FFCCEOT-Recomp or Phantom Hourglass Recomp (cite repo, commit,
  author).
- Everything built from a dump goes to `build/` (git-ignored): generated C, modules, saves, screenshots, hash logs
  (docs/BUILDING.md). `functions.txt` is generated on every build, never stored; `game/st/config/<code>/` holds only
  hooks, regions and interpret ranges (addresses and public SDK names).

## The anti-piracy routines (DS Protect, overlay 1)
- Do not decrypt, analyse, reconstruct or trace them instruction by instruction. Prefer build paths that decrypt
  nothing (`st_build.py --functions ours`; dsd's extractor decrypts them as a side effect).
- The default build replaces their six entry points with the engine's "genuine cartridge" answer; the experimental
  path runs the game's own code on the interpreter (`--dsprot original`, docs/research/DSPROTECT_ORIGINAL_PATH.md).
  With the replacements the game removes the train's controls on the first train ride, so sessions past Aboda use
  `--dsprot original` (`tests/rom/castle_town.txt`).
  Legal questions Q4 and Q14 are open.

## Historical research (R0-R4, retired 2026-11-14)
- Private and local, never published: `game/st/` except `config/` and `hooks/`; `game/st/runtime/`;
  `tools/build/st_recon.*`; the reconstruction harness in `tools/re/` (everything except `ndsrom.py`,
  `blz.py`, `discover.py`); `poc/` except `poc/sample/`; the R0-R4 reports, `SPIRIT_TRACKS_ANALYSIS.md`,
  `research_notes/`, `reports/`, `docs/legal/` (until the maintainer decides) and `poc/out/`
  (docs/research/HISTORICAL_RESEARCH.md).
- Game-derived files (extracts, Ghidra projects, asm/C exports) stay in git-ignored locations and are never sent to
  external services.
