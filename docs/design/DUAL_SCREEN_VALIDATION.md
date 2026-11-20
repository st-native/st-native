# Continuous dual-screen scene survey

Date: 2026-11-17. Platform: Windows x64, USA/Australia revision 0, local static-recompiled module with
`--functions ours --dsprot original`. This is a rendering survey, not a full playthrough or visual acceptance test.
The automatic cinematic camera selects the pre-title opening and Cole's verified opposed-frustum interval.
The asymmetric Tower camera is a **headless research preview**, not an interactive mode.

## Findings

| Session | Evidence | Status of a joined view |
|---|---|---|
| Pre-title opening | Display capture and alternating positive/negative vertical projection offsets. Positive control for the survey. | **Verified previously:** extra central 4:3 rendering and opening-specific UI placement. This survey does not broaden that approval. |
| New-game prologue, Niko's house, Aboda and first train ride to Castle Town | No additional off-axis projection/capture pair observed after the opening in the sampled 52,400-frame route. The prologue has a mostly black top screen in the local captures. | **Verified for sampled frames:** these are not another instance of the opening's opposed projection pair. Sampling is not proof that every short transition was covered. |
| Cole reveal checkpoint | Dense 12,000-frame replay; capture active on frames 6,596–9,712, 3,117 frames. Of these, 1,558 have a positive projection offset and 1,559 a negative one, alternating with screen swap. | **Implemented:** shared camera with identity UI placement. Fresh opposed pair required on entry; verified lens changes retain the profile. Human visual acceptance remains pending. |
| Tower after Byrne, including a northward approach | Dense 12,000-frame replay; after frame 1,800, capture on 10,111 sampled frames. Alternates a positive-offset perspective (5,056 frames) with a centered perspective (5,055 frames). | **Implemented for headless research:** explicit frustum union; centered perspective retained as world geometry. Interactive joining stays disabled pending inverse touch mapping and UI validation. |
| Fire Temple before Cragma | Save loads; 12,000-frame approach replay has no capture/off-axis projection after frame 1,800. | Boss introduction **not established as reached**. This does not validate the Cragma encounter. |
| Sand Temple before Skeldritch | Save loads; 12,000-frame approach replay has no capture/off-axis projection after frame 1,800. | Boss introduction **not established as reached**. This does not validate the Skeldritch encounter. |

The dense Cole replay contains 12 additional off-axis frames outside the capture interval. A projection count alone
must not activate a scene rule. Overlay probe matches also establish byte residency, not necessarily the game's
currently active logical scene; combine verified lifecycle evidence with the display state.

An additional dense diagnostic window observes equal focal scales on Cole's alternating pair and opposite offsets.
The Tower pair instead has equal focal scales with one zero and one positive offset. These observations establish
the projection difference; they do not by themselves prove matching world-space cameras or a seamless image.

## Local checkpoint source

