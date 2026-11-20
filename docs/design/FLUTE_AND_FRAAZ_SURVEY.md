# Gage duet and Fraaz: presentation investigation

Date: 2026-11-19. Scope: USA/Australia revision 0, Windows software renderer, local
`ours + original` game module. This records encounter reachability and presentation requirements;
it does not enable a new automatic layout or approve either encounter for Single screen.

## Current result: Gage completed (2026-11-19)

**Verified:** calibrated virtual breath passes the initial teaching and full Gage duet in the native build with
pointer-selected notes. The game acknowledges success, shows the restored rail map, concludes the dialogue and
returns control to Link. Optional button-note controls and the PC flute composition remain unimplemented.

| Replay | Local save collection / slot | Frames | Verified scope |
|---|---|---|---|
| `tests/rom/gage_teaching.txt` | HelpTheWretched Saves03 / 2 | 10,484 | Teaching accepted, free practice with Back, return to Gage's dialogue. |
| `tests/rom/gage_duet.txt` | Saves03 / 2 | 17,241 | Teaching, free practice/Back, all three player turns, joint performance, explicit success, restored rails, final dialogue and resumed walking. |

Use CPU speed 4, fast cartridge, English and virtual breath at gain 100. The scripts contain only our recorded
inputs and require the player's own ROM/module and a local save copy. Use `--read-only-save` and put screenshots,
profiles, hash logs and other derived outputs below ignored `build/`. Source-save hashes remain unchanged.

### Microphone correction

The former quiet band-pass source reached the host callback and core ADC but failed the initial teaching stage.
The parameter and shared-core interpreter experiments below describe that retired source. The positive local
reference used the melonDS team's explicitly GPL-licensed frontend signal. Our public generator contains no
recording: it uses our deterministic bipolar noise, a 30 ms attack, an 80 ms release and gain clipping.
It feeds the existing microphone path without choosing pipes or changing game functions, hooks or protection
routines. MIC TEST, real-mic comparison, other duets and other microphone mechanics remain acceptance checks.

### Verification

- Teaching replay: all 10,484 state/RAM hashes agree off/on; 2,202,676 interpreter instructions each. All first
  8,461 frames also match the former source; the first intentional change is frame 8,463 after breath starts.
- Full duet: all 17,241 state/RAM hashes agree off/on; 3,595,601 interpreter instructions each. All 29 original
  dual-screen checkpoints and 58 foreground-plane checkpoints agree. Nine success/reward/world-return checkpoints
  also match the successful interactive recording. Both runs exit normally and preserve the source save.
- No current automatic profile is active from loaded frame 3,000 onward. The virtual source is silent from frame
  13,000 through the end. These are headless checks, not measurements of real-time audio/frame pacing.
- Windows build and all 17 C++ tests pass, including signal energy, deterministic output, clipping, release and
  diagnostic noninterference. The existing Python checks report 15 tests, one skipped, no failures.

For reproduction, use the command below with `tests/rom/gage_duet.txt --mic-mode virtual --mic-gain 100` in place
of the historical practice script. Compare off/on with the same calibrated source, ROM, module and save.

### Requirements for the PC composition

| Stage | Essential information/actions observed locally |
|---|---|
| Teaching | Upper-screen notes, actors and pipe openings; Touch Screen captions/swipe guidance; pointer and breath. |
| Free practice | Pipe alignment and the lower-left Back control, returning to Gage's dialogue. |
| Actual duet | Partner/player camera changes, note guide, all three player turns, selected notes and breath release. |
| Turn feedback | A hand-shaped graphic and musical feedback on Touch. Their exact semantics are not established; preserve them. |
| Joint performance | Both screens carry 3D views after the played turns; a separate composition decision is needed. |
| Reward/return | Rail-map animation above, scene/caption below, concluding dialogue, then normal field control. |

Inspection of the available Touch foreground shows pipe artwork as well as feedback. Overlaying that entire
plane would cover essential content; useful UI needs a verified region/layer rule. The opening's camera profile
cannot collect these controls. The app should compose explicit source regions, stacking and inverse pointer
mappings, selected by small game-specific profiles. Alpha bounds may assist drawing, never scene classification.

A separate native attempt reached a timing-failure return. Retry from that failure, interruptions, handedness,
context entry/exit and use of an adapted view remain **Unknown**. A completed Gage duet does not establish other
duets. Fraaz is still bounded to reveal/ordinary combat; split forms and fight completion remain open.
The current checklist is [PC_ALPHA_PLAN.md](PC_ALPHA_PLAN.md); no flute profile is enabled from overlay residency alone.

## Historical investigation: before breath calibration

The following records explain the original progression block and the entry-only evidence. Statements about
Gage's unconfirmed success or unresolved signal behavior below describe that earlier state, superseded above.
Fraaz's bounded scope and the proposed presentation requirements still apply.

### Initial repeatable entry points

