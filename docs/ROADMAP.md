# Roadmap

No dates and no percentages: each milestone ends at a gate that is checked, not estimated. Labels follow the research
documents (Verified / Likely / Unknown / Proposed). Tests marked **(ROM)** run only on a maintainer's machine with a
legally obtained dump; everything else runs in public CI.

**Direction (ADR-0008, 2026-11-14): a port by static recompilation.** The game runs as C generated from the
player's own ROM and compiled on the player's machine, on the engine's runtime; game-specific changes are small hooks
in our own code. Full decompilation and the reconstruction of new functions in C++ are retired from the plan; the
milestones that depended on them (R5 subsystem waves, R6 native SDK layer, R7 scaffold removal) are dropped. The goal
is unchanged: a playable version on Windows, Linux, Steam Deck, Android and macOS, starting with Windows x64, with
remappable controls, flexible layouts for the two screens, selected cutscenes adapted to one screen and other
modernisations, higher internal resolution, widescreen, smoother motion without speeding up the simulation, and mods
([ENHANCEMENTS.md](design/ENHANCEMENTS.md) says, for each, what the app/runtime can do and what needs a game hook).


## Product track

## M0 — Research and feasibility (this document set)

| | |
|---|---|
| Scope | Audit upstreams, ST structure, references, licences; propose architecture and decisions |
| Deliverables | `docs/research/*` (incl. PLATFORM_PORTABILITY and SOURCE_RECONSTRUCTION_POC), `docs/architecture/*` (+ ADR-0001…0007), `docs/design/*`, `docs/legal/*`, this roadmap, `docs/RISKS.md`, `docs/REPOSITORY_PLAN.md` |
| Acceptance | Maintainer approves ADR-0002…0007 (or amends them), chooses the first ROM version, decides how to approach zeldaret/st, shortlists a name, commissions the legal review of a source port |
| Status | Done. ADR-0001 superseded by ADR-0008 and ADR-0002 accepted (2026-11-14); first ROM version: USA/Australia rev. 0; zeldaret/st: not used (ADR-0004); legal research done 2026-11-13 (counsel still to be consulted) |

## M1a — Portability spike