**Verified:** downloaded HelpTheWretched's
[Start-to-Finish Zelda saves for DS/3DS/Wii](https://gbatemp.net/download/start-to-finish-zelda-saves-for-ds-3ds-wii.37279/),
and extracted only the Spirit Tracks subset locally. The catalogue describes 23 save files with two slots each,
covering 46 checkpoints. **The initial cinematic survey loaded only the four files above.** The later read-only
bootstrap below adds starting points; it does not establish that their encounters were reached or adapted.
The story labels come from the author's catalogue. No save, archive, catalogue text, screenshot or game-derived log
is included in this repository. Local source URLs and archive digests are recorded under ignored `build/`.

| Our input script | Required file in that collection | Slot |
|---|---|---|
| `tests/rom/cinematic_cole.txt` | Saves01 | 2 |
| `tests/rom/cinematic_tower_after_byrne.txt` | Saves18 | 1 |
| `tests/rom/cinematic_cragma_approach.txt` | Saves16 | 1 |
| `tests/rom/cinematic_skeldritch_approach.txt` | Saves20 | 1 |
| `tests/rom/gage_practice.txt` | Saves03 | 2 |
| `tests/rom/gage_teaching.txt` | Saves03 | 2 |
| `tests/rom/gage_duet.txt` | Saves03 | 2 |
| `tests/rom/fraaz_boss_entry.txt` | Saves08 | 2 |

These scripts contain only our timed button/stylus inputs. Copy the required save to an ignored local directory;
always use `--read-only-save`. Do not combine it with `--no-save`, which starts from blank memory. Do not overwrite
a player's normal save. The two approach scripts need further navigation before they can test boss introductions.

## Reproducing a survey

After building the program and a local game module, run, substituting your own ROM and module paths:

```powershell
.tools\upstream\ffcc\build\release\st_native.exe "<your-ROM.nds>" `
  --module "<your-local-st_game.dll>" `
  --save build\scene-survey\cole.sav --read-only-save --headless `
  --script tests\rom\cinematic_cole.txt `
  --scene-profile build\scene-survey\cole.csv --scene-stride 1 `
  --wireless-group screen-survey-cole
```

Create the output directory first. The default stride is 30: sample adjacent pairs at frames 30n and 30n+1. Use
stride 1 for a dense trace. Rows include the app frame, capture/swap flags, latest geometry frame, aggregate
projection-load counts, display/BG registers, the effective projection at the first perspective primitive, a
model-view signature, the current automatic join decision, and overlay residency probe matches. Protected overlay
1 is deliberately excluded. This inspects graphics metadata, not protected code, pixels or instructions.

Projection counts describe full matrix loads following an explicit matrix-mode command in that stream, not every
effective matrix operation or what was visible.
`complete=0` marks a truncated command. `geometry_frame` can repeat when no new geometry is submitted. Empty or
stale metadata must not be interpreted as a second camera. The effective first projection is only a diagnostic
sample, not a description of every draw in a scene. The API never selects a game scene or layout.

Use `--hashes build\...` for guest-state comparisons and `--shots build\...` for private visual checkpoints.
They add overhead, so do not use such runs to measure real-time audio or frame pacing. The original two screen
images remain available. Add `--preview-tower-camera --headless --cinematic-aspect 1.3333333333` to request the Tower
experiment. The program rejects this preview in an interactive window.

## Implementation boundaries

The engine accepts a generic pair of vertical projection offsets, computes the union's center/half-span, and
changes only the extra camera. Centered perspective can be world geometry; orthographic UI remains UI. No game IDs
or story-specific placements enter the engine. Both original screen buffers and guest state remain untouched.

Cole uses the symmetric camera and full-size UI. Its overlay probes (26/31/88/102) are necessary but insufficient:
the policy checks capture, fresh geometry, projection kind, swap polarity, focal scale and an opposed frame pair.
At cuts 7,609 and 8,691 the focal scale changes and offset rounding changes by up to four SDK units. After scene
entry, a fresh opposite half within eight SDK units retains the layout across those cuts. Stale frames, changed
residency and unmatched spans release it. Cole never inherits the opening's logo rectangle.

The Tower probe group (31/48/70/93/95) plus a fresh positive/centered pair supplies the headless experiment.
Its new world projection changes the position of the original Touch Screen viewport. Current mouse routing uses
normalized DS coordinates, so enabling this camera interactively would misplace direct clicks and potentially UI
hit targets. The regular launcher therefore retains the configurable two-screen layout in this context.

The additional `ST_NATIVE_SMOKE_MODULE`, `ST_NATIVE_SMOKE_SAVE`, `ST_NATIVE_SMOKE_SCRIPT` and
`ST_NATIVE_SMOKE_SCENE_PROFILE` controls work only with `st_launcher --game-smoke <ROM>`. Saves are read-only;
module overrides still pass normal host validation. Normal launcher sessions ignore these controls. Use existing
`ST_NATIVE_SMOKE_LAYOUT`, `ST_NATIVE_SMOKE_FRAMES` and `ST_NATIVE_SMOKE_ORIGINAL` controls to exercise layouts,
pause and manual override. Keep all input files and generated output below ignored `build/`.

## Verification and remaining acceptance

### Verification of the initial diagnostic changes

**Verified locally:** Windows program/recompiler/launcher build; all 13 C++/UI tests pass. The Python CI command
(`python -m unittest discover -s tests -t .`) reports 12 tests, one skipped, no failures. Other platforms and a remote
GitLab pipeline were not run for this change.

A 7,500-frame Cole-checkpoint replay with graphics diagnostics, requested cinematic output and `--gx-check`
matches a reference run without those options: all 7,500 state/RAM hashes, interpreter counts, and 3/3 original
dual-screen checkpoint images agree. The geometry checker replays 4,651 scenes with zero differences. Metadata
also matches the first 7,500 frames of the earlier dense survey. The automatic join stays off after loading this
checkpoint; this verifies diagnostic isolation and the existing opening pass, **not a new joined Cole render**.
All four tested save copies remain byte-identical to their extracted source files.

### Verification of the scene profiles and span support

**Verified locally:** all 14 C++/UI tests pass; Python reports 12 tests, one skipped, no failures. Synthetic tests
cover scene entry/exit, stale geometry, missing capture/residency, incorrect polarity, unmatched focal scales,
camera cuts, profile isolation, UI placement and asymmetric projection bounds. The engine's default camera remains
compatible: all 4,200 opening state/RAM hashes and 8/8 original/cinematic golden checkpoint images agree.

Cole's final 12,000-frame replay matches all reference state/RAM hashes and interpreter counts. The joined interval
is continuous on all 3,114 frames from 6,597 through 9,710, including both lens changes. The final pass checks 10,737
geometry replays with zero differences, and all four common original-screen checkpoints agree. An earlier dense
pass also compared 68 original-screen checkpoints with zero differences. Extra private captures cover scene entry,
camera cuts and exit. Original DS remains available through the existing setting and F2 override.

The real-window Cole checkpoint smoke test exits successfully in the same launcher window, exercises Esc/F1
pause/resume and both F2 transitions, and leaves the save byte-identical. Presentation metadata confirms cinematic
output on all 104 frames 6,597–6,700, original screens on all 30 frames 6,711–6,740, then cinematic output on all
20 frames 6,741–6,760. This is a functional test; concurrent correctness runs make its timings unsuitable as a
performance benchmark.

The asymmetric Tower research pass preserves all 12,000 reference state/RAM hashes, 68/68 original-screen
checkpoints, and 22,371 geometry replays. The final headless-only option was additionally checked over 3,000 frames:
all reference hashes agree, 17/17 common original checkpoints agree, and 4,403 geometry replays have no differences.
These are rendering-isolation checks, not interactive acceptance. Small alternating-image differences remain in
stationary Tower probes; camera continuity/culling and visual quality still require review.
The default path was also checked through frame 2,300: no Tower join after loading. The CLI rejects an interactive
`--preview-tower-camera` invocation before running the game.

No game images were uploaded for visual review. Human acceptance of framing/UI remains pending. These headless
correctness runs do not measure real-time audio or frame pacing. Other platforms and remote GitLab CI were not run.

### Further acceptance work

1. Cole: complete human visual review of UI, fades and camera cuts. Its identity UI profile is separate from the
   opening's logo rectangle.
2. Tower: finish continuity/culling checks, implement inverse touch routing and matching UI composition, then
   validate interactions before enabling its profile in the launcher.
3. Reach the boss introductions from the Cragma/Skeldritch checkpoints; test further catalogue points and the finale.
4. For each enabled profile, verify original per-frame hashes and screen buffers with the feature off/on; check
   geometry restoration, touch, transitions and the Original DS override, then measure real-time audio/frame pacing.

Do not call this a universal or lossless join. The current extra camera is a 256×192 reframing, and some scenes may
use independent cameras or 2D art requiring a different policy.

## Campaign checkpoint bootstrap (2026-11-18)

The current context-by-context requirements and acceptance gates are in
[SINGLE_SCREEN_COVERAGE.md](SINGLE_SCREEN_COVERAGE.md). A later survey ran twelve additional save/slot pairs for
3,000 frames each, read-only, without movement, attacks or assumed encounter success. Every process exited 0 and
every source save remained byte-identical. This establishes repeatable starting points, not boss/duet acceptance.

| Candidate | Save / slot | Local evidence |
|---|---|---|
| FL02 Gage | Saves03 / 2 | Boot only |
| BO01 Stagnox | Saves04 / 2 | Boot only |
| FL03 Steem | Saves07 / 2 | Boot only |
| BO02 Fraaz | Saves08 / 2 | Boot only |
| TR06 Carben rescue | Saves11 / 1 | Boot only |
| FL04 Carben duet | Saves11 / 2 | Boot only |
| BO03 Phytops | Saves12 / 2 | Boot only |
| FL05 Embrose | Saves14 / 2 | Boot only |
| BO05 Byrne | Saves17 / 2 | Boot only |
| FL06 Rael | Saves18 / 2 | Boot only |
| TR08 Demon Train | Saves23 / 1 | Boot only; display swap set throughout frames 2,000–3,000 |
| BO08/FL07 finale | Saves23 / 2 | Boot only; display swap set throughout frames 2,000–3,000 |

The post-load windows recorded no capture/off-axis pair and matched no existing automatic scene profile.
This does not imply that their upper screens are unnecessary. With the earlier four scene checkpoints, the
documented work has exercised 16 distinct save/slot pairs across 13 files, at different evidence levels.

To reproduce a checkpoint bootstrap on Windows, using your own local files and a new output directory:

```powershell
.venv\Scripts\python -I tools\testing\checkpoint_survey.py `
  --rom "<your-ROM.nds>" --module "<your-local-st_game.dll>" `
  --saves "<local-spirit-tracks-save-collection>" `
  --case FL02:3:2 --case BO02:8:2 --out build\coverage-bootstrap-01
```

The helper requires existing local files, uses `--read-only-save`, checks source digests before/after, refuses to
overwrite a nonempty output directory, and confines outputs to this checkout's ignored `build/`. It records dense
graphics metadata and guest hashes, without requesting screenshots or protected-code tracing. Every report keeps
`encounter_confirmed=false` and `presentation_accepted=false`; a successful load cannot promote either claim.
Raw logs and save content are never part of the publication bundle.

**Verified helper checks:** Python reports 15 tests, one skipped, no failures, including output containment, preservation of existing
results, case/slot validation and rejection of non-dense metadata. The published helper was then run sequentially
for FL02 and BO02: both 3,000-frame bootstraps passed and the source saves were unchanged. The initial exploratory
FL02 sequence did not establish the actual duet. Subsequent recorded navigation reached practice and the Fraaz
arena as detailed below; neither full encounter is accepted. No new automatic scene
profile is enabled by this survey. The launcher identifies manual Single screen as incomplete in both settings
menus; the Windows build, all 17 C++ tests, launcher UI smoke and synthetic framebuffer checks pass.

## Gage duet and Fraaz entry (2026-11-19)

The [bounded encounter survey](FLUTE_AND_FRAAZ_SURVEY.md) adds own-input replays beyond the earlier bootstraps.
Gage's teaching, free practice/Back, all three duet turns, joint performance, rail reward and resumed walking are reached.
Fraaz's warp route, reveal and first ordinary combat are reached; split forms and fight completion remain open.
These are **Reached**, not Implemented or Accepted, in the campaign register. No new automatic rule is enabled.

**Verified:** Fraaz's 6,511-frame off/on replays have identical state/RAM hashes and interpreter totals
(1,990,039 instructions each). All 7 original dual-screen checkpoints agree, the source save is unchanged,
and no existing automatic profile or display capture is active from frame 3,000 through 6,511.
These headless runs do not measure real-time audio/frame pacing or approve a new PC composition.

**Historical, before virtual-breath calibration:** Gage's 8,781-frame off/on replays have identical state/RAM hashes and interpreter totals
(1,901,261 instructions each). All 8 original dual-screen checkpoints agree and the final practice checkpoint
matches the interactive exploration. The source save is unchanged. No existing automatic profile or display
capture is active from frame 3,000 through 8,781. This old run did not pass teaching.

**Verified with the calibrated source:** `gage_teaching.txt` passes teaching, exposes free-practice Back and returns
to Gage's dialogue. All 10,484 state/RAM hashes match off/on (2,202,676 interpreter instructions each).
Compared with the former weak-signal baseline, all first 8,461 frames match; the first intentional difference
occurs at frame 8,463 after virtual breath begins. No guest function, hook or protection routine was changed.

**Verified full duet:** `gage_duet.txt` replays 17,241 frames through successful completion and resumed walking.
All state/RAM hashes match off/on, with 3,595,601 interpreter instructions each. All 29 original dual-screen
checkpoints and 58 foreground-plane checkpoints match; nine success/reward/world-return checkpoints also match
the successful interactive recording. Both source-save hashes are unchanged. The virtual input is silent from
frame 13,000 through the end. No current automatic screen profile is active after loading at frame 3,000.
This validates the native gameplay route and presentation isolation, not a new one-canvas flute composition,
retry after failure, other duets, real-mic behavior or real-time audio/frame pacing.
