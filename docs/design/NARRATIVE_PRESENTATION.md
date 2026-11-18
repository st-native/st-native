# Narrated scenes in automatic presentation

Date: 2026-11-18. Scope: the USA/Australia revision 0 new-game prologue, Windows software rendering.

## Behavior

**Implemented:** `Presentation → Automatic (PC)` composes the prologue's art and narration in one centered 4:3
canvas. The art comes from the original Touch Screen. Its pixels keep their positions; the narration comes from
the transparent foreground of the original top screen, translated toward the lower part of the art. A translucent
black band improves contrast. An empty foreground produces only the art, without an empty caption panel.
**Screen features → Subtitles over artwork** controls this composition independently of cutscene cameras and
secondary-screen hiding. Turning it off restores the selected two-screen layout for this prologue.
See [independent screen features](DUAL_SCREEN_UX.md#independent-screen-features-2026-11-18) for the shared settings,
preference migration and option-isolation checks.

The compositor uses the game's actual glyph pixels, spacing, colors, fades and typewriter progress. No text is
transcribed, recognized or replaced. Long foregrounds fit inside the canvas rather than being clipped. The current
renderer remains 256×192; this change is not higher internal resolution or a font replacement.

The original screens remain untouched and available with `Original DS` or F2. The selected two-screen layout
(including PiP) remains the fallback outside verified contexts. Existing saved layout preferences are preserved.
The first gameplay dialogue/choice does not inherit the prologue rule.

## Boundaries and evidence

`game/st/hooks/screen_context.hpp` supplies the USA rev. 0 context; `screen_policy.hpp` is the pure state policy.
Local new-game replays observe overlay probes 31/62/102 alongside display controls A=`0x11118`, B=`0x40211035`,
no display capture and no screen swap. Probe bytes persist into gameplay, so residency alone is insufficient.
Engine A enabling window 0 (`0x13118`) ends the rule before the in-room dialogue. Initial sampled surveys of the
52,400-frame Aboda/train route found that exact combination only in the prologue; later-game coverage is incomplete.
These are SDK display facts, not a claim to a logical scene ID. No pixel content selects the automatic profile.

Patch 0019 exposes the runtime's existing, optional software foreground planes through `ntr::Frame`. Text BGs and
palette sprites are retained; captured bitmap copies of the 3D world are excluded. This adds no GPU render pass.
The read-only register query can omit projection parsing when only display state is needed.

`app/present/subtitle_composition.hpp` owns the generic CPU composition. Alpha bounds control drawing and fitting,
not scene detection. The game-specific 40-pixel translation lives in `game/st/hooks/cinematic_profile.hpp`.
The normal observed glyph rows begin around 81 and extend through 132; their translated rows are 121–172.
The compositor clamps unusual extents and can uniformly shrink long layers. The existing backend presents the
extra plane, so UI/layout code calls no graphics API and the engine contains no Spirit Tracks scene rule.

Touch routing remains identity in this context: the art is the unchanged original Touch Screen image, and the
overlaid narration is display-only. No world-camera reprojection is applied. Unknown contexts keep the fallback
instead of inheriting this arrangement. The separate Tower camera preview remains headless-only.

## Reproduction and acceptance

Replay `tests/rom/prologue_presentation.txt` from a blank save. Compare a reference run without cinematic output
against `--cinematic-aspect 1.3333333333`; write both `--hashes` files below `build/` and use
`tools/build/compare_runs.py`. The session lasts 17,400 frames and includes entry, empty-caption periods, multiple
lines, exit and the first gameplay choice.

`--scene-profile <local.csv> --scene-stride 1` records numeric context metadata.
`--shots <local-directory> --layer-shots` also writes private foreground-plane checkpoints beside the original
screens and composed image. Foreground images contain game data: keep them below ignored `build/` and never
publish or upload them. Hash/image correctness runs are not real-time performance benchmarks.

Use the developer launcher checkpoint smoke path with `ST_NATIVE_SMOKE_SCRIPT` pointing to the replay,
`ST_NATIVE_SMOKE_LAYOUT=pip`, and `ST_NATIVE_SMOKE_FRAMES=6200` to exercise the automatic view and Esc/F1/F2
transitions. This uses a blank save and writes no player save.

**Verified locally:** Windows build and all 15 C++/UI tests pass; Python reports 12 tests with one skipped and no
failures. The complete 17,400-frame reference/enabled runs have identical state hashes, RAM hashes and interpreter
counts. All 15 original dual-screen checkpoints and 30 original foreground-plane checkpoints are byte-identical.
The automatic prologue interval is continuous on frames 3,912–10,970 (7,059 frames); it is off throughout the
subsequent sampled gameplay through frame 17,400. Geometry validation checks 6,657 replays with zero differences.

At the four nonempty composed checkpoints, all 6,151 original foreground pixels retain their exact colors at the
translated coordinates. The upper 112 art rows remain unchanged. At two empty-caption checkpoints the composed
image is exactly the original art, without a band. These numeric checks establish source preservation and placement,
not visual approval of every story card.

The PiP-origin launcher smoke test exits successfully after Esc/F1 pause/resume and F2 override. Presentation
metadata confirms the composed image on 2,201 consecutive displayed frames in the prologue, original screens for
all 30 frames after switching with F2, and the composed image again for the final 20 frames. It keeps the same
window and starts with a blank, nonpersistent save. Concurrent correctness work makes these timings unsuitable
as a performance benchmark.

**Pending:** human visual acceptance across all narration cards and fades; additional languages, later narration
contexts, other platforms and real-time performance measurements. Synthetic tests cover transparent/empty layers,
typewriter stability, multiline placement, alpha blending, long layers, invalid styles and output bounds. The
context tests reject gameplay's display-window state, missing probes, capture and screen swap.

## Stylus and handedness prompt

The original Right/Left question controls handedness and mirrors parts of the original interface
([Nintendo instruction booklet](https://www.nintendo.com/eu/media/downloads/games_8/emanuals/nintendo_ds_21/Manual_NintendoDS_TheLegendOfZeldaSpiritTracks_EN.pdf),
pp.11 and 13). It does not choose a PC input device or require a physical pen.
That preference can still matter with mouse/touch and for accessibility. **Unchanged in this implementation:** the
original prompt, choice, confirmation and saved preference remain active; no automatic answer is injected.

**Proposed:** present this later as a device-aware controls preference, with PC wording and optional setup for
mouse, controller or touch. Preserve existing save preferences and verify both handedness layouts when adapting
train controls and UI hit targets. Rewriting/skipping the tutorial is separate from composing the prologue.
