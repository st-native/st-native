# ADR-0006: Real microphone through melonDS's mic interface; virtual blowing from our own synthesized signal

- Status: **Proposed**
- Date: 2026-11-12

## Context

Spirit Tracks uses the microphone for the Whirlwind and the Spirit Pipes (manual p.24, p.26), plus a mic test and
probably other interactions. The pinned melonDS pulls mic samples through `Platform::Mic_ReadInput` (signed 16-bit mono,
~47.6 kHz) and calls `Platform::Mic_Start/Mic_Stop` when the game powers the mic channel. PH Recomp leaves Start/Stop
empty and feeds white noise or a looped third-party WAV of unknown licence while a button is held; its author reports
noise was not enough for blow interactions. An external D-pad patch supplied candidate overlay-0 address
`0x02070128` (USA) and value `0x5F`; its exact role/signature still needs independent verification before use.

## Options for virtual blowing

1. Third-party recorded sample — rejected (licence unknown; also not tunable).
2. **Synthesized signal at the hardware level**: deterministic noise with an envelope and calibrated energy, fed
   through the same path as the real mic. Game-agnostic, exercises the game's own detection.
3. **Game-level hook** on the mic-loudness function: exact and cheap, but per-release addresses, and bypasses the game's
   own signal processing (may differ from real-mic behaviour).
4. Button-note flute controls — originally rejected by the initial requirement; the maintainer later requested
   an optional button-note mode. **Proposed**, tracked in ENHANCEMENTS.md; it is separate from virtual blowing.

## Decision

- Real mic: implemented in `app/audio` behind a `MicSource` interface; the physical device is opened only between
  `Mic_Start` and `Mic_Stop` and only when the player chose the real microphone. Android requests the permission only
  when that choice is made.
- Virtual blowing: option 2 by default. Option 3 is kept as a verified fallback **per mechanic** if option 2 cannot be
  made reliable for that mechanic in testing.
- Arbitration: while the virtual-blow input is held, the virtual signal replaces the real mic signal (no summing); in
  Auto mode the source is chosen when the game opens the mic and does not change until it closes it.
- Neither path plays notes, selects pipes or completes puzzles: they only produce "breath". Note selection stays with
  the player.
- **Requirement update, 2026-11-19:** an optional flute mode may bind each player-selected note to a key/button,
  combining tube selection with virtual breath while held (see [ENHANCEMENTS.md](../../design/ENHANCEMENTS.md)).
  It must preserve song/turn timing and leave note choice to the player. The original stylus/microphone path remains
  available. Button-note implementation and its duet validation are still pending. Microphone signal generation
  itself remains game-agnostic and does not select notes.
- **Calibration, 2026-11-19:** quiet band-pass noise failed Gage's initial teaching stage despite reaching the ADC.
  The positive local reference used melonDS's explicitly GPL-licensed frontend signal. Our own generator now uses
  enveloped bipolar noise with sufficient sustained energy. It passes teaching and the full Gage duet in the native
  build with pointer-selected notes (`tests/rom/gage_duet.txt`), including the rail reward and world return.
  It contains no recorded sample. Other duets, other mechanics and real-mic comparison remain open.

## Verification

Each mic mechanic has a test in `docs/design/CONTROLS_AND_MICROPHONE.md` §6, run with real mic and with virtual blow,
including full Spirit Pipes songs and duets. A `--mic-log` option records Start/Stop events and levels (no audio).
