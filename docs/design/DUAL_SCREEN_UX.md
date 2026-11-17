# Dual-screen presentation and input routing

Status: **Proposed** design (2026-11-12). Starting point in code: FFCCEOT-Recomp `game/src/video.{hpp,cpp}`
(`Layout`, `Presenter`, `screen_at`, second window) and PH Recomp `frontend/src/layout.hpp` (`to_ds`, widescreen
HUD moves). Reference behaviour: Wind Waker HD Recomp `docs/gamepad-display.md`. Decision record: ADR-0005.

## 1. Vocabulary

| Term | Meaning |
|---|---|
| **Top screen / Touch Screen** | The two physical DS screens. Only the Touch Screen (physical bottom) ever receives stylus input. |
| **Engine A / B** | The DS 2D engines. Engine A also carries the 3D layer. A display-swap bit decides which physical screen shows engine A. |
| **3D screen** | The physical screen currently showing engine A's 3D (a hardware fact the runtime can report every frame). In Spirit Tracks exploration it is the Touch Screen; the top screen shows the map (manual p.17). |
| **View** | One DS screen drawn somewhere: window, rect, z-order, opacity, frame, `interactive` flag. |
| **Layout** | The set of views for all windows, computed from settings and context. |

## 2. Principles

1. Both DS screens remain reachable in every mode, by at most one input (toggle, PiP tap, hotkey or button).
2. A hidden view never receives input; a pointer over nothing interactive is dropped, never redirected.
3. Only the Touch Screen is touchable. Top-screen views are display-only unless a game-specific, verified integration
   says what a tap means (§5.3).
4. Layout never changes game state. Automatic switching only reads state (ADR-0005).
5. Every automatic choice can be overridden immediately and pinned; **Classic** is always one action away.
6. All settings persist; per-platform presets apply only when the player has not chosen.

## 3. Modes

