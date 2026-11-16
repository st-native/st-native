# Architecture

Status: **Proposed** (revised 2026-11-14 for [ADR-0008](DECISIONS/0008-static-recompilation-port.md)). The product is
a cross-platform **port by static recompilation**: the player's own ROM is translated to C by the recompiler and
compiled on the player's machine into a game module that runs on the runtime (the `ntr` engine lineage on melonDS),
under our application. Targets: Windows x64 (first), Linux x64, Steam Deck, Android ARM64 and macOS Apple Silicon.
Evidence: [M1A_SPIKE.md](../research/M1A_SPIKE.md), [M1B_FIRST_BOOT.md](../research/M1B_FIRST_BOOT.md),
[PLATFORM_PORTABILITY.md](../research/PLATFORM_PORTABILITY.md). (The earlier source-reconstruction design, ADR-0001
rev. 3, is retired.)

## 0. Layers

```
 ┌───────────────────────────────────────────────────────────────────────────────────────────┐
 │ app/  launcher (ROM selection, first-run build), present, input, audio, settings, ui,     │  published
 │       saves, mods, platform glue                                                          │
 ├───────────────────────────────────────────────────────────────────────────────────────────┤
 │ game/st/  hooks (our C++ at verified addresses) and config (hooks.txt, regions.txt,       │  published
 │           interpret ranges): the only Spirit Tracks-specific code                         │
 ├───────────────────────────────────────────────────────────────────────────────────────────┤
 │ game module  C generated from the player's ROM, compiled on the player's machine          │  never leaves
 │              (st_build / setup; cached per ROM hash, config and ABI)                      │  that machine
 ├───────────────────────────────────────────────────────────────────────────────────────────┤
 │ runtime  ntr-runtime + melonDS (ARM7, GPU, SPU, timers, DMA, cartridge), fallback         │  published
 │          interpreter, HLE BIOS, NitroSDK thread/IRQ replacements                          │  (engine + our patches)
 ├───────────────────────────────────────────────────────────────────────────────────────────┤
 │ recompiler  ntr-recomp (function list from our discovery or dsd, hooks, interpret ranges) │  published
 └───────────────────────────────────────────────────────────────────────────────────────────┘
```

- **Game-specific behaviour is hooks.** A `hooks.txt` entry replaces a guest function with ours (`replace`), runs ours
  first (`before`), or lets ours decide (`instead`). Hooks are small, at addresses found in our own analysis and
  recorded with how they were found; they are our code, not translations of the game's (ENHANCEMENTS.md).
- **Code that cannot be translated ahead of time** (decrypted or copied at run time) runs on the runtime's fallback
  interpreter: `regions.txt` for copied code, `interpret.txt` (patch 0008) for code whose ROM bytes are not what runs.
- **Verification** is by replay: scripted or recorded sessions with per-frame state and RAM hashes (patch 0007);
  reproducible module builds compared artifact by artifact.

## 1. Shape of the system

