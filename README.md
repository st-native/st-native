# Spirit Tracks Native (working title)

An unofficial **port by static recompilation** of *The Legend of Zelda: Spirit Tracks* (Nintendo DS, 2009). You
select your own legally obtained game dump in the launcher; the program translates the game's ARM code into C and
compiles it **on your computer** into a game module, which runs on a runtime that provides the rest of the console
(melonDS). The project ships no game code or data: no ROM, no assets, no generated code, no compiled module
([ADR-0008](docs/architecture/DECISIONS/0008-static-recompilation-port.md), [ADR-0002](docs/architecture/DECISIONS/0002-distribution-model.md)).

Targets: Windows x64 first, then Linux x64, Steam Deck / SteamOS, Android ARM64 and macOS Apple Silicon (not released
simultaneously). Goals: remappable controls, flexible layouts for the two screens, selected cutscenes adapted to one
screen and other modernisations, higher internal resolution, widescreen, smoother motion without speeding up the
game, and mods ([ENHANCEMENTS.md](docs/design/ENHANCEMENTS.md)). "Spirit Tracks Native" is a working name.

> **Status (2026-11-14): the game boots to a new game on Windows x64, from the player's own dump.** The module is
> generated and compiled locally and reproducibly (two clean builds are byte-identical); a scripted run from power-on
> through the title screen, save creation and file selection to the start of a new game replays with identical
> per-frame state. Not yet playable beyond the first scene, not yet packaged with a launcher, and not tested on other
> platforms. Nothing has been published yet: the GitLab.com project is prepared locally for review.

> Unofficial fan project, not affiliated with or endorsed by Nintendo. "The Legend of Zelda" and "Spirit Tracks" are
> Nintendo trademarks, used only to describe compatibility. This project contains no game code or data; you need your
> own copy of the game.

## Where things stand

| | |
|---|---|
| Works (Windows x64, verified on one PC) | Building the program from pinned sources (`tools\build\build_program.cmd`); building the game module from your dump, reproducibly (`tools\build\st_build.cmd`); playing from power-on to the start of a new game in a window, with keyboard, mouse as touch screen and saves (`st_native`); scripted runs and recorded sessions replayed with per-frame state hashes ([docs/BUILDING.md](docs/BUILDING.md), [report](docs/research/M1B_FIRST_BOOT.md)) |
| Experimental | A function list from our own extractor and discovery, which decrypts nothing (`st_build.py --functions ours`; boots to the new game); running the game's own anti-piracy checks instead of replacing them: with our list, a scripted session plays from power-on through the first train ride to Castle Town, saves there and reloads (`tests/rom/castle_town.txt`); the default build's replacements lose the train's controls on that ride ([report](docs/research/DSPROTECT_ORIGINAL_PATH.md)). Linux x64/arm64, macOS arm64: the engine builds and passes the recompiler and synthetic-cartridge checks ([M1A_SPIKE.md](docs/research/M1A_SPIKE.md)); our program is not built there yet. Android arm64: the engine compiles and links only |
| Not checked yet | Gameplay past Castle Town; the picture in the window (checked through the frames it receives); sound by ear; gamepads; a second machine |
| Planned | [docs/ROADMAP.md](docs/ROADMAP.md): launcher with ROM selection and first-run build (M4), playable early game (M2), runtime stability and microphone (M3), enhancements (M5), Steam Deck, Android and macOS (M6), mods (M7), full campaign (M8) |
| Unsupported | Multiplayer (Battle/Tag modes) until a later decision; any dump other than USA/Australia revision 0 (`BKIE`) |

## Building and playing

[docs/BUILDING.md](docs/BUILDING.md): the validated path on Windows x64, from a clean checkout to a new game, plus how
to check that your builds and runs are reproducible. Contributors: [CONTRIBUTING.md](CONTRIBUTING.md).

## Documents

| Topic | Document |
|---|---|
| Building, playing, checking reproducibility | [docs/BUILDING.md](docs/BUILDING.md), [CONTRIBUTING.md](CONTRIBUTING.md) |
| Architecture and decisions | [docs/architecture/ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md), [ADRs](docs/architecture/DECISIONS/README.md) |
| Enhancements: app/runtime vs game hooks | [docs/design/ENHANCEMENTS.md](docs/design/ENHANCEMENTS.md) |
| Dual-screen presentation, controls and microphone, interaction matrix, settings and mods | [docs/design/](docs/design/) |
| Recompilation base on five targets (M1a) | [docs/research/M1A_SPIKE.md](docs/research/M1A_SPIKE.md) |
| First boot of the game (M1b local, M2's first milestone) | [docs/research/M1B_FIRST_BOOT.md](docs/research/M1B_FIRST_BOOT.md) |
| The game's anti-piracy checks on the original path | [docs/research/DSPROTECT_ORIGINAL_PATH.md](docs/research/DSPROTECT_ORIGINAL_PATH.md) |
| Existing ports, recompilers, dependencies; platform portability | [docs/research/EXISTING_PROJECTS.md](docs/research/EXISTING_PROJECTS.md), [docs/research/PLATFORM_PORTABILITY.md](docs/research/PLATFORM_PORTABILITY.md) |
| Roadmap, risks, publishing | [docs/ROADMAP.md](docs/ROADMAP.md), [docs/RISKS.md](docs/RISKS.md), [docs/PUBLISHING.md](docs/PUBLISHING.md) |
| Rules for AI agents | [AGENTS.md](AGENTS.md) |


## Development and AI assistance

This project is developed with AI coding assistants under human direction and review. Every change is reviewed by a
human maintainer, every claim in the documents is labelled Verified, Likely, Unknown or Proposed, and agents follow
[AGENTS.md](AGENTS.md), including respecting upstream projects that do not want AI-agent use.
