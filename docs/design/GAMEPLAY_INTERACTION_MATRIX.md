# Gameplay interaction matrix

Status: **Proposed** (2026-11-12). Facts marked **M p.N** are from the official manual (EU English); **L** = Likely
(game knowledge, unverified in this project); **U** = Unknown. Every row needs confirmation by play-testing with the
original game (melonDS as reference) before its control mapping or context detection is implemented.

**Coverage update (2026-11-18):** this document classifies requirements, not implemented support. The current
per-context evidence, PC treatment and default-promotion gates live in
[SINGLE_SCREEN_COVERAGE.md](SINGLE_SCREEN_COVERAGE.md). C23/C24 are expanded there into individual cinematics,
bosses and phases. Several old proposals below (including Tab as a map shortcut) are not current bindings:
M now peeks at the original upper screen; Tab remains fast-forward. Read the coverage register before implementing
or claiming a context is safe with the upper screen hidden.

Conventions used below:
- **Stylus fallback** is available in every context: mouse on desktop, a pointer driven by the right stick / trackpad /
  gyro on controllers and Steam Deck (trigger = stylus down), the finger on Android.
- "Follow 3D" = the main view is the screen showing the 3D engine (DUAL_SCREEN_UX §6).
- Modern controls only translate input into what the original game accepts (stylus positions, buttons, mic signal) or,
  where verified, into equivalent hook-level inputs. They never change rules, timing windows or damage.

## A. Screens, visibility and layout

| ID | Context | Top screen | Touch Screen | Must see both? | Proposed desktop layout |
|---|---|---|---|---|---|
| C01 | Title, file select, name entry, handedness | logo/info | menus, on-screen keyboard (M p.10) | No | Follow 3D/Touch main; PiP optional |
| C02 | Field/town exploration | area map, location (M p.17) | 3D gameplay | Often glance | Single (Touch main) + PiP map |
| C03 | Dungeons (incl. Tower floors) | floor map | 3D | Often; floor changes | as C02; PiP auto on floor change (U) |
| C04 | Sword combat | map | 3D | No | Single, PiP may auto-dim |
| C05 | Dialogue/signs; "look at top screen" marker (M p.18) | sometimes required info | text box | **Yes when marked** | PiP auto-show when hint flag known |
| C06 | Map viewing | map | 3D | Yes | PiP enlarge on click |
| C07 | Map annotation (MEMO/ERASE, floors) (M p.17) | 3D view (L) | map | Yes | Map on Touch main; PiP shows 3D |
| C08 | Menu bar: items, potions, SAVE, pipes, collection (M p.13) | map | menu bar over 3D | No | Single |
| C09 | Collection screen; **Swap Screens** button (M p.14) | item details (L) | collection | Yes | Follow game swap |
| C10 | Boomerang path (M p.24) | map | 3D + drawn path | No | Single |
| C11 | Whirlwind: aim then blow (M p.24) | map | 3D | No | Single + mic indicator |
| C12 | Other items: bow, bombs, whip, sand rod, bow of light (L) | map | 3D; taps/lines (L) | No | Single |
| C13 | Spirit Pipes solo songs (M p.26) | song/notes guide (L) | pipes UI | **Yes** (L) | Horizontal or stacked while active (auto when `pipes_active` known) |
| C14 | Pipes duets (L) | guide/partner (L) | pipes | Yes (L) | as C13; Gage practice reached, actual duet turns open ([survey](FLUTE_AND_FRAAZ_SURVEY.md)). |
| C15 | Phantom: draw route, stop, call, switch viewpoint (M p.22–23) | map w/ Phantom icon (M p.17) | 3D | Often | Single + PiP map |
| C16 | Phantom co-op puzzles (L) | map | 3D | Often | as C15 |
| C17 | Rail Map world/region, memos (M p.16) | 3D/train (L) | rail map | Yes | Map on Touch main |
| C18 | Route drawing with quill, GO!/CANCEL (M p.29) | train view (L) | rail screen | Yes | Map on Touch main + PiP train |
| C19 | Train driving: gear box, whistle, look around, switch, ROUTE (M p.28) | map (L) | cab 3D + controls | **Yes** (threats on map, L) | Single + PiP map always on |
| C20 | Train cannon (L) | map | cab 3D | Yes | as C19 |
| C21 | Cargo, passengers, stations (M p.28) | map | cab | Yes | as C19 |
| C22 | Rabbit catching (L; manual mentions rabbit stock p.14) | map | 3D | No | Single |
| C23 | Cutscenes, transitions, scenes spanning both screens | varies | varies | Sometimes | Opening/Cole/prologue have bounded profiles; other contexts need separate camera/UI checks (SC01–SC08). |
| C24 | Spirit Tracks bosses and their phases | map/camera/cues, depending on phase | combat/targeting | Sometimes essential | BO01–BO08: Fraaz reveal/combat reached; split-form identity and finale cues remain sourced requirements; local adaptation open ([survey](FLUTE_AND_FRAAZ_SURVEY.md)). |
| C25 | Shops, stamp stations, mini-games (L) | varies | varies | U | Single |
| C26 | Battle Mode, Contact Mode (M p.31–36) | records/arena (L) | arena | U | Out of scope until wireless decision |
| C27 | MIC TEST meter (M p.11) | — | meter | No | Single; doubles as our mic calibration screen |
| C28 | Pause, Save and Quit, Game Over (M p.10, p.23) | — | menus | No | Single |