**Windows developer alpha.** The launcher and pause menu offer **Large + side** (default), **Classic DS**,
**Side by side**, and **PiP overlay**. Large + side gives the touch screen twice the linear size of the top
screen in a landscape window. The secondary view has its own space, so it cannot cover the HUD. In a portrait
window this mode uses an equally sized stack. Picture-in-picture is an explicit upper-right overlay.
**Cutscene view** defaults to Single screen. The pre-title 3D opening uses a re-rendered central projection
in the largest centered 4:3 game view; unused window space stays black. It recovers the vertical interval omitted between the original off-axis views, instead of
compositing the two finished screens. The original projection and game state are restored after drawing the extra
view. This first implementation rasterizes at 256×192. It recomposes a camera view rather than preserving every
pixel of the two original screens; scaling and reframing change the presentation. A lossless claim is not verified.
The opening's game-side profile places its complete screen-space 3D UI canvas inside a centered 168×126 rectangle
at (44, 4), above the lower prompt. Only the extra cinematic image uses this placement. The 2D prompt, copyright,
touch mapping and both original DS screen images are unchanged. The engine accepts a generic placement rectangle;
it has no logo-specific positions or game IDs.
**Original DS** preserves the two-screen composition. F2 overrides the current scene. Other scene-specific rules
remain Proposed. See the [scene validation list](GAMEPLAY_INTERACTION_MATRIX.md#scenes-to-check-with-the-desktop-layouts).

### Reusing the cinematic camera

**Verified:** only the pre-title opening currently selects this camera. Its logo, prompt, overlay IDs and scene
lifecycle are not special cases inside the engine. The engine operates on projection matrices and 2D layer provenance.

To support another scene:

1. Identify its entry/exit using verified game state in `game/st/hooks/`; do not enable the camera just because both
   screens are active. Record the evidence beside the scene rule.
2. Check whether its two views are off-axis portions of the same 3D camera. Those can reuse the current recentering
   pass. Independent cameras, maps and prerecorded/2D images need a different composition strategy.
3. Verify its UI layers. The current compositor accepts text backgrounds and palette sprites, excludes bitmap BG/OBJ
   copies of captured 3D, and retains screen-space 3D UI and 2D foreground for the alternating frame pair. A scene
   using bitmap art as actual interface needs an explicit policy rather than dropping that art or treating it as 3D.
4. Validate touch mapping, fades, transitions, UI removal and the manual override. The previous foreground expires
   after one frame; this is not a persistent cache of a logo or of text.
5. Replay with the camera off and on, compare guest hashes, and review the composition at all visual checkpoints.
   Use `st_native --gx-check --cinematic-aspect 1.3333333333` with the usual local ROM/module arguments to check
   that the original polygon list is restored after the extra camera passes.
   Keep the new scene disabled until its semantics are confirmed. Widescreen reframing and higher render resolution
   remain separate features.

| Mode | Description | Parameters | Default on |
|---|---|---|---|
| **Single** | One DS screen fills the window at its aspect ratio (bars filled black, blurred edges or a pattern). The other is hidden until requested. | which screen: *follow 3D* (default) / Touch / Top | — |
| **Single + PiP** | Single, with the other screen as picture-in-picture. | anchor (9 spots + custom drag), size (10–60 % of window height), opacity (20–100 %), margin, border/background, visibility (always / auto / manual), **click behaviour** (§5.3), pin | Desktop, Steam Deck (gamescope) |
| **Horizontal split** | Side by side. | ratio (equal / main+side 60–80 %), order, vertical alignment of the smaller, gap | Ultrawide (≥ 21:9) suggestion |
| **Vertical split (stacked)** | One above the other, like a DS. | gap (0–64 px, or a "hinge" bar), scale mode (equal / main larger) | Android portrait |
| **Separate windows** | Each screen in its own OS window, independently sized, positioned, fullscreen per monitor. | which screen goes to window 2, remembered position/monitor, fullscreen per window | — |
| **Classic** | Original orientation (top above Touch Screen), equal size, integer scaling preferred. The compatibility fallback. | gap | Fallback |
| **Auto** (option on Single/PiP) | Chooses the main screen and PiP visibility from game context (§6). | per-context rules, close delay, pin | Off until context flags are verified (M5) |

Swap (exchange the two screens' places) is available in every mode. Screen rotation (90°/270°, for vertical monitors
and Android portrait "book" style) is a per-view property.

## 4. Composition

- Input: `ntr::Frame` — software buffers (256×192 ×2) or a GL array texture (layers 0/1, `256·N × 192·N`, wider with
  widescreen; `wide_layer` says which layer is widened).
- Each view samples its layer with the chosen filter: nearest, linear, sharp-bilinear (FFCC), integer scaling
  (optional per view). 2D-specific upscalers (xBRZ-class) are a later, optional step.
- Both windows present the **same frame** in the same `on_frame` call, so they cannot drift. The main window owns VSync;
  the second window presents without waiting (prevents halving the frame rate on two VSync-locked swaps; tearing on
  window 2 is to be measured).
- PiP draws last with optional border and drop shadow; a "dim and shrink while the player is touching underneath"
  option avoids hidden-touch confusion.

## 5. Input routing

### 5.1 Hit testing

`LayoutQuery::hit(window_id, x, y) → {screen, ds_x, ds_y, interactive}` walks views topmost first. Coordinates use
**pixels** (SDL3 `SDL_GetWindowSizeInPixels`, pointer positions scaled by the window's pixel density), not points.

For a view with rect `r`, rotation `θ`, widescreen extra `e` (DS pixels per side) and HUD moves `M`:

```
(u, v) = inverse_rotate((px - r.x) / r.w, (py - r.y) / r.h, θ)        // 0..1 inside the view
wx = floor(u · (256 + 2e)) - e ;  y = clamp(floor(v · 192), 0, 191)
if a HUD move m ∈ M contains (wx - m.dx, y): wx -= m.dx              // PH ph::to_ds
x = clamp(wx, 0, 255)
```

Round-trip property tests: for every mode, size and rotation, a DS pixel's view-space centre maps back to itself.

### 5.2 Stylus state rules

- A stylus press starts only inside an interactive Touch Screen view.
- While pressed, the pointer is **captured** by that view: leaving the view clamps to its edge (games read drags at the
  edge, e.g. train look-around); it never jumps to another view or window.
- Release, focus loss, window hide, layout change or controller takeover release the stylus first (no stuck touches).
- Multi-touch (Android/Deck screen): the first contact inside the Touch Screen view is the stylus; further contacts are
  ignored by the stylus (virtual controls handle their own regions).
- A layout change mid-gesture is deferred until release, except when the player forces it.

### 5.3 Interacting with the top screen

The DS cannot touch the top screen. A tap on a top-screen view therefore performs a **configured action**, never a
touch:

| Click behaviour | Effect |
|---|---|
| *Enlarge* (default) | Temporarily swaps the PiP into the main view (display-only) until the next tap/hotkey. |
| *Bring to Touch Screen* | Presses the game's own shortcut that moves the map to the Touch Screen (manual p.17: D-pad Down or B), only in contexts where `game/st/context` reports that the map is on the top screen and the shortcut is valid. Unknown context → falls back to *Enlarge*. |
| *Nothing* | Click-through disabled. |

This gives map annotation on desktop without pretending the top screen is touchable.

### 5.4 Keyboard and controller focus

Keyboard/gamepad input goes to the game regardless of which window has OS focus; the settings overlay captures input
while open. Closing window 2 hides it (WWHD behaviour) and the layout falls back to Single + PiP; it never quits the
game or leaves a pressed stylus. The main window's close button quits (with a confirmation while a save file is in
play, optional).

## 6. Context-aware layout (Auto)

Signals (`GameContext`, published per frame by `game/st/context`, each with `known`):

| Signal | Source | Status |
|---|---|---|
| `touch_has_3d` | display-swap state (generic hardware) | Verified mechanism (runtime knows engine A's layer); reading it per frame is Proposed |
| `map_on_touch_screen`, `menu_bar_open`, `collection_open` | ST UI state | Unknown (to find) |
| `train_cab`, `route_drawing`, `rail_map` | ST train state | Unknown |
| `pipes_active`, `mic_open` | ST state; `Mic_Start/Stop` (generic) | mic_open: Proposed generic; pipes: Unknown |
| `phantom_control`, `cutscene`, `dialogue`, `top_screen_hint` | ST state | Unknown |

Policy (pure function, unit-tested):
- Main view follows the 3D screen; PiP shows the other.
- PiP auto-shows for contexts where the top screen carries required information (`top_screen_hint`, train map, boss
  phases listed in the interaction matrix) and hides ~250 ms after (WWHD's delay).
- When `known` is false for a signal, the policy keeps the current layout (no guessing).
- Pin (hotkey/button) freezes the current choice; manual mode changes cancel Auto until re-enabled.
- Rate limit: at most one automatic change per 500 ms; no change while the stylus is pressed.

## 7. Platform presets (applied only to unset settings)

| Platform | Preset |
|---|---|
| Desktop 16:9/16:10 | Single (follow 3D) + PiP top-right 30 %, click = Enlarge |
| Ultrawide | Horizontal split, main + side |
| Steam Deck Gaming Mode (gamescope detected) | Single + PiP, fullscreen; second window disabled (gamescope shows one window); Deck touchscreen → Touch view |
| Steam Deck Desktop Mode / multi-monitor | Separate windows offered |
| Android landscape | Single (follow 3D) + PiP, virtual controls on the sides |
| Android portrait | Stacked (PH's portrait mode) |
| Dual-display Android devices (e.g. AYN Thor) | Separate windows via Android's second display (Presentation API; WWHD drives a second display this way): Unknown feasibility for us, M6 research |
| macOS | As desktop; separate windows across displays and native fullscreen Spaces per window (Unknown behaviour with two GL windows, M6 test) |

Layouts are computed by the pure layout engine and drawn through the present backend (ADR-0007), so every mode is
available on every graphics backend.

## 8. Widescreen interplay

The widened view applies to the 3D screen only; the other screen stays 256 wide centred (FFCC/PH). Hit testing
handles the extra pixels and HUD moves (§5.1). Widescreen stays **experimental** until ST's projection hook and
culling are verified; the default is 4:3.

## 9. Tests

- Unit (ROM-free): layout engine golden tables for every mode × window sizes × rotation × wide; hit-test round trips;
  stylus capture/release sequences; Auto policy with synthetic contexts (including unknown signals).
- Integration (ROM-free): UI smoke test drives the overlay to each mode and checks view rects (FFCC `ui_smoke` style).
- Local (ROM): scripted inputs (`--input-script`) that tap known DS pixels in each mode and compare the game's touch
  reaction; second-window close/reopen during a drag; Steam Deck Gaming Mode run.
