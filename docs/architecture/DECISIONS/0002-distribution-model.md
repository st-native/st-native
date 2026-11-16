# ADR-0002: Releases contain no game-derived code; the game module is built on the player's machine

- Status: **Accepted** (2026-11-14, with [ADR-0008](0008-static-recompilation-port.md)): the project is a
  recompilation port, which is this record's premise. The player selects their own ROM in the launcher; the module is
  generated and compiled on the player's machine. (Was "under review" while ADR-0001 rev. 3 planned published
  reconstructed source.)
- Date: 2026-11-12

## Context

The recompiled C and the compiled game module are translations of Nintendo's code. Distributing them would distribute
a derivative of the game's program. Excluding the ROM from Git is not enough if binaries contain translated game code.

## Options

1. Publish prebuilt game modules — legally untenable.
2. Source-only; users run build scripts (PH Recomp today) — safe, but hard for players and invites sharing of builds.
3. **Ship the program, recompiler and a pinned compiler; build the module on first launch from the player's own ROM**
   (FFCCEOT-Recomp, Wind Waker HD Recomp, Egg of Time).

## Decision

Option 3. Release packages contain only our code, third-party libraries with their licences, the recompiler, and a
pinned, checksum-verified toolchain (FFCC bundles LLVM 22.1.8; WWHD downloads llvm-mingw/zig on first use). Setup
verifies the ROM's SHA-1, and the module is cached per ABI version, ROM hash and config hash. Packaging scans each
artifact for game data before upload (FFCC `package.py` already does).

## Toolchain per platform (Proposed; chosen by the portability spike)

| Platform | Player-side compiler | Precedent |
|---|---|---|
| Windows x64 | Bundled LLVM (clang-cl, lld-link), SHA-256 pinned | FFCC (LLVM 22.1.8) |
| Linux x64 / Steam Deck | Bundled LLVM inside the AppImage | FFCC |
| macOS Apple Silicon | Xcode Command Line Tools (prompted), or bundled LLVM; module ad-hoc signed by the linker | WWHD (Command Line Tools) |
| Android ARM64 | Android-hosted clang/lld shipped as APK native libraries (Android 10+ forbids executing binaries from writable storage), API 33+ | WWHD (LLVM 20.1.8 built with the NDK) |

## Consequences

- First launch takes minutes (WWHD reports minutes on phones too); an engine update that changes generated code
  triggers an automatic rebuild.
- Package size grows (FFCC's AppImage ≈ 210 MB, mostly LLVM; WWHD's Android compiler ≈ 200 MB).
- Android developer path: PH builds the APK on a PC with the game module inside; such an APK is for the builder's own
  device only and must never be distributed.
- macOS: a notarized, hardened-runtime app needs the library-validation exception to load a locally built module;
  ad-hoc distribution avoids notarization but triggers Gatekeeper prompts. Maintainer decision before macOS releases.
- This reduces, but does not remove, legal risk (see `docs/legal/LICENSING_AND_PROVENANCE.md`).
