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
**Presentation** defaults to **Automatic (PC)**. The narrated prologue composes its top-screen text over the original
Touch Screen art, without PiP; see [Narrative presentation](NARRATIVE_PRESENTATION.md). Unverified gameplay contexts
keep the chosen two-screen layout. The pre-title 3D opening uses a re-rendered central projection
in the largest centered 4:3 game view; unused window space stays black. It recovers the vertical interval omitted between the original off-axis views, instead of
compositing the two finished screens. The original projection and game state are restored after drawing the extra
view. It renders at the selected internal resolution, including with PC Game Menus enabled. It recomposes a
camera view rather than preserving every
pixel of the two original screens; scaling and reframing change the presentation. A lossless claim is not verified.
The opening's game-side profile places its complete screen-space 3D UI canvas inside a centered 168×126 rectangle
at (44, 4), above the lower prompt. Only the extra cinematic image uses this placement. The 2D prompt, copyright,
touch mapping and both original DS screen images keep their placement. The engine accepts a generic placement rectangle;
it has no logo-specific positions or game IDs.
**Original DS** preserves the two-screen composition. F2 overrides the current scene. The Cole profile uses the
same symmetric camera with full-size UI; other interactive scene rules remain Proposed. See the
[scene validation list](GAMEPLAY_INTERACTION_MATRIX.md#scenes-to-check-with-the-desktop-layouts).

### Independent screen features (2026-11-18)

**Implemented:** the launcher and Esc/F1 pause menu have a **Screen features** tab with three independent switches:

| Control | Effect when enabled | Effect when disabled |
|---|---|---|
| Unified cutscenes | Extra camera for the verified opening/Cole intervals. | Both original screens use the selected layout. |
| Subtitles over artwork | Prologue artwork and original narration composed together. | Original art/text screens use the selected layout. |
| Hide unused screens | Original Touch Screen alone in verified file-selection, file setup and Adventure/mode menus. | The decorative secondary screen remains in the selected layout, including PiP. |

All three default to On. **Presentation → Original DS** suspends automatic presentation and disables these controls
without forgetting their saved choices. F2 remains a temporary original-view override. Each switch applies on resume
and is saved for the next launch. Changing one does not change the other two.

The selected two-screen layout remains the fallback for gameplay and unknown contexts. Hiding is based on a verified
menu state, not screen brightness or an assumption that a map is useless. No generic blank-screen detector is used.
The menu rule requires overlay probes 19/101, no capture/swap, engine A display control `0x211c10` or `0x211f10`, and
engine B `0x211f30` (USA/Australia revision 0, blank/existing-save replays). It releases when those states leave.
The extra image is the unmodified original Touch Screen, so all buttons retain identity touch coordinates. Battle,
Tag Mode and later in-game menus have not been individually validated; this is not a claim to hide every unneeded
screen throughout the game.

The application passes a single options snapshot from the window thread to the producer through an atomic mask.
On resume it publishes the new options before waking the producer and discards any old queued presentation frame.
The game/context reader does not own user settings; the engine is unchanged. Developer replays use
`--screen-features <mask>`: cutscenes=1, subtitles=2, hide-secondary=4, default=7. `--scene-profile` logs the mask and
detected scene separately from whether its feature was selected.

Preferences retain the previous quoted ROM path, scale, language, window, layout and master-mode fields. A versioned
`screen-enhancements-v1` suffix adds the three switches. Older files keep their existing master mode/layout; invalid
or unknown suffixes use default switches. ROM-free tests cover all eight combinations, save/reload/replacement,
legacy formats, malformed extensions and RmlUi change events in both menus.

**Verified locally:** Windows build and all 16 C++/UI tests pass. The launcher smoke test exercises all eight
combinations through launcher and pause controls, checks master-mode disabling and synchronization, and verifies
wide/compact/scrollable layouts. Python reports 12 tests, one skipped, no failures.

The all-off/all-on replays match every state/RAM hash across 17,400 frames, with 15/15 original screen checkpoints
unchanged and 6,657 geometry checks reporting zero differences. Three 6,200-frame runs enabling only cutscenes,
only subtitles or only hiding also match the reference hashes and select only their own contexts. Menu-only views
are byte-identical to the original Touch Screen at the handedness/setup/mode checkpoints.

An existing-save replay compares 3,000 frames off/on: all hashes and 6/6 original-screen checkpoints agree, and the
gameplay map remains in the fallback layout after loading. A real-window PiP-origin menu-only smoke test exits
successfully, displaying the single menu canvas, opening/closing Esc/F1 settings, switching to original screens for
30 presented frames with F2, and returning to the menu canvas. These concurrent correctness tests are not frame
pacing benchmarks. Other platforms and remote CI were not run for this change.

### Single-screen foundation (2026-11-18)

**Implemented:** `Screens → Screen layout → Single screen` fits the original Touch Screen into the largest centered
4:3 rectangle. It preserves aspect ratio and does not crop/stretch to the monitor. The verified cinematic,
narration and save-menu policies still take priority when their individual enhancements are enabled.

`Map` in both the launcher and Esc/F1 menu offers **Hidden**, **Small (20%)**, **Medium (30%)**, and opacity
**100% / 75% / 50%**. Percentages measure the secondary view's width relative to the main 4:3 view. These settings
apply only to Single screen and persist as a validated `secondary-view-v1` preferences extension; old preferences
retain their selected layout. The secondary image is the whole original upper screen, not an extracted or
semantically identified minimap. It may contain other information outside normal exploration.

Hold **M** to inspect the original upper screen at full 4:3 size in any layout, including a unified scene; release
to restore the prior presentation. This is display-only: no stylus events go to it. Opening Esc/F1, losing focus,
changing layout, or pressing F2 clears the temporary view. M is not intercepted during native name entry; Tab keeps
its existing fast-forward function. A manual layout switch or peek releases any active stylus first. An existing
drag may cross a PiP rectangle, but a new press there never reaches the Touch Screen, even at 50% opacity.

The backend draws the three-frame atlas through a small textured-quad compositor. It blends the secondary view
over the main image; layout/settings/UI contain no GL calls. Synthetic captures exercise opacity, correct atlas
planes, vertical orientation, letterboxing, and touch routing. Run `tests/check_screen_composition.py` on the
ROM-free launcher smoke output to verify actual framebuffer colors without extra Python dependencies.

**Proposed, not enabled:** automatic secondary-screen visibility for train radar, Spirit Pipes duets, boss phases
and unverified exploration states. The existing default/fallback layout remains Large + side; Single screen is an
explicit choice until these contextual rules are verified. There is no blank-screen detector or assumption that
every upper screen is unnecessary. This step does not claim campaign-wide automatic PC presentation.

**Verified locally (Windows x64):** 17 C++ tests and 12 Python tests pass. The ROM-free launcher smoke checks both
menus, scrolling and all size/opacity choices; framebuffer checks pass for actual 50/75/100% blending, separate
upper/touch/cinematic planes, asymmetric orientation markers and aspect bars. The existing early-gameplay script
was replayed with presentation features off/on: all 31,000 state/RAM hashes match, with the same 3,997,080 fallback
interpreter instructions. This is guest-state regression coverage, not a visual claim for every later scene.
A real-window Single-screen boot smoke passes with M press/release, F2 and both pause-menu shortcuts. No player
save is written by these tests. Other platforms and remote CI have not been run for this step.

### Reusing the cinematic camera

The [local scene survey](DUAL_SCREEN_VALIDATION.md) now includes community checkpoints for Cole and the Tower.
It confirms different projection patterns. Cole is enabled for its verified pair; the Tower is a headless preview.
Boss approaches are recorded separately from verified boss introductions.

**Implemented:** the pre-title opening and Cole's verified opposed-frustum interval select this camera. Only the
opening profile shrinks the title UI; Cole keeps identity UI placement. The pure scene policy is separate from the
runtime adapter and has ROM-free lifecycle tests. The engine operates on projection matrices and 2D layer provenance.
A Tower asymmetric-span preview is restricted to the headless CLI until its touch coordinates and UI are adapted.

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

**Verified on Windows.** The launcher's **Graphics** tab and the pause menu (`Esc` or `F1`)
offer the same internal resolutions, from **256x192 (Native)** through **3072x2304** per DS
screen. In the pause menu, the new resolution takes effect when the game resumes. Window size
and screen placement are controlled separately under **Window** and **Screens**.

The verified file menus have a decorative upper screen. When automatic hiding is enabled, the
touch screen fills the view even if a renderer change leaves the frame without a composed
texture. F2 still toggles the stacked DS view in those menus.