```
 player's ROM ─► app/setup: verify SHA-1 ─► engine/recompiler ─► C ─► platform toolchain ─► st game module
                                                                     (bundled LLVM / Xcode CLT / Android-hosted clang)
┌──────────────────────────────── program (ships with no game code) ────────────────────────────────────────┐
│ app/core      lifecycle, main loop, threads, commands                                                      │
│ app/setup     ROM import + verification, recompile, compile, module cache                                  │
│ app/present   layout engine (pure) · window manager · hit testing · PresentBackend (GL/GLES; SDL_GPU later) │
│ app/input     devices → actions → DS events · profiles · touch router                                     │
│ app/audio     output · MicSource (real capture, virtual blow, arbiter)                                     │
│ app/settings  schema registry · storage/migration · presets      app/ui   RmlUi menus (render iface/backend) │
│ app/saves     cartridge saves, backups, import/export · save states      app/mods  manifests, loader (M7)   │
│ app/diag      logs, problem reports, counters                                                              │
├────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ game/st       Spirit Tracks integration: config/<code>/ · hooks · context reader · control schemes ·       │
│               enhancements · events                                                                         │
├────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ engine/       game-agnostic, split-ready: recompiler/ (ntr-recomp) · runtime/ (ntr-runtime) · common/ ·     │
│               third_party/melonDS (pinned, configure-time patches) · tests/                                │
├────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ platform/     thin OS services SDL3 does not cover: paths, lifecycle hooks, permissions, toolchain          │
│               discovery, crash handling, gamescope/Deck detection, file import (SAF on Android)             │
│ packaging/    windows (zip) · linux (AppImage, Docker) · macos (.app) · android (Gradle app from PH)        │
└────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

## 2. Boundaries and why each exists

Each boundary is justified by a portability or maintenance need; where no such need exists, modules are plain
directories, not abstraction layers.

| # | Boundary | Location | Justification | Interface |
|---|---|---|---|---|
| 1 | Recompilation and code generation | `engine/recompiler` (library + CLI), driven by `app/setup` | Translates the player's ROM into the game module on all five targets | `Program/Analysis/emit_c` API; `NTR_ABI_VERSION` game-module ABI |
| 2 | Game-specific integration | `game/st` | Keeps the engine reusable; holds per-release address tables | Runtime hook lookup by name, `peek32`, publishes `GameContext` and events |
| 3 | Reusable DS runtime | `engine/runtime` | Portable core; OS-specific pieces limited to fibers and module loading (internal shims) | `ntr::Runtime`, `ntr::Host`, `ntr::Frame` |
| 4 | Rendering backends | DS rasterisers in melonDS (engine); `GpuContext` supplied by the app; presentation backends in `app/present/backends/` | GL on desktop, GL 4.1 on macOS, GLES on Android, SDL_GPU later (ADR-0007) | `GpuContext` (make current, proc address, API/version); `PresentBackend` |
| 5 | Screen composition and layout | `app/present/layout` (pure), compositor in backend | Same layouts on every backend and platform; testable without a GPU | `layout(...) → views`; `LayoutQuery::hit` |
| 6 | Input and microphone | `app/input`, `app/audio/mic` | Device sets differ per platform (touch, trackpads, gamepads, mics, permissions) | actions/profiles; `MicSource` behind melonDS `Mic_*` |
| 7 | Settings and integrated UI | `app/settings`, `app/ui` | One schema, several frontends (desktop, Deck, touch); UI renderer follows the present backend | schema registry; RmlUi `RenderInterface` per backend |
| 8 | Save management | `app/saves` | Platform paths and lifecycles (Android process death, Deck suspend) | save store API; separate save-state store |
| 9 | Modding and extension APIs | `app/mods` + `game/st/events` | Versioned contract independent of internals (M7) | manifest; typed events; C ABI (later) |
| 10 | Platform services | `platform/` | Gaps SDL3 leaves: permissions, toolchain discovery, crash handlers, gamescope, SAF | small free functions per OS, chosen at build time |
| 11 | Packaging and distribution | `packaging/`, `tools/package.py`, CI | Different artifacts and signing per OS; must never contain game data | scripts + CI jobs + guard |

Dependency direction: `app/* → engine (public API only)`, `app/* → game/st` through interfaces (`ControlScheme`,
`GameContext`), `game/st → engine`, `engine → melonDS, common`, everything → `platform` (never the reverse).
`engine/` must not include `app/`, `game/` or `platform/` headers (CI check), so it can be split into its own
repository later (ADR-0001, option C).

## 3. Threads and data flow

| Thread | Owns | Talks to others through |
|---|---|---|
| Main (SDL events; Android UI thread via SDL) | windows, event pump, UI input | lock-free input state; command queue to the game thread |
| Game | `Runtime::run()`, recompiled code, melonDS, graphics context, presentation | `Host::on_frame` per DS frame |
| Audio out | SDL audio stream | ring buffer filled by `on_audio` |
| Mic in | SDL recording stream | ring buffer read by melonDS `Mic_ReadInput` |

Per frame (`Host::on_frame`, the only point where guest state is consistent): `game/st/context` publishes a
`GameContext` snapshot (read-only guest reads) → `layout()` computes views for every window → the present backend
draws and presents → `app/input` latches input into the runtime (`set_buttons`, `touch`).

## 4. Module notes

- **engine/runtime**: merge PH's deltas into the FFCC-lineage code (save states, AArch64 fibers, GLES conversion,
  `peek32`); add macOS/Linux arm64 fibers; replace PH's mic globals with `MicSource`; move game-specific video options
  (e.g. PH's `hud_sides`) out of the public API into game-layer configuration.
- **game/st**: release tables selected by game code; context reader reads documented, verified fields only (WWHD
  pattern), `known` bit per signal; control schemes declare the contexts they were verified in.
- **app/present**: FFCC's `video::Presenter` split into the pure layout engine and a GL backend; second window via SDL3
  windows sharing the GL context; details in [DUAL_SCREEN_UX.md](../design/DUAL_SCREEN_UX.md).
- **app/audio**: real capture opened only between `Mic_Start` and `Mic_Stop` and only when chosen; virtual blowing
  synthesized (ADR-0006); per-platform permission handled by `platform/`.
- **app/settings + app/ui**: schema registry; RmlUi (already proven in FFCC; renderers exist for GL3/GLES, Vulkan,
  SDL_GPU) as the only player-facing UI toolkit.
- **app/saves**: atomic writes, rotated backups, never silent overwrite; save states versioned and separate.
- **app/setup**: per-platform toolchain: bundled LLVM (Windows, Linux), Xcode Command Line Tools or bundled LLVM
  (macOS, decided by the spike), Android-hosted clang/lld in the APK (WWHD model) with a developer PC path.

## 5. Platforms

| Platform | Status | Path |
|---|---|---|
| Windows x64 | Proposed, first | clang-cl, zip, GL / Software |
| Linux x64 | Proposed, first | clang, AppImage (Ubuntu 22.04 base), GL / Software |
| Steam Deck | Proposed, with Linux | AppImage, Gaming-Mode presets, controller-first UI |
| Android ARM64 | Proposed, M6 | NDK + PH's Android app adapted; GLES / Software; on-device compiler target |
| macOS Apple Silicon | Proposed, M6 | AppleClang, `.app`; GL 4.1 / Software; SDL_GPU fallback path |

Additional architectures (Linux arm64, Windows arm64) are not targets but are kept possible: no x86-only code exists
in the engine (only `__builtin_memcpy` is compiler-specific; fibers are per-architecture).

## 6. Product requirements coverage (Ship of Harkinian–level goals)

| Requirement | Where | Platform constraints today |
|---|---|---|
| Integrated settings menus | `app/settings` + `app/ui` | none (RmlUi on every backend) |
| Graphics enhancements | ADR-0007, `app/present`, melonDS renderers | Compute renderer not on macOS; some enhancements GL-family-only |
| Advanced screen layouts | `app/present` | Android second display: Unknown |
| Control remapping | `app/input` | none |
| Optional gameplay enhancements | `game/st/enhancements` | none (game layer is platform-neutral) |
| Accessibility | settings + input + present | none |
| Save management | `app/saves` | per-OS paths |
| Asset replacement | melonDS texture cache hook | GL-family renderers only |
| Mods and extensions | `app/mods`, `game/st/events` | code mods: native per-platform builds or a portable format (open, M7) |
| Stable release engineering | `packaging/`, CI, guard | macOS signing decision |

Any requirement an upstream limitation blocks on a platform is listed here and in
[RISKS.md](../RISKS.md), never dropped silently.

## 7. Error handling and conventions

C++20, CMake ≥ 3.28, Ninja; clang family on every platform (clang-cl, clang, AppleClang, NDK clang). RAII for every
handle; expected failures return result types with messages; exceptions only at module boundaries; runtime `fatal()`
for unrecoverable guest states with a crash report. No new global mutable state; inherited globals are moved into
owned objects as code is touched. Engine code keeps upstream style so fixes can be exchanged.

## 8. Deliberately not done

No generic multi-console framework; no rewrite of melonDS services before a measured need; no simulation-rate changes;
no stable modding API before verified hooks exist; no second repository for the engine until ADR-0001's promotion
criteria are met.
