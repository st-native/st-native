# Platform portability assessment

Evaluation date: **2026-11-12**. Targets (architectural, not simultaneous releases): **Windows x64, Linux x64,
Steam Deck / SteamOS, Android ARM64, macOS Apple Silicon**. Labels: **Verified** (read in code/docs at the commits in
[EXISTING_PROJECTS.md](EXISTING_PROJECTS.md)), **Likely**, **Unknown**, **Proposed**. Nothing was built or run on any
platform; the validation plan in §7 is what turns these findings into evidence.

## 1. Supported-platform matrix of candidate foundations (as they exist today)

| | Windows x64 | Linux x64 | Steam Deck | Android ARM64 | macOS Apple Silicon |
|---|---|---|---|---|---|
| **FFCCEOT-Recomp** (engine + app) | Released (author) | Released AppImage (author) | Desktop mode via AppImage (author) | **None** | **None**: not built; one `__APPLE__` GL context flag in `video.cpp` |
| **Phantom Hourglass Recomp** (engine + app) | Tested Win 11 (author) | CMake preset only, untested | Untested | Tested Pixel 8, LG V30 (author); game code compiled on a PC into the APK | **None**: `.dylib` path in `game_module.cpp` only |
| **ntr engine** (common to both) | Win32 fibers, `LoadLibrary` | `ucontext` fibers, `dlopen` | as Linux | own AArch64 fiber asm (PH) | Fibers: needs the AArch64 asm ported to Mach-O; rest Likely portable |
| **melonDS core** (pinned `906e9ebb`) | Yes (upstream) | Yes | Yes | Used by third-party Android apps; GLES via PH's conversion | Yes (upstream builds; `APPLE` branches in CMake) |
| melonDS **Software** renderer | Yes | Yes | Yes | Yes (slow on old phones: PH measured 31.7 fps on an LG V30) | Yes |
| melonDS **OpenGL** renderer (GLSL 1.40, GL 3.2 core) | Yes | Yes | Yes | GLES 3.2 conversion (PH) | GL 4.1 core: Likely yes (deprecated API) |
| melonDS **Compute** renderer (GLSL 4.30 compute) | Yes (GL 4.3) | Yes | Yes | GLES 3.1+ conversion; scrambled tiles on Mali, unresolved (PH handover) | **No** (macOS GL stops at 4.1) |
| Reference: **Wind Waker HD Recomp** (Wii U, MPL-2.0) | Yes | Yes + AppImage | Yes (Gaming Mode presets) | Yes, **compiles the game on the phone** | Yes (Metal; Vulkan via MoltenVK) |
| Reference: ndsrecomp / MPH Recomp | Windows releases | Linux/Deck AppImage | Yes (MPH) | Unknown | Unknown |

Conclusion: no existing DS foundation covers all five targets. The **engine and melonDS are portable**; what is missing
(Android in FFCC, desktop polish in PH, macOS everywhere) lives in the **application layer**, plus one small engine
piece (macOS fibers). Wind Waker HD Recomp proves the "recompile on the player's device" model works on all five.

## 2. Platform-specific dependencies and blockers