## B. Controls per device

Bindings are defaults; every one is remappable. "Scheme" items depend on verified hooks or stylus synthesis
(CONTROLS_AND_MICROPHONE.md §4); until verified, the stylus fallback is the supported path.

| ID | Keyboard + mouse | Modern controller | Steam Deck | Android |
|---|---|---|---|---|
| C01 | Mouse; keyboard typing for name entry (enhancement, U) | Pointer; D-pad menu nav where the game supports it | Trackpad pointer / touchscreen | Touch |
| C02–C03 | WASD = movement scheme; mouse = stylus | Left stick = analog movement (scheme, PH-proven approach); A = interact (scheme); D-pad = original shortcuts | as controller; right trackpad = pointer | Touch; optional virtual stick |
| C04 | Mouse = lock-on/slash; keys for slash/lunge/spin (scheme) | B = slash toward target/facing; stick + B = lunge; spin = stick circle or Y+B (scheme) | as controller | Touch gestures |
| C05 | Click/Space advances | A advances (original A skips messages, M p.12) | A | Tap |
| C06 | Hold M = view upper screen (implemented); click enlargement Proposed | View/Select = toggle PiP (Proposed) | View button (Proposed) | Tap PiP (Proposed) |
| C07 | Mouse draws; MEMO/ERASE by click | Pointer + trigger draws; a "draw mode" toggle hides other actions | **Trackpad absolute mode** on map | Finger draws |
| C08 | Click; L/R keys take out item (original) | Pointer; L/R original | as controller | Touch |
| C09 | Click | Pointer | Pointer | Touch |
| C10 | Mouse drag, release throws | **Pointer drawing** (right stick/trackpad + trigger); optional "straight throw" assist = experimental | Trackpad drawing | Finger |
| C11 | Mouse aims; hold blow key | Aim via stick/pointer; hold blow button | Aim; blow on back button | Aim; BLOW button or real mic |
| C12 | Mouse | Pointer; scheme shortcuts only where verified | Pointer | Touch |
| C13–C14 | Mouse slides pipes; blow key or real mic | Stick/D-pad slides pipes **if** verified as equivalent to sliding; blow button | as controller | Finger + BLOW/mic |
| C15–C16 | Click icon, drag route | **Phantom command mode**: toggle, cursor, trigger draws route, button stops, button calls; D-pad keeps original switch | as controller with trackpad | Touch |
| C17–C18 | Mouse | Pointer drawing; GO!/CANCEL by pointer | Trackpad | Finger |
| C19 | W/S = gear up/down (lever synthesis); H = whistle; Q/E or mouse at edges = look; mouse for switch/ROUTE | Triggers = gear down/up; button = whistle; right stick = look around; pointer for switch arrows | as controller | Touch; optional big gear buttons |
| C20 | Mouse aims/fires | Pointer + trigger | Trackpad | Touch |
| C21–C22 | Mouse | Pointer | Pointer | Touch |
| C23 | Start skips (original, M p.12) | Start | Start | Virtual Start |
| C26 | — | — | — | — |
| C27 | Mic meter shows real/virtual level | Hold blow → meter rises | as controller | BLOW button / mic |

## C. Detection, risks and tests

