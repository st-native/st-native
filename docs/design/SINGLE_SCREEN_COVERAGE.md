# Single-screen campaign coverage

Updated: 2026-11-19. Scope: supported single-player content, USA/Australia revision 0.
Status: **Proposed acceptance register; incomplete.** This is the current register for PC presentation coverage.
The interaction matrix supplies control requirements; the scene survey supplies measured camera evidence. Neither
a catalogue entry, a successful save boot nor a passing guest hash proves that an encounter is adapted.

## Product target

One PC game canvas, with the information and interactions of both DS screens preserved. This can mean a re-rendered
camera, captions, a map panel, a status indicator or a deliberately framed secondary camera. It does not mean always
displaying the physical Touch Screen, or asking the player to hold M to see a compulsory combat/duet cue.

**During development:** unknown contexts retain both original views in the default Large + side layout. Manual
Single screen remains experimental; selecting it can hide information. Its optional secondary overlay is the whole
original upper screen, not a native map/HUD extraction. M and F2 are recovery tools, not evidence of finished PC UX.
Existing automatic opening/Cole/prologue/save-menu profiles remain limited to their verified conditions.

**Proposed map-discovery hint (maintainer suggestion, 2026-11-18):** a small, brief map icon with the current
binding can invite an optional map consultation at a verified relevant moment. Suppress it when the map is already
visible; use a cooldown and an accessibility setting rather than repeating it continuously. Context and key glyph
must come from verified state and the active binding, not a hardcoded instruction. Unknown contexts do not trigger
the hint. This is recorded for later work, not implemented here. Required combat, duet or navigation information
must still appear automatically; the hint must not replace those adaptations.

**Before promotion to the default:**

1. Identify every supported campaign chapter and optional activity reached in a complete playthrough. Record new
   variants here; this register is a candidate list, not a claim to enumerate every cutscene already.
2. For each entry, record each screen's role and whether information is simultaneous, glanceable, editable or
   decorative. A black background, missing display capture or repeated pixels cannot establish that role.
3. Verify entry, exit and interruptions using game state / graphics metadata. Record address provenance if guest
   fields are needed. No OCR or image heuristics in the automatic policy.
4. Implement the PC composition and inverse input mapping. Preserve text, timing, fades, prompts, targets and
   essential off-screen information. Display swapping must not redirect the stylus to the physical upper screen.
5. Replay off/on: guest hashes must match for presentation-only changes. Check geometry restoration, both language
   lengths and handedness variants, transitions, restart/death/load, M/F2, Esc/F1 and pointer capture.
6. Review the actual presentation and complete every relevant encounter phase. Correctness counters alone cannot
   approve framing, readability or missing cues. Measure real-time audio/frame pacing separately.
7. Complete a supported single-player playthrough using the automatic PC presentation, without needing the original
   layout or M to recover compulsory information. Unknown mandatory contexts block default promotion. Optional
   content must be explicitly covered or declared unsupported; it cannot silently inherit an unrelated rule.

Native widescreen, resolution and button-only flute controls are separate enhancements. They must not be prerequisites
for a correct 4:3 PC presentation. Existing original-view settings remain available after promotion.

## Evidence vocabulary

| Label | Meaning |
|---|---|
| Source | Requirement described by the manual or a walkthrough; local verification is still needed. |
| Boot | Read-only save bootstrap ran; target encounter/phase is not confirmed. |
| Reached | Recorded input and local evidence identify the actual interaction and phase. |
| Implemented | A bounded profile/composition exists; acceptance may still be open. |
| Preview | Research rendering only; not approved for interactive use. |
| Accepted | Entry/exit, all relevant phases, input and human visual checks passed, with linked evidence. |
| Unknown | Not established. Never translate this to "upper screen unnecessary". |

No entry is newly marked Accepted by this consolidation. Existing measurements are preserved in
[DUAL_SCREEN_VALIDATION.md](DUAL_SCREEN_VALIDATION.md), [NARRATIVE_PRESENTATION.md](NARRATIVE_PRESENTATION.md) and
[DUAL_SCREEN_UX.md](DUAL_SCREEN_UX.md). Generic platform tests do not automatically accept game contexts.

## Menus, information and ordinary gameplay

The requirements below use the [official manual][manual] and the existing interaction matrix. PC treatments are
**Proposed**, except the explicitly implemented menu profile. "Check" means the information role is still Unknown.

