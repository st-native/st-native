# Existing projects: native ports, recompilers and dependencies

Evaluation date: **2026-11-12**. Every repository below was cloned and read at the commit given, unless the row says
"metadata only". Labels: **Verified** (read in code or docs, or measured), **Likely** (supported by evidence, not
demonstrated), **Unknown**, **Proposed** (our intention, not an existing capability).

Nothing here was built or run: no ROM is available locally and no build was attempted (see "What was not verified").

## 1. Summary

| Project | Commit / date read | Licence | Relevance | Verdict |
|---|---|---|---|---|
| [JonathanDC64/FFCCEOT-Recomp](https://github.com/JonathanDC64/FFCCEOT-Recomp) | `934c7434` (2026-10-17), 109 commits | GPL-3.0 | Origin of the `ntr-recomp` / `ntr-runtime` engine; mature desktop app (layouts, PiP, second window, settings UI, in-app build, Linux AppImage, CI) | **Engine source** (imported with history) + selected desktop components (ADR-0001, provisional) |
| [dguillot-gh/phantom-hourglass-recomp](https://github.com/dguillot-gh/phantom-hourglass-recomp) | `27ec7aec` (2026-11-03), 54 commits | GPL-3.0 | Same engine applied to Spirit Tracks' predecessor; Android app, GLES renderers, ARM64 fibers, save states, mic button | **Source of engine and Android changes**, ported with attribution |
| [melonDS-emu/melonDS](https://github.com/melonDS-emu/melonDS) | `906e9ebb` (2026-08-24), pinned by both recomps | GPL-3.0-or-later | ARM7, 2D/3D GPU, SPU, timers, DMA, cart, mic | Dependency (submodule) |
| [zeldaret/st](https://github.com/zeldaret/st) | `236b5cac` (2026-11-08), README level only | CC0-1.0 | Spirit Tracks decompilation (ds-decomp configs) | **Use blocked pending maintainer contact** (§4) |
| [StraDaMa/…-D-Pad-Patch](https://github.com/StraDaMa/Legend-of-Zelda-Spirit-Tracks-D-Pad-Patch) | `7ea6778c` (2025-03-01) | **none** (all rights reserved) | ST button controls: hook sites in overlays 0/17/24/93 | Facts (addresses) only, as hypotheses |
| [ZeldaWWHDRecomp/ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp) | `19e1fcc9` (2026-11-06) | MPL-2.0 | Second-screen modes, auto-PiP from game state, Steam Deck defaults, mod manager/SDK | Design reference |
| [HarbourMasters/Shipwright](https://github.com/HarbourMasters/Shipwright) | `d4bf6528` (2026-11-01) | **none at repo root** (GitHub reports no licence) | Enhancement registry, hook table, menus, presets | Pattern reference only; **no code reuse** |
| [2ship2harkinian/2ship2harkinian](https://github.com/2ship2harkinian/2ship2harkinian) | `fd9fb03b` (2026-11-02) | CC0-1.0 | Same patterns as SoH | Pattern reference |
| [Kenix3/libultraship](https://github.com/Kenix3/libultraship) | `759b1340` (2026-09-02) | MIT | CVars, ImGui menus, ControlDeck, resource archives, TCC scripting | Pattern reference; N64-specific, not a dependency |
| [AetiasHax/ds-decomp](https://github.com/AetiasHax/ds-decomp) | metadata only (pushed 2026-10-24) | MIT | `dsd`: DS project init, function analysis, NitroSDK signatures | **Tool for our own function analysis** (ADR-0004) |
| [RetroPortingToolKit/ndsrecomp](https://github.com/mstan/ndsrecomp) | metadata + README | NOASSERTION | Alternative DS recompiler (full LLE, ARM7 recompiled) | Not suitable now (§3.2) |
| [mstan/arm-recomp-core](https://github.com/mstan/arm-recomp-core) | metadata + README | MIT | Shared ARM decode/IR core used by ndsrecomp | Watch |
| [mstan/MetroidPrimeHuntersRecomp](https://github.com/mstan/MetroidPrimeHuntersRecomp) | metadata + README | MIT | DS title on ndsrecomp, alpha | Watch |
| [power-pudu/egg-of-time](https://github.com/power-pudu/egg-of-time) | metadata only | NOASSERTION | Chrono Trigger DS recomp using ds-decomp | Watch |

**No other Spirit Tracks native port exists** as of the evaluation date (GitHub search for "spirit tracks", web search).
A listing on openportsgames.com labels the decompilation as a "native PC port"; it is not one. One GitHub search hit
appears to host a ROM file and was disregarded. Related ST community work: `Yanis002/strando` (randomizer prototype),
`Yanis002/stgz` (practice tools), Archipelago clients for PH/ST (`carrotinator/DSZeldaClient`) — relevant to a future
randomizer, not to the foundation.

## 2. The ntr engine lineage (FFCCEOT-Recomp → Phantom Hourglass Recomp)

### 2.1 What the engine is (Verified, both repos)

- **`ntr-recomp`** (`recompiler/`): decodes ARMv5TE ARM and Thumb, discovers functions from a seed list
  (`functions.txt`) plus BL/BLX targets and jump tables, and emits one C function per guest function
  (`void fn_<module>_<addr>(NtrCpu*)`), sharded into 64 files. Modules: `main`, `itcm`, `dtcm`, overlays
  (`100 + n`), and "code regions" (`regions.txt`: code an overlay copies elsewhere at run time).
  `recompiler/include/recomp/recomp.hpp`.
- **Per-game configuration is data**: `functions.txt` (address, size, ARM/Thumb, name, `!dsprot` flag),
  `hooks.txt` (`<module> <addr> replace|before|instead <c_function>`), `regions.txt`. Nothing game-specific is
  compiled into the recompiler (PH only changed its CLI help text and ROM list).
- **Game module ABI** (`runtime/include/ntr/recomp.h`, `NTR_ABI_VERSION 1`): the generated C compiles to a freestanding
  shared library (`*_game.dll/.so`) exporting `ntr_game_load(const NtrHostApi*)`. The host passes memory slow paths,
  call/jump/SWI/poll services and a `find_hook(name)` lookup. This is what makes "build on the player's machine"
  possible: the module needs only a compiler and linker.
- **`ntr-runtime`** (`runtime/`): recompiled ARM9 is the master CPU and counts cycles; `ntr_poll` lets melonDS catch up
  (timers, DMA, GX, ARM7 interpreter, scheduler). Page-table fast paths into main RAM/ITCM/DTCM. HLE BIOS IRQ entry;
  NitroSDK `OS_SaveContext`/`OS_LoadContext`/`OS_InitContext`/`OS_IrqHandler` are replaced so **guest threads run on
  host fibers** (Win32 fibers, `ucontext` on Linux, hand-written AArch64 switch on Android). Overlay residency is
  detected by **probe words**, not FS hooks. Code with no recompiled function falls back to **melonDS's ARMv5
  interpreter**.
- **Runtime API** (`runtime/include/ntr/runtime.hpp`): `Host::on_frame(const Frame&)` delivers **both screens
  independently** (two 256×192 ARGB buffers with the software renderer, or a 2-layer `GL_TEXTURE_2D_ARRAY` at N× with
  the GL renderers), `on_audio` (48 kHz stereo), `on_save`. Input: `set_buttons`, `touch(x,y)`, `release_touch`.
  `peek32` reads guest RAM/TCM for game-state readers. Save-state request/load. Video settings switch at frame
  boundaries.
- **melonDS integration**: pinned submodule, built from a curated source list (no JIT, no frontend), **patched at
  configure time** by exact-text replacement (`third_party/CMakeLists.txt`, `ffcc_patch(var file from to)`, fails the
  configure if upstream text changes). Patches add GX command/VBlank hooks (frame interpolation), widescreen viewport
  widening, HUD sprite moves, stable UVs, texture smoothing.
- **Tests (ROM-free)**: `tests/difftest` (recompiled C vs Unicorn ARM946 on random instructions), unit tests for GX
  stream matching and input latching; FFCC adds settings, input-script, release-info and UI smoke tests.
- **Copyright guard**: `tools/check_no_game_data.py` refuses ROM/NitroFS extensions and magic numbers, oversized
  binaries, and files carrying the `@ntr-recomp-generated` marker; FFCC runs it in CI (`guard.yml`).

### 2.2 FFCCEOT-Recomp specifics (Verified unless marked)

- Status: v0.3.1-alpha, "plays from the title into the field" (author's claim, not verified by us).
- **Distribution model**: the release contains no game code. On first launch the player selects their ROM; setup
  verifies the SHA-1, recompiles, and compiles the module with a **bundled, pinned LLVM 22.1.8** (clang + lld,
  SHA-256 checked). `docs/PACKAGING.md`, `game/src/setup.cpp`, `tools/package.py`. Linux AppImage built in Docker
  (Ubuntu 22.04, glibc ≥ 2.35, Steam Deck desktop mode). Release workflow attaches artifacts to a **draft** release.
- **Presentation** (`game/src/video.hpp`): `Layout { Single (+ optional PiP), MainSide, Stacked, SideBySide, Windows }`,
  swap, PiP size/opacity, side alignment; `screen_at(px,py)` hit-tests the topmost screen and returns DS-local
  coordinates; the second window is opened/closed by the frontend; closing it falls back to `Single` without quitting.
  DXGI flip-model presentation on Windows, VSync and tearing control, GPU timing.
- **UI**: RmlUi menus with controller navigation, settings window, controls window (rebinding, controller-specific
  glyphs), presets (Original/Enhanced), problem reports with path redaction, update check (opt-out).
- Game layer (FFCC-specific, not reusable as is): spell ring, Wii cutscene import, local wireless/WFC, FF-themed art and
  fonts (Cinzel, Alegreya Sans, Material Symbols — licences listed in `NOTICE.md`).
- `tools/port_region.py`: ports a function/hook config from one regional release to another by byte comparison.
- Compute renderer is **switched off** in FFCC (OpenGL only), because widescreen/interpolation were built on OpenGL.

### 2.3 Phantom Hourglass Recomp specifics (Verified unless marked)

- Forked FFCC's engine on 2026-10-12 as a **squashed initial commit** (FFCC history not preserved), removed FFCC's
  frontend, wrote a new SDL3 frontend (`frontend/src/main.cpp`, ~1,400 lines, many globals) and an Android app
  (`android/`, Java: launcher, `SDLActivity`, on-screen pad, layout editor, menu).
- Engine deltas vs FFCC (by `diff -rq`): `runtime/src/savestate.cpp` (save states taken only when every other guest
  thread is parked in `OS_SaveContext`, so states are described by guest data, not host stacks), AArch64 fiber switch,
  `set_mic_blow`/`set_mic_sample`, `peek32`; `third_party/gles_*.cmake` (desktop GLSL → GLSL ES 3.20 for melonDS's GL
  and compute renderers, Mali fixes); wireless/LAN code removed. `recompiler/` decode/analysis/emit are byte-identical.
- PH config: 24,621 functions (24,608 seeded from zeldaret/ph's ds-decomp config, 13 discovered), main + 62 overlays,
  927,030 instructions, 1,390 jump tables, 3 undefined instructions reached (`PH-PORT.md`, author's log).
- Layouts: only touch screen / top screen / both stacked, plus stretch (`frontend/src/layout.hpp`). No PiP, no
  side-by-side, no second window.
- Analog movement for the Zelda engine (`frontend/src/ph_stick.cpp`): holds a virtual stylus at Link's screen position
  + stick direction (22–80 px), Link's position derived from `gPlayerControl` fields. Directly relevant to ST (Likely
  similar structures; must be re-derived).
- Tested only on Windows 11, Pixel 8, LG V30 (author). A `linux-release` CMake preset exists; **Linux is untested**.
  No public CI (a private build-PC script uploads APKs).
- **Microphone: no real capture.** `Platform::Mic_Start/Mic_Stop` are empty; `Mic_ReadInput` returns white noise while
  "blow" is held, or loops an **embedded 8 kHz WAV described as "user-provided PERFECT BLOW FOR DS EMULATORS.wav"**
  (`runtime/src/mic_sample_data.hpp`, 2.4 MB, commit `dfa0f40`). That sample's authorship and licence are
  undocumented: **we must not copy it** (see `docs/legal/LICENSING_AND_PROVENANCE.md`).
- Process note: `docs/HANDOVER.md` is written for "the next AI assistant"; both repos read as heavily AI-assisted,
  single-maintainer projects about one month old. Bus factor and review depth are risks (`docs/RISKS.md`).

### 2.4 Reusability assessment

| Component | Reusable for ST? | Game-specific assumptions found |
|---|---|---|
| `recompiler/` | **Yes, unchanged** | None (config-driven). Comment examples mention AZEE/CFIE only. |
| `runtime/` core (memory, scheduler, IRQ, fibers, SWI, interpreter fallback) | **Yes** | NitroSDK hook *mechanics* are generic; the *addresses* are per game. DTCM at `0x027E0000` noted as "placed by the game". |
| `runtime/` GX/HUD patches | Partly | `FfccHudSides` heuristics tuned to PH's HUD bands; widescreen needs per-game projection hook (`G3i_PerspectiveW_` found in PH by call-site pattern). |
| `savestate.cpp` | Likely yes | Requires `OS_SaveContext`/`OS_LoadContext` hooks (NitroSDK, present in ST: Likely). |
| melonDS patches | Yes | Generic, but each re-pin must re-validate exact-text patches. |
| FFCC `video::Presenter` + layouts + second window | **Yes** | None game-specific; touch routing currently treats only the bottom screen as touchable. |
| FFCC setup/packaging/CI/guard | **Yes** | ROM table, names. |
| FFCC UI framework (RmlUi glue, windows, nav) | Yes | Theme/art/fonts are FF-specific and must be replaced. |
| PH Android app | Yes, adapted | Package name, PH menu items, PH-specific controls (LID, analog via `gPlayerControl`). |
| PH mic noise / sample | Noise: idea only. Sample: **no** | Sample licence unknown. |

## 3. Other DS recompilers

### 3.1 ds-decomp (`dsd`)
MIT. Initialises a decomp project from a ROM and analyses functions; FFCC's `functions.txt` was produced with
`dsd init` + `sig apply` (NitroSDK/NitroSystem signatures) and refined by hand (FFCC `docs/ARCHITECTURE.md` §3).
`tools/import_dsd.py` (in both recomps) converts a local dsd analysis into `functions.txt`. This gives us a path to ST
function boundaries **that does not depend on reading the zeldaret/st repository** (ADR-0004).

### 3.2 ndsrecomp / arm-recomp-core
A separate, more ambitious design: full LLE (recompiles ARM7 too, own hardware models, melonDS only as an accuracy
oracle), firmware boot, Wiimmfi. Self-described "very early pre-alpha… no stable API"; licence reported as
NOASSERTION by GitHub. Not a better foundation today: unclear licence, no stable API, and ST's engine sibling (PH) is
already playable on the ntr engine. Worth watching; `arm-recomp-core` (MIT) is a candidate decoder if ours ever needs
replacing (FFCC listed it as an open evaluation).

## 4. The Spirit Tracks decompilation (zeldaret/st)

Facts read from its README before the policy below was noticed (Verified):

| Version | Description | ROM SHA-1 |
|---|---|---|
| `jp` | Japan Rev 0 | `eaee3602b8a2235211b2e20cdcd4cb357956a264` |
| `eur` | Europe Rev 0 (decomp default) | `9e99cc803a14ce038eb908db585431f8254f09ee` |
| `eur1` | Europe Rev 1 | `dcb221a89acb894503a461c2c7d4cfdad9a06b68`, `b9096f46e9163750a9e3f6c2fc0758e6094b6135`, `4efbff9a263fe622f3f7e7cfb7bea8046b86e57d` |
| `usa` | USA Rev 0 | `1f0921b5e99db9d92d24710f1b004e4f3c997b73` |

The repository has per-version ds-decomp configs (`config/<ver>/arm9/{config.yaml, symbols.txt, delinks.txt,
relocs.txt, overlays/, itcm/, dtcm/}`), is CC0-1.0 at the root, builds with GCC 9+/Ninja/Python, and had 242 commits.

**Maintainer policy (Verified):** the repository contains `AGENTS.md` and `CLAUDE.md` from the
[no-agents.md initiative](https://codeberg.org/rossabaker/no-agents.md) stating that the maintainers reject the use of
AI agents with the repository (reading, analysis, answering questions, forge features). The CC0 licence does not
legally restrict use, but this project is AI-assisted and wants the Zelda community's trust. **Analysis of the
repository's contents was stopped at that point.** Decisions:

- Do not point AI agents at zeldaret/st (code, configs, issues). Recorded in our `AGENTS.md`/`CLAUDE.md` plan.
- A human maintainer should contact the zeldaret/st maintainers before any use of their data, even by humans.
- Derive function boundaries independently with `dsd` on the user's own ROM (ADR-0004).
- Check zeldaret/ph for the same policy before relying on PH Recomp's `functions.txt` lineage for anything.

## 5. Modern Zelda port references (patterns, not code)

### 5.1 Ship of Harkinian / 2Ship2Harkinian / libultraship
- **Hook table**: `soh/Enhancements/game-interactor/GameInteractor_HookTable.h` declares typed events
  (`DEFINE_HOOK(OnSceneInit, (int16_t))`, `OnPlayerUpdate`, `OnOcarinaNote`…), and enhancements subscribe.
  `ShipInit.hpp` registers init functions re-run when named CVars change. → Our game layer should publish typed,
  versioned events from verified hooks (§ARCHITECTURE "Game integration").
- **Settings**: libultraship CVars (string-keyed, persisted), ImGui menus with search; enhancements grouped by
  category with presets (`Enhancements/Presets`). → Adopt the *schema + presets* idea, not stringly-typed globals.
- **Input**: libultraship `ship/controller` (ControlDeck: physical → mapping → N64 buttons), per-port profiles.
- **Mods**: resource archives (`.o2r`, zip) replacing assets by path; LUS can load TCC-compiled scripts.
  Asset replacement by path depends on extracted assets — not our model (we keep assets in the ROM, so DS texture
  replacement must be hash-based at the GPU texture cache).
- **Licence**: Shipwright has **no root licence** → nothing may be copied. 2S2H is CC0, LUS MIT.

### 5.2 Wind Waker HD Recomp (MPL-2.0)
- **GamePad display modes** (`docs/gamepad-display.md`, `runtime/src/gfx/auto_pip.h`): `window`, `pip`, `auto`, `off`,
  `gamepad`. **Auto** mode follows game *state* read on the guest thread at the frame boundary (UI scene singleton,
  menu descriptors, controller routing) and publishes **atomic host-only flags**; the renderer never reads guest
  memory; no guest writes; ~250 ms close delay; a **pin** keeps PiP up; brightness/pixel heuristics were **removed**.
  Unit tests map a synthetic guest heap read-only (a guest write faults). → This is the model for our context-aware
  layout (ADR-0005).
- PiP placement: nine anchors + custom drag, size 5–100 %, opacity 10–100 %, avoids covering HUD elements at some
  anchors; "fill bars with background".
- **Steam Deck Gaming Mode**: PiP + fullscreen by default when gamescope is detected; a second window is still
  available but gamescope shows one window at a time. Closing the GamePad window only hides it.
- **Mods**: in-game manager (built-in / community / catalogue), `manifest.json` (`format_version`, `id`, `version`,
  `game_id`, `minimum_manager_version`, `kind`, typed `options`, dependencies, conflicts), code mods as guest ELF with
  an explicit **trust prompt** per package version, and a manifest **`"permission": "microphone"`** for mods that open
  the mic. → Our manifest and permission design follows this shape (`docs/design/SETTINGS_AND_MODDING.md`).

## 6. Answers to the "first investigation" questions (short form)

| # | Question | Answer | Label |
|---|---|---|---|
| 1 | Reuse PH Recomp's runtime for ST without major redesign? | Yes. The engine is config-driven; ST needs its own `functions/hooks/regions.txt`, NitroSDK hook addresses and ROM entries. | Likely (Verified for structure; boot unproven) |
| 2 | Game-specific recompiler changes? | None expected in code. ST config + possibly `regions.txt` entries for runtime-copied code and `!dsprot` entries if DS Protect is present. | Likely |
| 3 | Can the ST decomp supply boundaries? | Technically it has ds-decomp configs for 4 versions; **we won't use it via AI** (policy). `dsd` on the user's ROM is the independent path. | Verified (policy) / Likely (dsd path) |
| 4 | Biggest boot obstacles? | Locating NitroSDK context/IRQ functions; runtime-copied code; DS Protect (unknown); overlay count (≥ 94); save type. | Likely |
| 5 | Both screens independently? | Yes: `ntr::Frame` exposes top/bottom separately; FFCC draws them into separate rects and windows. | Verified (code) |
| 6 | Input mapping separate from presentation? | Yes, via a hit-test interface (`screen_at`) between layout and input. | Verified (FFCC design) |
| 7 | Real mic + reliable virtual blow? | Real: melonDS `Mic_Start/Mic_Stop/Mic_ReadInput` (47.6 kHz pull) + SDL3 recording stream. Virtual: own synthesized signal, validated per mechanic; game-level hook as fallback. | Verified (API) / Unknown (ST thresholds) |
| 8 | Independent windows and PiP without invasive runtime changes? | Yes, FFCC does it entirely in the frontend. | Verified (code) |
| 9 | Licence implications? | Whole program GPL-3.0(-or-later); no code from Shipwright, D-pad patch or PH's blow sample. | Verified |
| 10 | Shortest path to playable? | Portability spike → new repo with the engine imported → minimal app shell (reusing FFCC's presenter/setup) → ST config via dsd → boot with software renderer → mouse touch → early gameplay. | Proposed |

Platform coverage of each candidate and the portability plan: [PLATFORM_PORTABILITY.md](PLATFORM_PORTABILITY.md).

## 7. What was not verified

- No project was built or run here (no ROM; no build attempted, to avoid executing third-party build scripts before
  review). Build reproducibility of FFCC/PH is therefore **Unknown** on this machine; it is part of the M1a portability spike.
- Maturity claims ("plays to the Fire Temple", "plays into the field") are the authors' own.
- Licences of RmlUi/FreeType/fonts were taken from FFCC's `NOTICE.md`, not re-audited file by file.