| ID | Detection possibilities | Compatibility risks | Required tests (local, with ROM) |
|---|---|---|---|
| C01 | Overlay set + scene id (U) | Keyboard enhancement must use the game's own key handling | New game, name, handedness L/R, two files |
| C02–C03 | Player-control object active (PH analogue `gPlayerControl`, U for ST) | Analog scheme fighting the game's own touch-movement; camera-relative angles (patch uses a map angle function) | Walk/run thresholds, auto-jump, double-tap roll; rooms with rotated cameras (patch "Update 1" fix) |
| C04 | Attack state fields (patch offsets +0x58, U) | Lock-on target choice differs from tap-on-enemy | Each slash type vs stylus; spin dizziness preserved |
| C05 | Message marker flag (U) | Missing required top-screen info | Every "look at top screen" message seen at least once |
| C06–C07 | Map location (top/touch) flag (U); D-pad Down/B shortcut (M p.17) | "Bring to Touch Screen" pressed in wrong context | Memo write/erase, floor switch, save/reload keeps memos |
| C08–C09 | Menu bar open (U); game swap state (generic display swap) | Game swap vs our swap confusion | Swap Screens button with every layout |
| C10 | Item-in-hand id (U) | Pointer smoothing distorting the path | Same path stylus vs pointer hits same switches |
| C11 | `Mic_Start/Stop` (generic) + item id (U) | Virtual signal below threshold | Whirlwind stuns, propels, clears; real + virtual |
| C13–C14 | Pipes UI active (U); `Mic_Start` | Note-selection equivalence; timing; breath length | Every song solo + duet, real + virtual mic, 3 runs each |
| C15–C16 | Phantom-control state (ov93 per patch, U) | Path resolution vs stylus; switching shortcut | Each Phantom type's puzzles; stop/call |
| C17–C18 | Rail map/route mode (U) | Route drawn imprecisely | Draw routes across junctions; GO/CANCEL |
| C19–C21 | Train mode (U) | Gear lever is a drag gesture: synthesized drags must match lever detents | Every gear position, emergency brake, whistle, look-around, switches, cargo runs |
| C20 | Cannon state (U) | Aim latency | Train battles, bosses on rails |
| C23 | Cutscene flag (U); display-swap changes | Scenes spanning both screens split by layout | Visual comparison vs melonDS on listed scenes |
| C24 | Boss id (U) | Required simultaneous screens | Each boss in Classic and default layouts |
| C26 | Wireless init (generic) | Hangs waiting for peers | Enter/exit Battle and Contact modes without hang |
| C27 | Options screen (U) | — | Meter responds to real and virtual mic |

Open research items: complete list of microphone uses (via `--mic-log`), all scenes that require the top screen,
every item's exact gesture, train gear detents and any lid use.

## Scenes to check with the desktop layouts

