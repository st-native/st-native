# Notice

Spirit Tracks Native (working name) is free software under the GNU General Public License, version 3 or (at your
option) any later version ([LICENSE](LICENSE); ADR-0003). It is an unofficial fan project, not affiliated with or
endorsed by Nintendo. "The Legend of Zelda" and "Spirit Tracks" are trademarks of Nintendo, used only to describe
compatibility. This project contains no game code, assets or data, and does not distribute them; you need your own
copy of the game.

## Third-party code

| Component | Origin | Licence | How it is used |
|---|---|---|---|
| Recompiler and runtime (`ntr-recomp`, `ntr-runtime`) | [FFCCEOT-Recomp](https://github.com/JonathanDC64/FFCCEOT-Recomp) by JonathanDC64, commit `934c7434c9a6c2866fea17fa36098ef9c0652fc4` | GPL-3.0 | fetched at that commit by `spike/m1a/fetch_engine.py`; our changes are the patches in `spike/m1a/patches/` |
| melonDS (the console emulated around the recompiled game) | [melonDS](https://github.com/melonDS-emu/melonDS), commit `906e9ebb27da8c6a715cd7abab4abfe8a8d29427` (the engine's submodule) | GPL-3.0-or-later | fetched with the engine |
| AArch64 context switch for Android (patch 0003) | [Phantom Hourglass Recomp](https://github.com/dguillot-gh/phantom-hourglass-recomp) by dguillot-gh, commit `db0d762` | GPL-3.0 | ported with attribution in the patch |
| Game-data guard (`tools/check_no_game_data.py`) | adapted from FFCCEOT-Recomp's `tools/check_no_game_data.py`, commit `934c743` | GPL-3.0 | our adaptation |
| SDL3 (`release-3.4.18`), FreeType (`VER-2-14-3`), RmlUi (`6.3`) | fetched by the engine's build | zlib; FreeType License / GPL-2.0; MIT | linked into the program |
| Cinzel and Alegreya Sans | font files from the pinned FFCCEOT-Recomp checkout above | SIL Open Font License 1.1 | used by the launcher with the original font license files |
| Reggae One | [Fontworks Inc.](https://github.com/fontworks-fonts/Reggae/tree/fe9be09d02ad8ba5e71d5ce24efc5249fe77dcfc) | SIL Open Font License 1.1 | rasterized letters for the new title prompts via `tools/testing/render_title_prompt.py` |
| dsd (ds-decomp) 0.12.1 | [AetiasHax/ds-decomp](https://github.com/AetiasHax/ds-decomp) | MIT | downloaded by the user (`poc/fetch_tools.py --only dsd`), run on the user's machine; not redistributed |
| Capstone 5.0.7 | [capstone-engine](https://github.com/capstone-engine/capstone) | BSD-3-Clause | installed from PyPI by the user for our function discovery |
| LLVM / clang 22.1.8 | [LLVM](https://llvm.org) | Apache-2.0 with LLVM exceptions | the user's compiler (a release would bundle a pinned copy, ADR-0002) |

Our own code: the application (`app/`), the Spirit Tracks hooks and configuration (`game/st/config/`,
`game/st/hooks/`), the build, publishing and verification tools (`tools/`), the synthetic test program (`poc/sample/`)
and its cartridge (`spike/m1a/fixtures/`), and the documentation.

Hook addresses and names in `game/st/config/` are facts about the game's program (addresses, sizes, public NitroSDK
routine names), each recorded with how it was found; they contain no game code.
