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

### 6.3 Virtual blowing signal (Proposed, to calibrate)
- Generator: white noise → band-pass (≈ 200 Hz–4 kHz) → envelope (attack 30 ms, release 80 ms) → amplitude set by
  "blow strength" (default calibrated so the game's MIC TEST meter reads a strong, steady blow).
- Optional slow amplitude modulation so detectors that look for natural variation still trigger (to test).
- Calibration procedure: (1) open MIC TEST (C27); (2) blow into a real mic at a comfortable distance and record the
  game-visible level via the mic-level function (read-only probe) or the meter; (3) set the default strength to match;
  (4) validate on every mechanic in §6.5.
- Fallback per mechanic (ADR-0006): game-level hook on the mic-loudness function (USA ov0 `0x02070128`, from the
  D-pad patch; to verify) returning the calibrated value while virtual blow is held.

### 6.4 What virtual blowing must not do
It never selects notes, never plays songs, never aims the Whirlwind, never completes puzzles. It is "breath" only.

### 6.5 Mic compatibility tests (local, with ROM; each in Real and Virtual modes)

| Test | Pass criteria |
|---|---|
| MIC TEST meter | Meter responds; silence reads ~0; virtual blow reads strong and steady |
| Whirlwind | Fires in the aimed direction; stuns enemies; works in each dungeon use |
| Spirit Pipes: each learned song, solo | Correct notes with player-selected pipes; song recognised; same breath-length tolerance as real blowing |
| Spirit Pipes duets (if present) | Timing windows met at normal difficulty with both input modes |
| Dust-blowing interaction(s) | Completes as with a real mic |
| Any other `Mic_Start` site found by `--mic-log` | Behaviour matches real-mic behaviour |
| Hybrid arbitration | Holding virtual blow while speaking gives the virtual result only |
| Device unplug during a song | Falls back or warns; game continues; no stuck "blow" |
| Android suspend during mic use | Mic closed on suspend, reopened on resume only if the game still listens |

## 7. Accessibility hooks into controls
Hold/toggle options for blow and stylus-hold actions, adjustable blow strength, one-handed layouts (all actions on
one side), pointer speed and snapping for drawing, large virtual buttons on Android, remappable everything, and a
left-handed preset that follows the game's handedness option.