| Moment | Evidence and expected presentation |
|---|---|
| Opening before the title and the illustrated new-game prologue | The existing local replays identify these separately. Use gapless Classic DS for shared framing and check the relative timing and order of both screens. The opening and new-game prologue are separate checkpoints in [the boot script](../../tests/rom/boot_new_game.txt). [Nintendo's introduction video via Nintendo Life](https://www.nintendolife.com/news/2009/11/legend_of_zelda_spirit_tracks_intro_video) |
| Aboda Village, dungeon floors and map editing | The official manual shows the area map above the touch gameplay. The game can bring the map down for annotations and floor selection. Keep it large while editing and route mouse input to the physical Touch Screen. [Nintendo manual, pp. 13, 16–17](https://www.nintendo.com/eu/media/downloads/games_8/emanuals/nintendo_ds_21/Manual_NintendoDS_TheLegendOfZeldaSpiritTracks_EN.pdf) |
| Signs and the Collection menu | The small message marker directs attention to the top screen. Collection also has its own screen-swap action. Check that both views remain readable and touch stays on the screen the game assigns to the stylus. [Nintendo manual, pp. 14, 18](https://www.nintendo.com/eu/media/downloads/games_8/emanuals/nintendo_ds_21/Manual_NintendoDS_TheLegendOfZeldaSpiritTracks_EN.pdf) |
| Alfonzo's driving exam and route planning | The rail map shows the junction and traffic while driving. Route drawing moves the map to the Touch Screen. Check both uses at windowed size and in fullscreen. [Nintendo manual, pp. 28–29](https://www.nintendo.com/eu/media/downloads/games_8/emanuals/nintendo_ds_21/Manual_NintendoDS_TheLegendOfZeldaSpiritTracks_EN.pdf) |
| Spirit Pipes lesson and Gage's duet | The upper view carries guidance and the handoff between players. Compare with equal views so timing and note cues remain legible while the pipes are operated below. [Forest Sanctuary walkthrough](https://gamefaqs.gamespot.com/ds/959019-the-legend-of-zelda-spirit-tracks/faqs/58811) |
| Fraaz and Carben's rescue | Fraaz's upper map identifies which duplicate has each element. During Carben's rescue the upper screen also needs attention while fighting below. Test whether both cues are legible in Large + side and check Side by side as an equal-size alternative. [Boss and encounter walkthrough](https://gamefaqs.gamespot.com/ds/959019-the-legend-of-zelda-spirit-tracks/faqs/58811) |
| Cragma and Skeldritch reveals | Proposed visual checkpoints. Compare both views during the boss reveal and the first attack cycle before defining a scene rule. [Recorded boss battles, 11:42 and 19:02](https://www.youtube.com/watch?v=cOsjOOpyBq0) |
| Malladus with the Bow of Light | The upper view shows Zelda's aim and the crosshair cue while commands and combat remain on the Touch Screen. Compare the two equal-size modes during this phase. [Final-battle walkthrough](https://gamefaqs.gamespot.com/ds/959019-the-legend-of-zelda-spirit-tracks/faqs/58811) |

**Verified for USA/Australia rev. 0.** In a local replay, the pre-title 3D opening used display capture while
overlays 25 and 102 were resident. The pair stopped at the title after skipping the opening. The illustrated
prologue and interactive gameplay also use overlay 102, so that overlay alone is insufficient. The opening
signature was absent after the title in samples taken every 30 frames through the 52,400-frame Castle Town route.
The frontend uses that verified opening context for the Single screen camera. Locally observed opening projections
have opposite vertical offsets, each covering a different part of the same view with an omitted interval between
them. Engine patch 0015 replays the current GX scene with the projection centered and its complete vertical extent
preserved. The launcher requests a 4:3 camera inside a centered, aspect-preserving game view. Text/palette sprites from both
physical screens are composed over that camera. Screen-space 3D UI is retained across the alternating scene pair,
while captured bitmap copies of the old 3D view are excluded. It restores the original geometry
after rendering. Other contexts keep their existing layouts until their camera semantics are verified.

**Earlier projection-only check, 2026-11-17.** A 6,000-frame unattended opening replay matches every frame's state and RAM
hash with the cinematic camera disabled, at 16:9 and at 21:9. Each enabled run reframes 5,858 frames. A separate
1,200-frame script skips to the title and also matches with all three configurations. The camera's projection
transform is checked with synthetic GX parameters at 4:3, 16:9, 21:9 and 32:9 in the C++ unit tests.

**Verified locally, overlay correction (2026-11-17).** A fresh 6,000-frame opening replay with the 4:3 camera and
composited UI matches the disabled run's RAM, clock and interpreter counts on every frame. A temporary geometry
checksum also confirmed restoration after every camera pass. Across frames 2,922–3,100, all 179 monitored 3D UI
passes contain geometry, alternating between current and one-frame-old UI scenes. Across frames 3,050–3,060,
the merged 2D foreground is present in all 11 samples although the current foreground alone alternates with an
empty frame. These are automated coverage checks, not visual approval of every game cutscene. Other split-screen
scenes remain **Unknown** until individually checked using the reuse procedure in `DUAL_SCREEN_UX.md`.

**Verified locally, reusable geometry check:** a 3,200-frame replay using `--gx-check` and the 4:3 camera matches all
3,200 guest frame hashes. All 53 periodic geometry reports record zero differences. The camera check uses the same
original-polygon-list comparison as the engine's GX replay checker; it now also checks restoration after cinematic
passes. The launcher UI smoke test and its four-layout in-window game smoke test both exit successfully. Full visual
approval of the corrected logo/prompt composition remains a maintainer review task; no game screenshots are published.

**Verified locally, title UI placement (2026-11-17).** `tests/rom/title_single_screen.txt` exercises the logo reveal,
visible prompt, touch and transition. The 4,200 guest hashes match both the earlier compositor and the disabled
camera. All five original dual-screen BMP checkpoints are byte-identical with the camera on/off. All 70 periodic
GX reports show zero geometry mismatches. At the two stable title/prompt checkpoints, rows 140–191 of the cinematic
image (prompt and footer) are byte-identical before/after placement. Synthetic compositor tests check identity,
alpha, invalid rectangles, source corners, aspect ratio and the reserved lower area. Only the opening's extra 3D UI
image is reduced/repositioned by its game-side profile; 2D text and input retain their coordinates. This is not a
pixel-lossless merge: the 3D camera is reframed and the current 256×192 output and smaller logo are resampled.
