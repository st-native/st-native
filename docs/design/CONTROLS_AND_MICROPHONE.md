# Controls and microphone

Status: **Proposed** (2026-11-12). Related: [GAMEPLAY_INTERACTION_MATRIX.md](GAMEPLAY_INTERACTION_MATRIX.md),
[DUAL_SCREEN_UX.md](DUAL_SCREEN_UX.md), ADR-0006.

## 1. What the original game receives

| DS input | How the game sees it | How the runtime feeds it (Verified, `ntr::Runtime`) |
|---|---|---|
| Buttons A B X Y L R Start Select, D-pad | KEYINPUT/EXTKEYIN registers | `set_buttons(held, pressed)`; `pressed` keeps taps shorter than a frame (`input_latch`) |
| Touch Screen | touch controller over SPI, 256×192 | `touch(x, y)` / `release_touch()` |
| Microphone | touch controller's mic channel, sampled by the game | melonDS `Platform::Mic_Start/Mic_Stop/Mic_ReadInput` (16-bit, ~47.6 kHz pull) |
| Lid | hinge switch (Sleep Mode) | `set_lid_closed(bool)` |

The manual (p.12) says the game is "primarily played using the Touch Screen and microphone"; buttons are shortcuts
(menu bar, map, collection, rail map/rail screen while driving, character switch; L/R take out the equipped item;
START pauses and skips cutscenes). The game also has a **handedness** option that mirrors parts of the UI (p.11).
The original stylus wording should become a device-aware controls preference in the PC presentation. The current
choice and save setting remain unchanged; see [Narrative presentation](NARRATIVE_PRESENTATION.md#stylus-and-handedness-prompt).

## 2. Input architecture

```
physical devices ──► actions (profile + bindings) ──► DS events / scheme actions ──► runtime (per frame)
 SDL3 keyboard, mouse,      ds.* buttons, stylus.*,      buttons, stylus, mic-blow,      set_buttons/touch,
 gamepads, touch, pen,      move.*, attack.*, train.*,   or game/st scheme output        MicSource state
 sensors; Android touch     phantom.*, mic.blow, ui.*
```

- **Actions** are named and typed (button, axis, pointer). Profiles bind devices to actions; one action may have
  several bindings; chords are allowed (e.g. Select + D-pad, as the D-pad patch uses).
- **Conflict detection** per context layer: bindings in the same layer that collide are flagged in the UI; layers are
  *always*, *gameplay*, *pointer/draw mode*, *train*, *phantom*, *menu overlay*.
- Per-device settings: dead zones (inner/outer, radial), response curve, pointer sensitivity and acceleration,
  invert axes, trigger thresholds, vibration (none in the original).
- **Glyphs**: Xbox, PlayStation, Nintendo, Steam Deck, keyboard (FFCC already names buttons per controller).
- **Reset to defaults** per profile and per action.
- Library: **SDL3** (already in both upstreams; gamepad database, HIDAPI drivers for PlayStation and Steam Deck
  controllers, sensor and touchpad APIs; which devices expose touchpads/gyro through SDL is to be verified per
  device). Steam Input works through SDL's virtual gamepad; we ship sensible defaults and do not require a Steam Input
  template.

## 3. Profiles

### Controller-first requirements (maintainer, 2026-11-20)

**Requirement, with a bounded implementation below.** The maintainer expects most players to use a gamepad. Give the
controller path the same complete gameplay coverage as keyboard/mouse, including configuration without a mouse.
The Controls foundation now implements remapping of the existing DS/utility actions and configuration of six
instrument actions. Button-note gameplay and adapted in-game flute prompts remain open. The visual flute study
is a proposal, not evidence of those features.

- One **Controls** category in the launcher and Esc/F1 overlay, with **Keyboard & mouse** and **Controller** pages.
  Include a contextual **Flute** group rather than a separate binding system. Group actions by general play,
  menus, pointer/map, train, partner and instrument as those contexts become supported.
- Both device paths support rebinding, contextual conflict notices, restore defaults and a preview of the bound
  action. Keyboard/mouse adds pointer sensitivity and mouse bindings; controller adds active device, stick dead
  zones, pointer sensitivity and trigger thresholds. Do not expose options whose input behavior is unimplemented.
- A single action/binding model supplies gameplay, configuration labels, tutorials and HUD hints. After a rebind,
  the instrument and any adapted prompt show that binding, never a fixed letter or a DS button from the ROM.
- **Button symbols:** Automatic / Xbox / PlayStation / Nintendo (Deck-specific cues where supported). Prefer the
  active device's reported type and labels, with a manual display override for virtual/unknown devices. SDL3 has
  [gamepad types](https://wiki.libsdl.org/SDL3/SDL_GetGamepadType) and
  [button labels](https://wiki.libsdl.org/SDL3/SDL_GetGamepadButtonLabel); actual device coverage needs testing.
  Changing symbol style alone must not change the action bindings.
- Store gamepad bindings by physical SDL position/action, not a manufacturer letter. The same south button is
  Xbox A, PlayStation Cross and Nintendo B; a Nintendo face-label change must not accidentally rebind a note.
  Where an explicit label-based preset is offered, its behavior is separate and visible.
- Automatic hints follow deliberate input from the active device. Ignore idle stick noise and background mouse
  motion; avoid glyph flicker during mixed input. Reconnection, multiple devices and a virtual controller need
  an explicit fallback. Do not claim the physical brand is always detectable through a virtual device.
- Use restrained, readable glyphs matching the game's UI. Preserve note identity with color plus shape/binding,
  highlight held notes, and show teaching text only when relevant. The symbol assets need compatible licensing.

**Proposed first flute preset:** four face buttons and two digital shoulder buttons, one action per pipe.
Hold sustains breath/note, release stops; no separate blow modifier. These are candidates for ergonomic testing,
not independently verified guest pipe coordinates or a guarantee for every duet. All remain remappable.

| Pipe identity | Physical input | Xbox | PlayStation | Nintendo | Keyboard candidate |
|---|---|---|---|---|---|
| Green | Left shoulder | LB | L1 | L | A |
| White | West face | X | Square | Y | S |
| Blue | North face | Y | Triangle | X | D |
| Orange | South face | A | Cross | B | F |
| Yellow | East face | B | Circle | A | J |
| Purple | Right shoulder | RB | R1 | R | K |

Acceptance: finish Gage using only gamepad notes, then keyboard notes; test quick changes, repeated notes,
rests, overlapping held buttons, cancellation and interruptions. Verify labels after remapping and each symbol
override, including Nintendo's face positions. Release note/touch/breath on menu open, focus loss and device
disconnect. Extend to solo songs and other duets before claiming campaign coverage.

### Implemented foundation (2026-11-20; Windows execution verified)

- Shared action/binding/state code in `app/input/controls.hpp`, SDL device ownership in `gamepads.hpp`, and one
  `app/ui/controls_panel.hpp` used by the launcher and Esc/F1 overlay. No new dependency, engine patch or guest hook.
- Keyboard/mouse and controller pages have primary/alternate capture, clearing, contextual conflict rejection and
  reset per device. Settings persist in versioned `controls-v3` / `binding-v1` records in `launcher.cfg`; old files
  load defaults, invalid/out-of-context bindings cannot activate, and conflicting sets recover to defaults.
  The former `controls-v1` Start/Menu preset migrates to the game Start binding, preserving custom L3 bindings.
- Existing DS buttons, touch/draw, virtual blow, upper-screen peek, menu, original view and fast-forward consume
  this model in live input. Mouse buttons may be rebound. The left stick retains D-pad behavior with adjustable
  dead zone; trigger bindings use the configured threshold. This does not implement analog Link movement or combat
  gestures. The verified file and name menus support direct controller focus.
- Six instrument bindings are saved and previewed but the live app keeps their context disabled until the flute
  adapter is verified. Their resolver supports last-pressed priority and returning to a still-held note when a
  newer one is released. It never chooses notes on the player's behalf.
- Automatic device-type labels plus Xbox/PlayStation/Nintendo overrides are implemented as readable text
  (`Cross`, `Square`, `L1`, etc.); illustrated glyph assets and adapted game prompts remain separate work.
  Unknown devices use neutral face positions. Changing label style does not alter physical bindings. Map hints
  in settings read the configured bindings rather than a fixed M key.
- Connected controllers are enumerated on startup and handled on hotplug; deliberate button/large axis input
  selects the active controller, ignoring small axis noise. Physical device coverage is not established by a
  synthetic SDL controller test. Steam Input may expose a virtual device; manual label override remains available.
- Gamepad D-pad up/down traverses menu focus, left/right adjusts controls, and the south face activates.
  This describes the port's settings UI; the separate game-menu adapter is described below.
  Start/Menu/Options/Plus sends **DS Start**, and Back/View/Share/Create/Minus sends **DS Select**.
  L3 opens/closes the settings overlay by default and is remappable. Keyboard Enter/Backspace retain DS Start/Select.
- Esc/F1 are fixed keyboard recovery routes. During capture Esc or the configured controller settings-menu
  button cancels and Delete clears; while
  focusing a binding, the controller's north face clears it without requiring a keyboard.
  F11 is reserved for fullscreen; F2 remains the original-view recovery key. Focus loss, menu entry/exit,
  controller removal/switch, name typing and changed settings clear held input instead of replaying stale presses.

**Verification:** C++ tests cover conflicts, malformed bindings, alternate/device overlap, note priority,
release/reset, dead zones, trigger thresholds, manufacturer labels and virtual-device lifecycle. Preference tests
cover legacy files, remap swaps and damaged records. Launcher smoke checks the same editor in both menus:
keyboard/controller rebinding, conflict cancellation, label overrides without rebinding and gamepad
traversal/activation. A physical gamepad session and button-only Gage acceptance remain open.

### PC game menus (2026-11-20)

**Implemented:** **PC game menus** enables discrete focus on verified file, name and mode screens.
In the verified option menus, focus changes the palette of the original artwork. A notice with one
action keeps its original advance marker. D-pad or stick changes the selection, and the south face
confirms through the game's existing action. Keyboard arrows and physical name entry also work.
Start accepts the name, and L3 still opens port settings.

- The train opening accepts a keyboard or controller button through the game's Start input. A separate
  press activates the title. At launch, the PC option replaces only the title caption cell's indexed
  tiles in the in-memory NitroFS resource. The original title palette, animated ribbon, feather and
  credits are still drawn by the game. The original instruction is baked into an image. Reggae One
  by Fontworks was the closest match in a local comparison of the original letter shapes. The
  project's new phrases are rasterized at DS size from the open font. The exact original typeface
  remains unconfirmed. Regenerate the letter masks with `tools/testing/render_title_prompt.py`.
- In the narrated prologue, **Skip opening story** is remappable and defaults to Start/Enter.
  The first press reveals the Skip button. Press the shortcut again or click the button to skip.
  With PC game menus enabled, the original button and lettering stay visible. A small matching
  label shows the binding for the most recently used input device.
- The PC adapter uses verified display states and read-only BKIE menu fields, plus the existing
  editable-name hook. Unsupported contexts retain the original DS inputs. The original title
  art returns on the next launch after disabling PC game menus.
- Changing the selected option plays the original name keyboard's press sample. The port reads the
  wave from the player's ROM, plays one short pass at the pitch measured in the game and mixes it
  into the normal output stream. Initial focus, blocked movement and confirmation do not trigger
  the added cue. Confirm continues to use the game's own action and sound.
- The conflict dialog identifies existing assignments and requires confirmation before transferring
  a binding. Other input contexts retain their assignments.

**Verification:** `tests/rom/controller_menus.txt` covers the title, slot 2, keyboard focus,
name and handedness confirmations, save creation, Adventure and the narrated prologue. This coverage
does not extend to the whole campaign or alternate keyboard pages. Physical controller validation remains
open. See [Controller menu replay](../BUILDING.md#controller-menu-replay).

**Asset verification, 2026-11-21.** The English launcher replay confirmed the new title and selected
palette on both slots. Comparing the decompressed archives for English, French and Spanish showed
that edits stay in caption tiles. The on-disk ROM is read only. The slot palettes are derived from the
local SelectM and KeyboardBG resources. `tests/rom/controller_menu_feedback.txt` covered both saves,
the name keyboard, Yes/No, Start/Copy/Erase, and Adventure/Battle/Tag Mode/Options. **Verified**
with SDL's dummy audio driver: 11 cues entered the output stream for 11 selection changes, and a
blocked Down produced none. The 3,000-frame original-input Gage replay still matches the reference.
Listening on physical speakers remains untested.

**Startup submenus.** Up and Down navigate Options. Left and Right change the selected value;
Confirm on a value advances it. Mic Test, Cancel and OK use the same focus and confirmation
bindings. Back exits Mic Test, cancels Options and selects No in confirmation dialogs. Copy and
Erase start with No selected and use the game's own confirmation and save flow. The Battle lobby,
Player List and waiting page support menu navigation. The Tag Mode back arrow is also available;
choosing and moving treasures for exchange still uses touch or mouse.

The extra contexts are tracked by replaying the original touch routes for BKIE rev. 0 and
comparing display state and read-only RAM. Options and save dialogs reuse palette variants
from SelectM. Battle lobby buttons use BselM_A and the list arrows use ListM. See
`tests/rom/startup_options.txt` and `tests/rom/startup_side_menus.txt` for the controller and
keyboard replays. Multiplayer sessions and physical microphone input are unverified.

**Direct analog movement remains under investigation.** A read-only Forest Sanctuary checkpoint and
idle/right/left/up replays verified the change and decay of the horizontal movement vector. A local
write probe traced the vector to overlay 17's frame update at `0x020bc620` in BKIE rev. 0. This is
downstream of input. The earlier movement request and the states that allow it still need verification
before binding the analog stick to that request.

### Planned gameplay profiles

| Profile | Movement | Combat | Gestures (boomerang, routes, map) | Notes |
|---|---|---|---|---|
| **Classic** | Stylus | Stylus | Stylus | Buttons exactly as the DS. Reference for correctness. |
| **Keyboard + mouse** | WASD (scheme) or mouse | Mouse (original taps/slides) + keys | Mouse | Desktop default. |
| **Modern controller** | Left stick (scheme) | Buttons (scheme) | Pointer on right stick/trackpad/gyro, trigger = stylus down | Controller default; fully playable without mouse. |
| **Hybrid** | Stick/WASD | Buttons | Mouse or trackpad | Steam Deck default (trackpads as pointer). |
| **Mobile** | Touch, optional virtual stick | Touch | Touch | Virtual buttons movable/resizable per orientation (PH editor). |

A scheme action never removes the stylus path: the pointer/mouse/finger always works at the same time.

## 4. Modern-control schemes (game/st/controls)

Two implementation techniques, chosen per action after verification:

| Technique | How | Proven where | Risk |
|---|---|---|---|
| **Stylus synthesis** | Convert the action into the stylus input the game expects (e.g. hold the stylus at Link's screen position + stick direction × distance) | PH Recomp `ph_stick.cpp` (author reports it works well) | Needs Link's screen position; camera angles; must yield to real touches |
| **Hook-level input** | `before`/`instead` hook at a verified site sets the same fields the touch path would (speed, angle, attack kind) | D-pad patch (ROM patch, same idea) | Per-release addresses; must not create states the stylus cannot produce |

Rules: a scheme writes only values the original input could produce (e.g. walk vs run speeds the stylus distance
gives), keeps the game's own cool-downs (spin dizziness), and is disabled outside the contexts it was verified in.

| Action | First technique to try | Notes |
|---|---|---|
| Analog walk/run | Stylus synthesis | Run vs walk by stick magnitude, matching stylus distance thresholds |
| Roll | Synthesized double tap ahead of Link | Original gesture (M p.18) |
| Lock-on slash | Synthesized tap on nearest target (or hook) | Target choice = experimental enhancement |
| Slide slash / lunge / spin | Synthesized slide relative to Link's facing / circle | Same gesture the game recognises |
| Interact | Synthesized tap at the facing point (the patch does this) | |
| Train gears | Synthesized drag on the lever between detents | Detent positions per handedness |
| Whistle | Synthesized downward slide on the cord (M p.28) | |
| Look around | Synthesized touch at screen edges (M p.28) | |
| Phantom command mode | Pointer drawing with a mode toggle; stop/call as taps on the game's icons | Icon positions per handedness |
| Pipes note selection | Synthesized horizontal slide on the pipes | Only if equivalent; otherwise pointer |

## 5. Steam Deck

- Fully playable with built-in controls: Modern/Hybrid profile, right trackpad = pointer (relative, click = stylus
  down) and an **absolute map mode** (trackpad area mapped to the Touch Screen view) for map notes and routes.
- Back buttons default to: blow, toggle PiP, Phantom mode, pointer draw mode.
- Settings overlay opens with a long press on View (WWHD solves the "no keyboard in Gaming Mode" problem this way).
- Gamescope detection selects the Deck layout preset (DUAL_SCREEN_UX §7). Suspend/resume: release stylus and
  mic on suspend; reopen the mic only if the game still has it open.

## 6. Microphone

### 6.1 Modes

| Mode | Behaviour |
|---|---|
| **Real microphone** | Selected input device feeds the DS mic. Device opened only while the game has the mic on (`Mic_Start`…`Mic_Stop`) and only if this mode is chosen. |
| **Virtual blowing** | A bound key/button/touch control produces a synthesized breath signal while held (or toggled, accessibility option with timeout). |
| **Hybrid** | Real mic active; the virtual control overrides it while held (replace, never sum). |
| **Automatic** | Real mic if a device is available when the game opens the mic, else virtual. The choice is fixed until the game closes the mic: no switching mid-interaction. |
| **Off** | The DS mic hears silence. Shown as a warning when the game opens the mic. |

### 6.2 Real microphone requirements
- Device selection (system default or a named device), gain (−12…+24 dB), noise-floor calibration (measure 2 s of
  silence → gate threshold), sensitivity/threshold, live **level meter** (in settings and as an optional on-screen
  indicator while the game listens).
- Latency: capture buffer ≤ 20 ms target; resample to 47,619 Hz in the capture callback; ring buffer drained by
  `Mic_ReadInput`; underrun = silence (never stale audio).
- Reconnect: device loss → switch to the mode's fallback (Automatic) or show "microphone disconnected" (Real),
  retry on device-added events; no crash, no blocking the game thread.
- Errors in plain language (no device, permission denied, device busy).
- Privacy: nothing recorded, stored or transmitted; the `--mic-log` diagnostic stores events and levels only. A
  visible indicator shows when the physical mic is open.
- Android: `RECORD_AUDIO` requested only when the player selects Real/Hybrid/Automatic; denial falls back to Virtual
  with an explanation.

### 6.3 Virtual blowing signal (implemented; mechanic coverage incomplete)
- Generator: deterministic bipolar noise at 47,605 Hz → envelope (attack 30 ms, release 80 ms) → gain and signed
  16-bit clipping. It uses our PRNG and no recorded sample. Zero gain is exact silence; steady gain 100 has high
  sustained energy and a near-zero DC mean.
- **Verified, 2026-11-19:** this signal passes Gage's teaching and all three turns of the actual duet in the native
  build (`tests/rom/gage_duet.txt`), followed by the joint performance, restored rails and resumed walking. The former quiet
  band-pass signal reached the core ADC but did not pass that stage. A private comparison using the melonDS team's
  explicitly GPL-licensed frontend signal supplied the positive reference; that recording is not part of our generator.
- MIC TEST, real-mic comparison, other duets and other mechanics remain acceptance checks below. This is virtual
  breath with pointer-selected notes; optional button-note controls and a one-canvas flute view are not implemented.
- Calibration procedure: (1) open MIC TEST (C27); (2) blow into a real mic at a comfortable distance and record the
  game-visible level via the mic-level function (read-only probe) or the meter; (3) set the default strength to match;
  (4) validate on every mechanic in §6.5.
- Fallback per mechanic (ADR-0006): a small hook on a verified input-level function if synthesis proves unreliable.
  The external candidate USA ov0 `0x02070128` has not been independently established as a no-argument volume getter;
  isolated execution needs additional call state. No fallback hook is enabled from that hypothesis.

### 6.4 What virtual blowing must not do
It never selects notes, never plays songs, never aims the Whirlwind, never completes puzzles. It is "breath" only.

### 6.5 Mic compatibility tests (local, with ROM; each in Real and Virtual modes)

| Test | Pass criteria |
|---|---|
| MIC TEST meter | Meter responds; silence reads ~0; virtual blow reads strong and steady |
| Whirlwind | Fires in the aimed direction; stuns enemies; works in each dungeon use |
| Spirit Pipes: each learned song, solo | Correct notes with player-selected pipes; song recognised; same breath-length tolerance as real blowing |
| Spirit Pipes duets | Timing windows met at normal difficulty with both input modes. Gage Verified with virtual breath and pointer-selected notes; real mic and other duets remain open. |
| Dust-blowing interaction(s) | Completes as with a real mic |
| Any other `Mic_Start` site found by `--mic-log` | Behaviour matches real-mic behaviour |
| Hybrid arbitration | Holding virtual blow while speaking gives the virtual result only |
| Device unplug during a song | Falls back or warns; game continues; no stuck "blow" |
| Android suspend during mic use | Mic closed on suspend, reopened on resume only if the game still listens |

### 6.6 Numeric microphone diagnostics

**Implemented, 2026-11-19:** the CLI option `--mic-profile build/<run>/microphone.csv` records one row per
completed video frame: callback read count, delivered sample count, absolute peak, listening/paused state and
the requested virtual-blow state. No waveform/audio samples or game memory are written. Use a new private
output directory; the CSV stays below ignored `build/`, alongside local replay results.

Counts/peak cover reads since the preceding row. Silent, paused and Off reads count their zero-filled samples;
no read is distinct from a read containing silence. `virtual_blow` is the requested input, not a claim that the
game accepted a note. The synthesized microphone input can remain nonzero briefly after that flag becomes zero.
The snapshot resets diagnostic counters only, preserving synthesis state, capture data and the Start/Stop log.
It measures the host microphone callback, not the game's note decoder. Compare identical scripted runs with
and without this option before using it as evidence; diagnostic runs are not performance benchmarks.

Synthetic audio tests check nonzero blow, release to silence, empty snapshots, paused reads and identical output
with/without diagnostic snapshots. Full songs/duets still require the ROM compatibility checks above.

## 7. Accessibility hooks into controls

Hold/toggle options for blow and stylus-hold actions, adjustable blow strength, one-handed layouts (all actions on
one side), pointer speed and snapping for drawing, large virtual buttons on Android, remappable everything, and a
left-handed preset that follows the game's handedness option.