| | |
|---|---|
| Scope | Prove the engine and the planned app boundaries on all five targets, without restructuring anything: V1–V7 in [PLATFORM_PORTABILITY.md §7](research/PLATFORM_PORTABILITY.md#7-minimal-validation-strategy-before-adr-0001-is-accepted-no-restructuring-needed) |
| Depends on | M0 approval; CI runners (Windows, Ubuntu x64/arm64, macOS arm64, Android NDK) |
| Risks | Engine assumes NitroSDK for the test cartridge; macOS fiber port; GLES emulator coverage |
| Deliverables | Scratch branch with CI matrix, fiber tests, test cartridge (our own code), presenter smoke, mic pipeline test, package skeletons; a short report |
| Acceptance | V1–V3 pass on all five targets; V4 on Windows, Linux, macOS, and compiles/runs on Android |
| Done when | The report's pending checks are closed or accepted as known limitations |
| Status | **Started 2026-11-13** ([report](research/M1A_SPIKE.md)), locally and in a private CI repository. V1–V3 pass on Windows x64, Linux x64 and Linux arm64. On macOS arm64, V1 and V3 pass and V2 fails one check (the rounding mode is not kept per fiber). On Android arm64 the base compiles and links, and V2 has run only on arm64 Linux. The synthetic cartridge gives identical per-frame hashes on Windows, Linux x64/arm64 and macOS. This takes five small engine patches. The V2 rounding-mode check (T5) is a known failure, left open with the check active (maintainer's decision, 2026-11-13). Pending: V2 on an Android device or emulator; V4–V7 |

## M1b — Reproducible technical baseline

| | |
|---|---|
| Scope | The public project on GitLab.com (dedicated account; pending): app, runtime, recompiler (the engine with our patches), tools, synthetic tests, docs; CI for five targets (Windows/Linux full, macOS build + tests, Android compile); the game-data guard on every change |
| Depends on | M1a; maintainer's ROM (for the local half) |
| Risks | Inherited code quality; melonDS patch drift; `dsd` quality on ST |
| Deliverables | Publication bundle assembled locally and reviewed ([PUBLISHING.md](PUBLISHING.md)); GitLab CI; `game/st/config/bkie/` hooks and regions; NitroSDK hooks located; recompiler report; a function-list path that decrypts nothing |
| Verification | CI: ROM-free build, unit tests, Unicorn difftest, engine include-boundary check, guard over full history. (ROM): SHA-1 check, recompiler stats (functions seeded/discovered, undefined instructions reached, jump tables, warnings), module compiles |
| Acceptance | A fresh clone builds the program on every CI target with pinned dependencies; with the ROM, the ST module is generated and compiled reproducibly (same hash twice) on Windows and Linux |
| Done when | Build steps documented in `docs/BUILDING.md` and repeated by a second person or clean VM |
| Status | **Local half done on Windows x64, 2026-11-13** ([report](research/M1B_FIRST_BOOT.md), [BUILDING.md](BUILDING.md)). Done: the ST config (`functions.txt` from dsd + import with patch 0006; hooks for the NitroSDK context switches, IRQ dispatch and the six DS Protect entry points); the recompiler report; the module built twice from clean folders, byte-identical in all 489 artifacts (after adding `/Brepro`: timestamps were the only difference); the steps repeated in a separate copy with a fresh engine clone; `docs/BUILDING.md`. Not done: the GitLab project and its CI (bundle prepared locally for review, 2026-11-14); Linux module build; a second person or clean VM |

## M2 — First playable execution

| | |
|---|---|
| Scope | Boot, both screens, audio, original input, cartridge save, early gameplay |
| Depends on | M1b |
| Risks | Thread/IRQ hooks wrong; runtime-copied code; DS Protect; save type; audio timing |
| Deliverables | Boot to title and file select; new game; reach and play the first areas through the first train journey; save and reload; the game's own DS Protect checks preserved if the runtime can run them ([report](research/DSPROTECT_ORIGINAL_PATH.md)) |
| Verification | (ROM) scripted `--input-script` runs with screenshots compared by a human against melonDS; `--perf` interpreter share; save written atomically and reloaded; 30-minute soak without fatal error |
| Acceptance | A documented scripted path from boot to the first dungeon entrance plays with Software renderer and mouse/keyboard, and the cartridge save reloads in melonDS (cross-check) |
| Done when | Results and known issues recorded in `docs/testing/COMPATIBILITY.md` |
| Status | **First milestone reached on Windows x64, 2026-11-13** ([report](research/M1B_FIRST_BOOT.md)): boot, title screen, save creation, file select, name entry and a new game started (prologue, then the first scene), with 0 instructions on the interpreter. A scripted run (`tests/rom/boot_new_game.txt`) replays with identical per-frame state hashes. Next: play through the first areas, save/reload, the melonDS cross-check; the game's own DS Protect checks and a function list that decrypts nothing (both run the first minutes of play identically by screenshot, 31,000 frames, 2026-11-14; the checks also run during play). **2026-11-14, session 4:** `--functions ours --dsprot original` plays from power-on out of Aboda, through the first train journey, to Castle Town, saves in game and reloads (`tests/rom/castle_town.txt`, 52,400 frames, reproducible frame for frame); the default build's replacements lose the train controls on that journey, so play past Aboda needs the original checks ([report](research/DSPROTECT_ORIGINAL_PATH.md) §5). Still open for M2: the first dungeon entrance, the melonDS cross-check of the save, a 30-minute windowed soak |

## M3 — Runtime and compatibility stabilisation

| | |
|---|---|
| Scope | Overlay transitions, timing, audio, microphone (real + virtual), GL renderers, save states |
| Depends on | M2 |
| Risks | Mic thresholds; save-state correctness; interpreter hot paths |
| Deliverables | `MicSource` + real capture + virtual blow (ADR-0006); `--mic-log`; OpenGL renderer upscaling; save states + resume; regression scenario set |
| Verification | (ROM) mic tests §6.5 of CONTROLS_AND_MICROPHONE; growing scenario suite replayed with frame hashes (FFCC `--frame-hash`); fps probe report |
| Acceptance | All mic mechanics found by `--mic-log` pass in both modes; regression suite passes on Windows and Linux |
| Done when | Compatibility matrix covers everything reached so far |

## M4 — Native desktop experience

**In progress.** The Windows developer launcher has ROM selection, local preparation and reuse, and display and
language settings. The game opens in the same window. Esc and F1 open the pause menu, and the physical keyboard
can enter the name on the game's new-save screen. See [BUILDING.md](BUILDING.md) for setup.

**Proposed.** Once the local game module is ready, bring the live title screen into the same window as the launcher.
Use frames rendered by the runtime from the player's ROM and draw the launcher controls above them.

| | |
|---|---|
| Scope | Layout engine + all modes, second window, persistence, input profiles, first settings UI |
| Depends on | M3 (stable runtime) — layouts can start in parallel because they are ROM-free |
| Risks | Hit-test bugs; stuck touches across windows; RmlUi learning curve |
| Deliverables | Launcher: ROM selection (SHA-1 checked), first-run module build with progress and cache (ADR-0002); DUAL_SCREEN_UX modes (Auto excluded), touch router, remappable controls and profiles (Classic, KB+M, Modern controller first schemes: analog movement, attacks), settings schema + UI for Graphics/Screens/Controls/Audio/Saves |
| Verification | ROM-free unit tests (layout tables, hit round trips, capture rules); UI smoke test; (ROM) tap-accuracy scripts in each mode; controller-only playthrough of M2's path |
| Acceptance | M2's path completed with mouse only, controller only, and in every layout mode |

## M5 — Graphics and quality of life

| | |
|---|---|
| Scope | Resolution scaling defaults, filters, frame pacing, interpolation (smoother motion at the game's own speed), experimental widescreen, selected cutscenes adapted to one screen, other modernisations, Auto layout, more control schemes, accessibility ([ENHANCEMENTS.md](design/ENHANCEMENTS.md)) |
| Depends on | M4; verified ST context fields |
| Risks | Widescreen culling/HUD; interpolation artefacts; context flags wrong |
| Deliverables | Auto layout on verified flags; train and Phantom schemes; widescreen (experimental); accessibility options |
| Verification | Context reader tests on synthetic memory; (ROM) visual comparison scenes; train/Phantom scheme tests from the interaction matrix |
| Acceptance | Each enhancement is optional, labelled, independently tested, and off in the Original preset |

## M6 — Steam Deck, Android and macOS

| | |
|---|---|
| Scope | SteamOS validation, controller-first UI, Steam Input defaults, suspend/resume; Android app (adapted from PH), touch controls, portrait/landscape, mic permission, lifecycle; macOS `.app`, GL 4.1 + Software, mic permission (TCC), signing decision |
| Depends on | M4 (M5 partially); CI from M1b already builds macOS and Android |
| Risks | GLES driver bugs (PH saw Mali issues); Android on-device compiler size and compile time (ADR-0002); thermal limits; macOS OpenGL deprecation |
| Deliverables | Deck presets and docs; Android build path (on-device compiler, developer PC path); virtual controls editor; macOS package; per-platform known-limitations list |
| Verification | (ROM, device) Deck Gaming Mode and Desktop Mode runs; two Android devices (low and high end) with fps measurements as PH did; an Apple Silicon Mac run with real mic |
| Acceptance | M2's path completed on a Deck without keyboard/mouse, on an Android phone with touch only, and on a Mac with keyboard/mouse and with a gamepad |

Platform releases may ship at different times; each platform enters "supported" only after its own M6 acceptance.

## M7 — Modding foundation

| | |
|---|---|
| Scope | Manifests, mod manager UI, texture replacement, enhancement events, one example mod with original assets |
| Depends on | M5 |
| Deliverables | SETTINGS_AND_MODDING stages 1–2; developer docs |
| Acceptance | One documented example mod installs, configures, disables cleanly, and is protected from corrupting saves |

## M8 — Full campaign and release candidate

| | |
|---|---|
| Scope | Full campaign testing, fixes, profiling, docs, release process validation |
| Deliverables | Compatibility matrix complete for the campaign; release candidate packages; publication checklist approved |
| Acceptance | Full campaign completed on Windows, Linux/Deck and Android with recorded evidence; no open data-loss bugs; legal checklist signed off |

Gates that block publication regardless of milestone: the pre-publication checklist in
`docs/legal/PUBLICATION_AND_BRANDING.md` §6.