**Verified locally:** genuine button/stylus inputs reached Gage's instrument tutorial and Fraaz's
reveal followed by ordinary combat. The two input scripts contain only our recorded actions:

| Replay | Local save collection / slot | Length | Scope and exclusions |
|---|---|---|---|
| `tests/rom/gage_practice.txt` | HelpTheWretched Saves03 / 2 | 8,781 frames | Introduction, choices, practice instruction and flute sliding/breath attempt. No successful song or completed duet. |
| `tests/rom/fraaz_boss_entry.txt` | HelpTheWretched Saves08 / 2 | 6,511 frames | Warp, upper-floor stairs, boss reveal/title and first combat HUD. No split-form phase or completed fight. |

The [checkpoint source and safe loading procedure](DUAL_SCREEN_VALIDATION.md#local-checkpoint-source)
apply. Saves, screenshots, raw profiles, hash logs and the game module remain in ignored `build/`.
The maintainer authorized private screenshot analysis for this investigation; images are excluded from Git
and publication bundles. Neither source save was edited; all interactive sessions used `--read-only-save`.

**Navigation finding:** the Fraaz save reloads outside Snow Temple, rather than at the saved boss approach.
The replay first walks off and back onto the blue warp, waits for the transition, then takes the stairs
from floor three to the boss floor. A successful save load alone had not established boss reachability.
For Gage, the initial conversation and the later explicit request to play are separate interactions;
an earlier conversation with Zelda or opening the solo flute is not evidence of entering a duet.

## Observed screen roles

**Verified, bounded to the reached Gage practice stage:** the physical upper screen carries the note
guide, actors and pipe openings. The physical Touch Screen continues the pipe artwork and carries
instructions/gesture guidance. Sliding and virtual blowing were exercised, but the test did not establish
successful musical timing or the real duet's turn transitions. Display swap is set in the instrument
view; neither screen can be selected merely by assuming that the 3D view is always on Touch.

This is not the opening's alternating captured camera pair. Reusing that camera-recentering rule would
not collect the practice instructions or provide an inverse mapping for flute input.

**Verified, bounded to Fraaz's reveal and ordinary combat:** the physical Touch Screen carries the arena,
item aiming and combat HUD; the upper screen carries the arena map and actor indicators. Exploratory
inputs also exercised elemental boomerang interactions and sword attacks. These inputs did not establish
the split-form phase. A later recorded exploration repeated the first stun/sword round and reached the
fire form, but did not complete its cycle; that longer exploratory record stays private under `build/`.
The [split-form guide][fraaz] describes a compulsory identity cue on the upper map;
its exact local presentation and timing remain **Unknown** until that phase is reached.

Both reached instrument/combat windows lack the opening's display-capture pattern. Absence of capture
does not mean that the other screen is decorative. Overlay probe matches and SDK display registers are
research observations, not a sufficient logical scene detector: entry, exit, interruption and negative
cases still need verification before they can select an automatic profile.

## Proposed reusable adaptation

The reusable part should be an app-side composition of source regions with explicit input mappings.
The game integration supplies a verified context and the required regions; the presentation backend
draws them. It must preserve the original two images and never write guest state to rearrange the UI.

- **Flute candidate:** keep notes, partner and pipe openings together; relocate instructional text into a
  reserved readable region. Define a touch band whose drag maps to the original physical Touch Screen.
  Check pipe alignment, handedness, captions, cancel, all turn changes and return to the world before
  enabling it. Cropping the long pipe body is only a proposal until its interactive roles are checked.
- **Fraaz candidate:** retain the combat canvas and show essential map/identity information automatically
  in a compact panel. Its necessity and placement must be tested in every phase, including split forms
  and arena changes. Holding M cannot substitute for this compulsory information.
- **Fallback:** unknown states retain both original views in the development default. The opening,
  narration and menu profiles must not silently match the flute or boss.

These are requirements for implementation, not newly shipped compositions. The
[campaign register](SINGLE_SCREEN_COVERAGE.md) keeps actual duet completion, split-form coverage and
interactive visual acceptance open.

## Gage follow-up: continuous pointer experiment

**Verified locally, 2026-11-19:** a fresh replay of the existing input through frame 8,461 returned to the
initial instrument tutorial. A new recorded session kept touch active between horizontal slides and sustained
virtual breath. Private checkpoints show the orange, yellow and purple pipes individually centered/illuminated
with Link's blowing animation. This confirms visible tube selection and sustained input for those three notes;
it does not establish the game's musical success/timing criteria.

The sequence still did not enter a confirmed duet or produce a song-success transition. Releasing input,
full-range sliding, a stationary blow and a lower-left return tap were also tried without establishing an exit
from this stage. The [Forest Sanctuary guide][gage] describes a later practice-return control and partner turns;
those remain sourced requirements, not locally verified interactions. Do not infer that practice must finish
automatically, or that the return control is absent from the original game.

**Next investigation:** distinguish the initial teaching stage from free practice by replaying its instruction
messages and transitions individually. Establish the intended exit/next action before changing the presentation
or attributing the failure to pointer input, microphone synthesis or the runtime. The continuous-input hypothesis
alone did not resolve progression. The complete exploratory input record and all images remain below ignored
`build/`; the source save is unchanged and the test process was stopped.

The concrete delivery checklist is [PC_ALPHA_PLAN.md](PC_ALPHA_PLAN.md), P1. Optional button-note controls are
also retained in ENHANCEMENTS.md; ADR-0006 now records the later maintainer request instead of retaining the
initial blanket rejection. No button-note implementation or new automatic flute profile is enabled here.

## Reproduction

### Teaching-stage and microphone diagnostics follow-up

**Verified locally, 2026-11-19:** individual instruction checkpoints distinguish the initial teaching stage
(tube alignment, then microphone use) from the later free-practice/duet described by the guide. A separate
pointer-only negative control also showed the playing pose, so that pose and highlighted tubes must not be
used as proof of accepted microphone input or musical success.

The app now supports [numeric microphone profiling](CONTROLS_AND_MICROPHONE.md#66-numeric-microphone-diagnostics).
Windows build and all 17 C++ tests pass, including checks that diagnostic snapshots preserve the signal and
release envelope. The profiled 8,781-frame Gage replay matches every baseline state/RAM hash, all interpreter
counts and all 8 original-screen checkpoints; its source save is unchanged.

That replay delivered 6,961,152 samples in 6,798 callback reads. The requested blow spans completed-frame rows
8,462–8,601; nonzero host samples span 8,462–8,607, with peak 19,815. The later silence is confirmed numerically.
An individually stepped short-puff test also delivered nonzero samples followed by silence, without establishing
the teaching-stage exit. These establish callback delivery/release, not the game's ADC/decoder acceptance.
No new flute profile is enabled and no game function or protection routine is changed.

The current diagnostic question is whether progression depends on input timing, gain, CPU speed-up or a runtime
compatibility issue. Parameter experiments must record their settings and actual reached state; a different
speed may invalidate an existing input timeline. A reference comparison is needed before assigning a cause.

**Bounded parameter results:** gain 200 at CPU speed 4 still did not establish the teaching-stage exit after a
short centered orange-note attempt. CPU speed 1 required additional navigation/dialogue timing to reach the same
instruction stage; a centered note attempt there also did not establish its exit. These are failed progression
attempts, not proof that gain or clock behavior can never affect the mechanic. All source saves are unchanged.

**Local reference experiment:** a private harness booted the same dump/save with an empty recompiled-function
table and called melonDS `RunFrame` for both ARM interpreters, using clock multiplier 1 and normal cartridge
delays. It reused the app's input/control and microphone source. Its recorded route reached the same instruction
stage. A centered single-note attempt and a short three-note phrase still did not establish progression.
This comparison shares the pinned/patched core, boot plumbing and signal generator; it is not an independent
stock-emulator or hardware validation, and cannot by itself identify or exclude a shared-component fault.
The harness, raw records and all game-derived outputs remain private under ignored locations.

### Replay commands

Create a new output directory below `build/`. With your own local ROM, module and copied save:

```powershell
.tools\upstream\ffcc\build\release\st_native.exe "<your-ROM.nds>" `
  --module "<your-local-st_game.dll>" --save "<local-Saves03-copy.sav>" --read-only-save `
  --headless --script tests\rom\gage_practice.txt --screen-features 0 `
  --hashes build\flute-check\off.hashes --wireless-group flute-check-off
```

Repeat with `--screen-features 7 --cinematic-aspect 1.3333333333`, a distinct hashes file and
wireless group. Compare using `python -I tools/build/compare_runs.py <off.hashes> <on.hashes>`.
For Fraaz, substitute `fraaz_boss_entry.txt` and the Saves08 copy. Optional `--scene-profile <csv>
--scene-stride 1` records metadata; optional `--shots <directory>` writes private checkpoints.
Hash equality checks guest-state isolation; it does not verify readability or real-time audio/frame pacing.

**Verified locally:** the off/on comparisons passed all 8,781 Gage frames and all 6,511 Fraaz frames,
including state, RAM and interpreter totals. The 8 Gage and 7 Fraaz original-screen checkpoint pairs
are byte-identical. Both source saves remain unchanged. No existing automatic profile or display capture
is active after loading (frame 3,000 onward). These results protect the original fallback while the new
compositions are designed; they do not establish that manual Single screen preserves the missing cues.

Python verification reports 15 tests, one skipped, no failures. The synthetic GL framebuffer check also
passes opacity, source-plane orientation and aspect-bar checks. These are independent of encounter acceptance.

[fraaz]: https://www.thonky.com/zelda-spirit-tracks/fraaz
[gage]: https://www.thonky.com/zelda-spirit-tracks/gage-forest-sanctuary