| Area | Windows | Linux / Steam Deck | macOS Apple Silicon | Android ARM64 |
|---|---|---|---|---|
| Guest threads (fibers) | Win32 fibers (Verified) | `ucontext` (Verified) | Port PH's AArch64 switch (AAPCS64 callee-saved regs; Apple reserves x18; Mach-O symbol syntax) — small, Likely | AArch64 asm (Verified) |
| Load game module | `LoadLibrary` (Verified) | `dlopen` (Verified) | `dlopen`; arm64 code must be signed: `ld64` ad-hoc signs by default; a notarized hardened-runtime app needs `com.apple.security.cs.disable-library-validation` to load a locally built module (Likely) | `dlopen` from app storage (Likely; WWHD loads its locally linked game) or a module packaged in the APK (PH) |
| Compiler on the player's machine | Bundled LLVM 22.1.8 (FFCC, Verified) | Bundled LLVM in the AppImage (FFCC, Verified) | Xcode Command Line Tools (WWHD, Verified in README) or bundled LLVM (Unknown size/signing) | Android-hosted clang/lld packaged as APK native libraries, API 33+ (WWHD `docs/android-implementation.md`, Verified as documented; Android 10+ forbids executing binaries from writable app storage) |
| DS 3D/2D rasterisation | Software / OpenGL / Compute | same (Deck: Mesa radeonsi GL 4.6, Likely) | Software / OpenGL 4.1 (deprecated); **no Compute** | Software / GLES 3.2 / Compute ES (experimental) |
| Presentation + UI | GL 3.3+ (DXGI flip optional, FFCC); RmlUi GL3 | GL 3.3+ (X11/Wayland through SDL3); RmlUi GL3 | GL 4.1 core (RmlUi GL3 uses `#version 330`, Verified); Metal through SDL_GPU later | GLES 3.x (RmlUi GL3 has an Android `#version 320 es` path, Verified) |
| Audio out / mic in | SDL3 (WASAPI) | SDL3 (PipeWire/PulseAudio/ALSA) | SDL3 (CoreAudio); `NSMicrophoneUsageDescription`; `com.apple.security.device.audio-input` if hardened | SDL3 (AAudio/OpenSL ES); `RECORD_AUDIO` runtime permission |
| Input | SDL3 | SDL3; Steam Input exposes a virtual gamepad | SDL3 | SDL3 + on-screen controls (PH's Java `DsPad`) |
| Lifecycle | — | Deck suspend: process frozen, clocks jump on resume | sleep/App Nap | background/foreground, process death; PH writes a resume state |
| Packaging | Portable zip (FFCC) | AppImage built in Ubuntu 22.04 Docker, glibc ≥ 2.35 (FFCC) | `.app` in zip/dmg; signing/notarization decision | Signed APK, sideloaded (not Google Play: GPL + legal profile) |
| Data paths | `%APPDATA%` (SDL pref path) | XDG data dir | `~/Library/Application Support` | app-private files; ROM import through Storage Access Framework |
| Known blockers | none | none | OpenGL deprecation (still present as a 4.1 core profile in macOS 26, per community reports; no Apple statement found); no Compute renderer; Developer ID cost for notarization | Compiler footprint (~200 MB in WWHD), compile time and heat; GLES driver bugs (PH: Mali); low-end CPUs need a GPU renderer |

## 3. Graphics portability

Two layers must not be confused:

1. **DS rasterisation** (turning GX/2D state into screen images) — melonDS renderers: Software (CPU, exact, 1×),
   OpenGL (GLSL 1.40), Compute (GLSL 4.30 compute). No Vulkan or Metal renderer exists in melonDS (Verified: source list
   at `906e9ebb`); a Vulkan compute port existed in a fork and was removed (PH handover).
2. **Presentation and UI** (layouts, PiP, windows, overlays, menus) — ours.

| Approach | Windows | Linux/Deck | macOS | Android | Notes |
|---|---|---|---|---|---|
| Desktop OpenGL 3.3+/4.3 | Yes | Yes | 4.1 only, deprecated | No | Today's path in FFCC/PH |
| OpenGL ES 3.x | via ANGLE | Mesa | via ANGLE | Yes | PH converts melonDS shaders to ES 3.20 at configure time |
| ANGLE (GLES over D3D11/Metal/Vulkan) | Yes | Yes | Metal backend: ES 3.0 Likely, ES 3.1 status unclear | n/a | Would unify on GLES; melonDS GL renderer wants ES 3.2 features after PH's conversion — Unknown on ANGLE-Metal |
| Vulkan | Yes | Yes | via MoltenVK | Yes (most devices) | No DS renderer for it; SDL_GPU and RmlUi (`RmlUi_Renderer_VK`) support it |
| Metal | — | — | Yes | — | No DS renderer for it; SDL_GPU and MoltenVK target it |
| **SDL_GPU** (SDL3: Vulkan, D3D12, Metal; shaders as SPIR-V/DXIL/MSL, `SDL_shadercross`) | Yes | Yes | Yes | Yes (Vulkan) | Good for presentation and UI (`RmlUi_Renderer_SDL_GPU` exists in RmlUi 6.3, Verified); has **no OpenGL backend**, so it cannot consume melonDS's GL textures without a copy |
| Software rasteriser + any presenter | Yes | Yes | Yes | Yes (fast phones) | Always-available fallback; no upscaling |

**Proposed strategy (ADR-0007):**
- Phase 1: the **GL family** everywhere (desktop GL 3.3 core on Windows/Linux, GL 4.1 core on macOS, GLES 3.x on
  Android), because the only upscaling DS rasterisers are GL. Software renderer as the universal fallback.
- Presentation, layouts, HUD composition decisions, settings and UI talk to a small `PresentBackend` interface
  (draw a screen image into a rect with filter/opacity/rotation; draw UI geometry), never to GL directly.
- Phase 2 (when needed, e.g. if Apple removes OpenGL): an SDL_GPU presentation backend fed by the Software renderer
  (CPU images) gives a no-OpenGL path on every OS immediately; an accelerated Metal/Vulkan DS rasteriser (porting the
  compute renderer's GLSL to SPIR-V/MSL) is a separate, larger research item.
- Enhancements implemented inside melonDS's GL shaders (FFCC/PH widescreen HUD moves, texture smoothing, texture-cache
  replacement) are **GL-family-only** and stay optional and labelled until backend-neutral versions exist.

## 4. Proposed approach per platform

| Platform | Build | Game module | Graphics default / fallback | Packaging | First validation |
|---|---|---|---|---|---|
| Windows x64 | clang-cl + lld, static CRT | bundled LLVM (FFCC model) | OpenGL N× / Software | portable zip | CI build + tests; local ROM boot |
| Linux x64 | clang, libstdc++ ≥ 13, Ubuntu 22.04 container | bundled LLVM | OpenGL N× / Software | AppImage | CI build + tests; local ROM boot |
| Steam Deck | = Linux x64 | = Linux | OpenGL (RADV/radeonsi) | AppImage as non-Steam game; Gaming-Mode presets | Device run in Gaming and Desktop modes |
| Android ARM64 | NDK, Gradle, SDL3 Java glue (from PH) | target: on-device compiler (WWHD model); developer path: PC-built APK for personal use | GLES 3.2 / Software (fast phones) | signed APK, sideload | CI compile; device run |
| macOS arm64 | AppleClang/CMake, `.app` bundle | Xcode Command Line Tools (prompted) or bundled LLVM — decided by the spike | OpenGL 4.1 / Software; SDL_GPU (Metal) + Software as no-GL fallback | zip/dmg; ad-hoc or Developer ID + notarization (maintainer decision) | CI build + tests on `macos-14` arm64 runner; device run |

## 5. What is reused and what must be adapted

| Component | Origin | Treatment |
|---|---|---|
| `ntr-recomp` (decode, analysis, emit) | FFCC = PH | **Reuse unchanged**, imported with history |
| `ntr-runtime` core (memory, scheduler, IRQ/HLE BIOS, interpreter fallback) | FFCC + PH | **Reuse**; adapt: `MicSource` interface, GPU-context interface, move game-specific video knobs (`hud_sides` "PH") to the game layer |
| Fibers | FFCC/PH | Reuse Win32/ucontext/Android asm; **add** macOS arm64 (and Linux arm64) asm |
| Save states (`savestate.cpp`) | PH | Reuse after review |
| melonDS + configure-time patches | FFCC + PH | Reuse; GLES conversion from PH; GL-only enhancement patches kept optional |
| Presenter, layouts, second window (`video.cpp`) | FFCC | **Adapt**: split pure layout from GL drawing (`PresentBackend`), extend modes (rotation, PiP placement, Auto) |
| Setup (ROM check, recompile, compile, cache) | FFCC | Adapt per platform toolchain |
| Packaging (`package.py`, Docker, `release.yml`), guard, CI | FFCC | Adapt; add macOS and Android jobs |
| Input bindings, controller glyphs, input latch | FFCC (+PH latch) | Adapt into `app/input` |
| RmlUi integration and widgets | FFCC | Adapt; new theme and art |
| Settings file | FFCC | Replace with schema registry (keep tests' ideas) |
| Android app (launcher, `SDLActivity`, `DsPad`, layout editor) | PH | Adapt to our app core |
| GLES shader conversion and checks | PH | Reuse |
| Not taken | FFCC wireless/LAN, **Wii disc decryption (AES with a user-supplied console key)**, Wii cutscenes, spell ring, FF art/fonts, site; PH blow sample (licence unknown), PH-specific stick/HUD code | Excluded |

## 6. Effects on product requirements (nothing silently dropped)

| Requirement | Windows/Linux/Deck | Android | macOS | Constraint and plan |
|---|---|---|---|---|
| Resolution scaling | GL/Compute | GLES | GL 4.1 | — |
| Compute renderer accuracy | Yes | experimental | **Not available** | Needs a Metal/Vulkan port (research) |
| Widescreen + HUD placement | GL (experimental) | GLES (experimental) | GL 4.1 (experimental) | Implemented in GL shaders today; backend-neutral design is open |
| Texture replacement | GL family | GLES | GL 4.1 | Hooks melonDS's texture cache; Software renderer excluded |
| Layouts, PiP, windows, settings, remapping, mic, saves, mods | All | All (second display: Unknown) | All | Backend-neutral by design |
| Running without OpenGL | Software + SDL_GPU (Phase 2) | n/a | Software + SDL_GPU (Phase 2) | Guards against OpenGL removal on macOS |

## 7. Minimal validation strategy (before ADR-0001 is accepted; no restructuring needed)

A **portability spike** in a scratch branch, ROM-free unless marked:

| # | Check | Targets | Pass criteria |
|---|---|---|---|
| V1 | Engine (recompiler + runtime + melonDS core) configures, builds, unit tests pass | Windows x64, Linux x64 (Ubuntu 22.04), macOS arm64 (`macos-14`), Linux arm64 (`ubuntu-24.04-arm`), Android arm64 (NDK compile + link) | Green CI on all five |
| V2 | Fiber conformance test (create, switch, nested calls, FP registers preserved, destroy) | all five (Android on emulator or device) | Identical results |
| V3 | Recompiler difftest (generated C vs Unicorn ARM946) | x64 and arm64 hosts | No mismatches |
| V4 | **Test cartridge end to end**: a small DS program we write with an open toolchain (e.g. BlocksDS) that draws on both screens, renders 3D, reads touch and mic, saves; recompiled with `ntr-recomp`, compiled with each platform's player toolchain, run headless N frames on the Software renderer | Windows, Linux, macOS (Android: one device or emulator) | Same frame hashes on every platform; module built by the player toolchain loads |
| V5 | Presenter smoke: synthetic two-screen frames composed in every layout, read back | GL 3.3 (Linux Mesa in CI), GL 4.1 (macOS runner), GLES 3 (Android emulator) | Matches CPU reference within tolerance; hit-test round trips pass |
| V6 | Mic pipeline: file-fed SDL3 recording (SDL "disk" audio driver, Likely) → resampler → `Mic_ReadInput` buffer | Linux CI; manual on macOS (TCC prompt) and Android (permission flow) | Expected level and timing; device open only between `Mic_Start`/`Mic_Stop` |
| V7 | Package skeletons (zip, AppImage, `.app`, APK) built and scanned by the guard | all | No game data; launch to the ROM-selection screen |

Gate: ADR-0001 becomes Accepted only when V1–V3 pass on all five targets and V4 passes on Windows, Linux and macOS
with at least a compile-and-run on Android. V4 depends on the runtime working without NitroSDK thread hooks for a
homebrew program (Likely; small engine changes may be needed, which also improve its generality).
