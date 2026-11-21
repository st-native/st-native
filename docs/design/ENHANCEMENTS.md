# Enhancements: what the app and runtime can do, and what needs a game hook

Status: **Proposed** (2026-11-14, ADR-0008). For each goal: the part that lives in the **app** (window, input,
presentation, settings, launcher) or the **runtime** (the engine: melonDS core, renderer, scheduler, audio), and the
part that needs **integration with the game's logic**: a hook (`hooks.txt` `replace`/`before`/`instead`) or a read of
verified game state at a named address. Game integration is our own small C/C++ at verified addresses, never a
translation of the game's functions; every address records how it was found (AGENTS.md).

Every enhancement is optional, off in the "Original" preset, and checked with the scripted replays (identical
per-frame hashes when it is off; the documented difference when it is on). Labels: **Verified** (the engine already
does it, checked in our build), **Likely** (the engine has it, not yet tried on Spirit Tracks), **Proposed**.

| Goal | App / runtime (no game logic) | Game integration (hooks, verified state) |
|---|---|---|
| **Remappable controls** | Shared Controls editor for keyboard/mouse and gamepad, primary/alternate bindings, conflicts, reset, device-type labels and persisted settings (Verified on Windows). Existing live DS/utility actions use the same model; six instrument actions await context integration. Left stick remains D-pad; full modern gameplay profiles remain M4 work. | None for DS buttons. Verified flute context still needed for button notes. |
| **Controller navigation in original menus** | Optional PC game menus: verified title accepts any non-reserved button, file/name menus have discrete stick/D-pad focus and remappable confirm/back/erase/clear actions. Start accepts the name; L3 opens port settings. Conflicts show a confirmed transfer dialog. Verified locally with SDL events and a named-file creation; physical-device testing remains open. | Uses ScreenContext's file-menu signature, verified menu fields and NameKeyboard's existing hook. Other campaign menus need separate verification. See CONTROLS_AND_MICROPHONE. |
| **DS gameplay that does not suit modern controls** (Spirit Tracks is stylus-first: walking, sword, rolling, items, train lever and whistle, map drawing, the Spirit Flute) | A touch router: the cursor driven by the stick or mouse; gestures (tap, swipe, circle) bound to buttons; on-screen regions for fixed UI (the train's lever and whistle sit at fixed bottom-screen positions). Microphone: real input and a synthesized "blow" (melonDS mic interface, ADR-0006). | **Needed for good controller play:** where Link is on screen and which mode the game is in (field, train, menu, map) to aim gestures; direct analog movement and button attacks (as the engine does for FFCC: `instead` hooks on movement and input packing); item aiming. Read-only state first, hooks only where reading is not enough. |
| **Spirit Flute by buttons** (Proposed) | Six remappable note actions for keyboard and gamepad. Controller-first candidate: two shoulders plus four face buttons. Press to play, hold to sustain, release to stop. Controls pages and in-game hints share the active bindings and automatic/manual Xbox/PlayStation/Nintendo symbols (CONTROLS_AND_MICROPHONE §3). Start with tube selection and virtual breath. | Use a small note-selection hook if touch synthesis introduces delay or plays intermediate notes. Preserve the original duet timing and keep stylus and mic input in the Original preset. Validate with a solo song and Gage's duet using only buttons and no physical microphone. |
| **Flexible layouts for the two screens** | Side by side, stacked, one large plus one small, swap, a second window, rotation, gaps and scaling: all presentation (DUAL_SCREEN_UX). | **Auto layout** (e.g. enlarge the top screen while the bottom one shows a static map) needs a verified "what is on each screen" flag (ADR-0005: from game state, never from pixels). |
| **Selected cutscenes adapted to one screen** | Composing both screens into one frame, crops and transitions: presentation. | **Needed:** knowing that a cutscene runs and which one (a hook on its start/end); for cutscenes whose camera spans both screens, re-framing means changing the game's camera or projection for those scenes (hooks at the projection setup, as FFCC's aspect hook). Per cutscene, curated. |
| **Other modernisations of things that aged badly** (Proposed examples: faster text, skipping already-seen cutscenes, quicker repetitive train trips, less stylus-only friction in menus) | Fast-forward on demand (Verified: Tab); speed-ups against load stalls (Likely: fast cartridge reads and a faster ARM9 are engine defaults); save states (Proposed, runtime). | **Needed** for anything that changes the game's rules or pacing: text speed, cutscene skip, train travel. Each one a reviewed hook, listed and labelled, off by default. |
| **Higher internal resolution** | Verified on Windows. Software at 1x and OpenGL at higher scales. The launcher offers 1x, 2x, 3x, 4x, 5x, 6x, 8x, 10x and 12x per screen. The CLI also accepts `--internal-scale 1-16`. | None. (2D layers stay at native resolution unless replaced by mods.) |
| **Widescreen** | Rendering a wider 3D picture (`VideoSettings::wide_extra`, Likely) and placing HUD sprites at the edges (`hud_edges`, Likely). | **Needed:** the game's projection must be widened (the engine's FFCC hook on the perspective setup is the model), objects the game culls outside 4:3 must not pop, and 2D backgrounds do not extend. Experimental, per scene. |
| **Smoother motion without speeding up the simulation** | The game draws its 3D at a lower rate than the display; the engine blends the last two scenes for each displayed frame (`interpolate`, `display_rate` with `render_between`, Likely). The simulation keeps the DS's own pace (VBlank-paced; the ARM9 speed-up only idles longer). | **Helpful:** a hint at camera cuts and scene changes, so frames are not blended across a cut (a hook or a verified flag). |
| **Mods** | Texture replacement from the renderer's texture cache (engine plan); file-level replacement of the game's own files as the cartridge is read (the runtime already logs each file read: `trace_files`); a mod manager (SETTINGS_AND_MODDING). Mods ship their own content, never game data. | **Needed** for gameplay or code mods: they are hooks, loaded and listed like the enhancement hooks. |

## Order

1. Windows x64 first (M2-M4): remapping, touch router, layouts, launcher.
2. Graphics (M5): resolution, interpolation, then experimental widescreen and single-screen cutscenes.
3. Mods (M7) once hooks and settings are stable.

## What makes a game hook acceptable

- The address is a fact found in our own analysis of the player's dump (the R1 tools, the recompiler's report or the
  call counters) and recorded with how it was found.
- The hook is small and does one thing; it calls the game's own recompiled functions where possible instead of
  re-implementing them.
- It is checked with a scripted replay: off gives the original per-frame hashes; on gives a documented change.