| ID | Context | Information to preserve | Candidate PC treatment | Current evidence / next check |
|---|---|---|---|---|
| UI01 | File selection / name / handedness / Adventure selection | File and selection controls | Touch menu canvas | Implemented profile; existing blank/existing-save replays. Do not extend to arbitrary menus. |
| UI02 | Collection and its screen-swap control | Inventory details and selection | Unified inventory/details panel | Source; verify both swap directions and all categories. |
| UI03 | Item bar / equipment / potions | Selection and gameplay | Overlay on main view | Source; verify item changes, cancel and handedness. |
| UI04 | Local map, floors and memo/erase | Map editing and navigation | Expandable interactive map | Source; test floor selection, pen coordinates, erase and persistence. |
| UI05 | World / region rail maps | Destination and route context | Map page with clear return | Source; verify physical display assignment. |
| UI06 | Train customization | Part preview and selection | Preview next to controls | Source, manual p.30; not a decorative-screen case. |
| UI07 | Shops / trading / prize postcards | Check item preview, price and choice | Unified shop panel | Unknown; enumerate interfaces rather than reuse UI01. |
| UI08 | Stamp book / treasure / rabbits / songs pages | Check details and paging | Unified collection pages | Source; each page must be checked. |
| UI09 | Save / quit / game over / retries | Check prompts and file state | One menu canvas | Source; interruption and load tests open. |
| GP01 | Aboda / towns / interiors / ordinary field | Area map and player location | Main world with optional map | Source; semantic exploration detector remains Unknown. |
| GP02 | Dungeon navigation and floor transitions | Floor map and annotations | Contextual map panel | Source; must not lose puzzle notes while moving. |
| GP03 | Dialogue/sign with upper-screen attention cue | Referenced information | Compose referenced content with dialogue | Source, manual p.18; must find and test the actual hint flag. |
| GP04 | Standard Phantom control | Route, remote position and focus | Command panel / contextual map | Source; verify far-apart Link/Phantom and viewpoint switching. |
| GP05 | Torch / Warp / Wrecker Phantom variants | Check variant-specific cues | Reuse command shell after per-variant checks | Unknown; every variant remains an acceptance subcase. |
| GP06 | Dark Tower rooms and cooperative puzzles | Check visibility, remote actor and map cues | Contextual map/status | Source/Unknown; ordinary exploration alone cannot cover these. |
| GP07 | Whirlwind / boomerang / bow / bombs / whip / sand rod | Check targeting, paths and item UI | Main world with item-specific overlays | Source; all items and screen transitions need local checks. |
| GP08 | Puzzle clues, map tracing, keys and note-dependent routes | Referenced clue and editable notes | Clue/map overlay without obscuring play | Source; test representative puzzles in each temple. |

## Cinematics and presentation

| ID | Moment | Candidate treatment | Current evidence / remaining work |
|---|---|---|---|
| SC01 | Pre-title 3D opening and title UI | Central 4:3 camera + composed UI | Implemented, existing replay/GX checks and maintainer feedback; retain bounded entry/exit. |
| SC02 | Illustrated prologue | Narration over art | Implemented; must release at dialogue/gameplay. |
| SC03 | Cole reveal | Reuse symmetric camera with identity UI | Implemented; human review of cuts/fades/UI still open. |
| SC04 | Tower after Byrne | Asymmetric camera union + adapted input | Preview only; touch mapping, continuity and UI unresolved. |
| SC05 | Graduation / castle escape / early story transitions | Check art, dialogue and camera roles | Unknown beyond current early-game samples. |
| SC06 | Temple completion / rail restoration / Tower restoration | Curated camera or art/dialogue composition | Unknown; record each distinct sequence, not one assumed universal rule. |
| SC07 | Item rewards / major story reveals / final approach | Check prompts and independent views | Unknown; discover exact scene boundaries in campaign sweep. |
| SC08 | Ending / credits / post-clear return | Check both original screens | Unknown; default cannot be declared campaign-complete before this. |

## Flute and duet cases

