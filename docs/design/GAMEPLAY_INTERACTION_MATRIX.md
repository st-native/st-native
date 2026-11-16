# Gameplay interaction matrix

Status: **Proposed** (2026-11-12). Facts marked **M p.N** are from the official manual (EU English); **L** = Likely
(game knowledge, unverified in this project); **U** = Unknown. Every row needs confirmation by play-testing with the
original game (melonDS as reference) before its control mapping or context detection is implemented.

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
| C14 | Pipes duets (L) | guide/partner (L) | pipes | Yes (L) | as C13 |
| C15 | Phantom: draw route, stop, call, switch viewpoint (M p.22–23) | map w/ Phantom icon (M p.17) | 3D | Often | Single + PiP map |
| C16 | Phantom co-op puzzles (L) | map | 3D | Often | as C15 |
| C17 | Rail Map world/region, memos (M p.16) | 3D/train (L) | rail map | Yes | Map on Touch main |
| C18 | Route drawing with quill, GO!/CANCEL (M p.29) | train view (L) | rail screen | Yes | Map on Touch main + PiP train |
| C19 | Train driving: gear box, whistle, look around, switch, ROUTE (M p.28) | map (L) | cab 3D + controls | **Yes** (threats on map, L) | Single + PiP map always on |
| C20 | Train cannon (L) | map | cab 3D | Yes | as C19 |
| C21 | Cargo, passengers, stations (M p.28) | map | cab | Yes | as C19 |
| C22 | Rabbit catching (L; manual mentions rabbit stock p.14) | map | 3D | No | Single |
| C23 | Cutscenes, transitions, scenes spanning both screens (U) | varies | varies | Sometimes | Auto → Classic/stacked when `cutscene` + dual-screen flag (U) |
| C24 | Bosses (some use both screens in PH; ST U) | U | U | U | Per-boss rules after verification |
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
| C06 | Click PiP = enlarge; Tab | View/Select = toggle PiP | View button | Tap PiP |
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