The manual establishes flute input. [Gage's lesson][walkthrough] and the [final duet][finale] describe essential
guidance/turn information. Reusing one duet shell is Proposed; acceptance is per song/partner, not per interface.
Button-note controls are a separate requirement and must preserve these cues too.

| ID | Interaction | PC treatment required | Local checkpoint / status |
|---|---|---|---|
| FL01 | Initial lesson / solo songs | Notes and instrument together | Solo instrument opened from Saves03/2 during exploration; initial lesson and learned-song completion remain Unknown. |
| FL02 | Gage | Notes + partner/turn cue | Saves03/2: Reached teaching, free practice/Back, all three duet turns, joint performance, restored rails and resumed walking (`gage_duet.txt`, 17,241 frames). Timing failure observed separately; retry and PC composition/input acceptance remain open. [Survey](FLUTE_AND_FRAAZ_SURVEY.md). |
| FL03 | Steem | Same shell, separately verified timing | Saves07/2: Boot. |
| FL04 | Carben | Same shell, separately verified timing | Saves11/2: Boot. |
| FL05 | Embrose | Same shell, separately verified timing | Saves14/2: Boot. |
| FL06 | Rael | Same shell, separately verified timing | Saves18/2: Boot. |
| FL07 | Final duet with Zelda | Instrument + final-sequence cues | Saves23/2: Boot; duet phase not reached by the bootstrap. |

## Trains and encounters

| ID | Interaction | Essential information / candidate treatment | Local evidence / next check |
|---|---|---|---|
| TR01 | First driving exam | Cab controls + route context | Existing Castle Town route reached; automatic PC adaptation not implemented. |
| TR02 | Route drawing / GO / cancel | Interactive route plus travel context | Source, manual p.29; verify edit/return and no stylus jumps. |
| TR03 | Ordinary travel / junctions / traffic | Readable radar and route indicators | Source; radar must appear automatically where compulsory. |
| TR04 | Passengers / cargo | Relevant gauges and travel cues | Source; include deliveries and station transitions. |
| TR05 | Cannon / tunnel Rocktite encounters | Check camera and threat direction | Source [boss guide][bosses]; test both tunnel encounters. |
| TR06 | Carben rescue / pirate boarding / Big Blin | Check escort status and attacks | Saves11/1: Boot; rescue encounter not established. |
| TR07 | Dark Realm armored trains | Threats and junctions | Saves22/2 candidate; Unknown. |
| TR08 | Demon Train | Junction prediction and cannon view | Source [finale][finale]; Saves23/1 Boot; phase coverage open. |

## Boss cases: introductory camera and combat are separate checks

The [boss guide][bosses] establishes encounter names/phases. The screen roles below are checked separately using
individual guides where available. A 3D capture pair is neither necessary nor sufficient for dual-screen gameplay.

| ID | Boss / phase | Candidate PC adaptation | Local checkpoint / evidence |
|---|---|---|---|
| BO01 | Stagnox: ground, flight and recovery | World framing plus airborne threat cues; verify role of upper view | Saves04/2: Boot. [Phase guide][stagnox]. |
| BO02 | Fraaz: large form, split forms, arena changes | Element identity cues visible with boomerang targeting | Saves08/2: Reached reveal and ordinary combat, `fraaz_boss_entry.txt`; split phase still Unknown locally. Upper map identity requirement [guide][fraaz]; not adapted. [Survey](FLUTE_AND_FRAAZ_SURVEY.md). |
| BO03 | Phytops: ascent and arena | Preserve climb/target view and whip input | Saves12/2: Boot. [Phase guide][phytops]; camera roles Unknown locally. |
| BO04 | Cragma: reveal, platform, mine cart and eye | Check vertical framing / aiming | Saves16/1: approach replay only; encounter not established. |
| BO05 | Byrne: opening, remote gauntlet and co-op phases | Link/Phantom information together | Saves17/2: Boot. [Phase guide][byrne]; not accepted. |
| BO06 | Skeldritch: spine, armor and skull | Check full-height framing, catapult and targeting UI | Saves20/1: approach replay only; encounter not established. |
| BO07 | Final train rooftop / Cole / possessed Zelda | Keep remote ally and targeting information visible | Saves23/1 candidate; exact phase not reached. |
| BO08 | Malladus: protect Zelda, aim cue, final input | Preserve partner aim cue with player combat | Saves23/2: Boot. [Finale guide][finale]; split into phase replays before implementation. |

## Optional single-player content and boundaries

| ID | Context | Required investigation |
|---|---|---|
| EX01 | Rabbit catching | Camera transition, time limit, aim and return to train. |
| EX02 | Archery / shooting minigames | Targets, score, timer and retries. |
| EX03 | Whip race / traversal challenges | Vertical views, targets, timing and failure. |
| EX04 | Take 'Em All On / boss rematches | Boss cases, Dark Link, inter-round UI and retries; repeat phases in rematch context. |
| EX05 | Stamp/postcard/treasure and delivery side quests | Relevant UI cases plus route/map/dialogue variants. |
| EX06 | Battle / Tag / wireless menus | Explicitly unsupported until the multiplayer decision; do not claim PC coverage. |
| EX07 | Castle sword-training minigame | Check score/hit counter, damage, timing and retries. |

Optional activities are candidate categories, not an exhaustive verified subquest list. Their names and ordering
are cross-checked against the [campaign index][index], [minigame catalogue][minigames],
[first-hand walkthrough][sideguide] and the existing interaction matrix, with local confirmation open.

## Checkpoint bootstrap: 2026-11-18

**Verified:** 12 additional save/slot pairs ran 3,000 frames each using the local original-checks module and
read-only saves: FL02-FL06, BO01-BO03, BO05, TR06, TR08 and BO08. All runs exited 0; every source save remained
byte-identical. No movement, attacks, dialogue advancement or success inference was included in this bootstrap.
These are starting points for navigation/phase scripts, not twelve completed or visually reviewed encounters.

Across frames 2,000-3,000, these cases did not match an existing cinematic/menu/narration profile. No capture or
off-axis projection was recorded in that window. TR08 and BO08 had display swap set in every sample, unlike the
other ten cases. **Inference:** a fixed physical-screen choice cannot be assumed valid throughout the campaign.
This does not identify their complete camera semantics, or justify a new profile from the swap bit alone.

All raw graphics CSV, hash logs, saves and any later visual checkpoints stay under ignored `build/`. Use the
read-only bootstrap tool described in [DUAL_SCREEN_VALIDATION.md](DUAL_SCREEN_VALIDATION.md); its reports never
promote Boot to Reached or Accepted. Save provenance remains the collection linked in that document.

## Work order

The concrete delivery checklists and current task are in [PC_ALPHA_PLAN.md](PC_ALPHA_PLAN.md).
Gage is first, Fraaz second; shared components are built with those cases before expanding to train/partner
gameplay and a declared-scope developer alpha. The campaign sweep and default-promotion gates remain required.

1. Extend the reached FL02 practice and BO02 boss-entry replays to the actual duet/turn changes and split forms,
   with original dual views, before trying another generic camera join. FL02 tests turn/notes; BO02 tests
   information absent from the main world view. Entry evidence: [bounded survey](FLUTE_AND_FRAAZ_SURVEY.md).
2. Validate TR02/TR03 radar/route semantics, GP03 upper-screen hint, UI02/UI04/UI06 swapping and editable panels.
3. Finish SC04 inverse touch mapping; reach BO04/BO06 introductions rather than repeating unconfirmed approaches.
4. Cover the remaining duet, boss and final-sequence phases, then sweep optional activities and discover missing
   cinematics through a complete playthrough. Record gaps explicitly.
5. Promote an automatic PC default only after the gates above pass. Keep each adaptation independently switchable.

[manual]: https://www.nintendo.com/eu/media/downloads/games_8/emanuals/nintendo_ds_21/Manual_NintendoDS_TheLegendOfZeldaSpiritTracks_EN.pdf
[walkthrough]: https://www.gamespot.com/articles/legend-of-zelda-spirit-tracks-walkthrough/1100-6242561/
[index]: https://www.thonky.com/zelda-spirit-tracks/
[bosses]: https://www.zeldaxtreme.com/spirit-tracks/bosses/
[stagnox]: https://www.thonky.com/zelda-spirit-tracks/stagnox
[fraaz]: https://www.thonky.com/zelda-spirit-tracks/fraaz
[phytops]: https://www.thonky.com/zelda-spirit-tracks/phytops
[byrne]: https://www.thonky.com/zelda-spirit-tracks/byrne-boss-fight
[finale]: https://www.thonky.com/zelda-spirit-tracks/malladus-final-boss
[minigames]: https://zelda.mtwildwood.net/spirit-tracks/minigames/
[sideguide]: https://gamefaqs.gamespot.com/ds/959019-the-legend-of-zelda-spirit-tracks/faqs/58497
