# PC presentation: next delivery steps

Updated: 2026-11-19. Status: **Proposed delivery sequence, approved direction by the maintainer.**
This implements M4/M5 incrementally; it does not replace M1–M8 or claim campaign-wide coverage.
The current task is **P1, Gage**. Start with a complete interaction, implement its presentation and input,
then verify it before extending the same components to other contexts.

## P1 — Complete Gage's interaction

- [x] Reach the introduction and instrument practice with recorded, read-only input.
- [x] Preserve baseline guest hashes and original screens with existing enhancements off/on.
- [x] Pass the initial teaching stage with our own virtual blow, reach free practice/Back, and enter a real duet attempt.
- [x] Complete the actual duet; record all three player turns, joint performance, restored rails and resumed walking.
- [x] Observe a timing-failure return separately from the successful route.
- [ ] Record retry from failure and interruption behavior.
- [ ] Verify context entry/exit and interruptions; exclude solo flute, field, menus and unrelated scenes.
- [ ] Implement one PC canvas preserving the note guide, partner, instructions and usable flute controls.
- [ ] Integrate optional remappable button notes for gamepad and keyboard, with controller-first ergonomics.
- [ ] Add Controls pages and binding-driven prompts: Automatic/Xbox/PlayStation/Nintendo symbols with override;
      configuration and gameplay share one binding model (2026-11-20 requirements in CONTROLS_AND_MICROPHONE.md).
- [ ] Verify inverse pointer mapping, sustained notes, breath, handedness and original-view override.
- [ ] Replay off/on, review the actual presentation and complete the interaction using the adapted view.

Evidence: [FLUTE_AND_FRAAZ_SURVEY.md](FLUTE_AND_FRAAZ_SURVEY.md), `tests/rom/gage_teaching.txt` and
`tests/rom/gage_duet.txt`. **Verified:** calibrated virtual breath passes teaching and the full Gage duet, restores
the rails and returns control to Link. The 17,241-frame successful replay has identical guest state/RAM with
existing screen features off/on, including all 29 original-screen checkpoints and 58 foreground-plane checkpoints.
A PC flute composition remains open. It must preserve teaching captions, free-practice Back, note/turn guidance,
lower-screen gesture feedback, the joint-performance cameras and the map reward; the opening profile cannot do this.
The played pose also appears in a pointer-only control and cannot establish accepted breath or song success.
The separate [button-note enhancement](ENHANCEMENTS.md) remains planned; the first layout change must preserve
the existing input path and must not depend on an unverified note-selection hook.

## P2 — Complete Fraaz's phase coverage and adaptation

- [x] Reach the reveal and ordinary combat with a replay; preserve baseline off/on hashes and original screens.
- [ ] Record both elements, split forms, arena changes, required map cues, damage/retry and fight exit.
- [ ] Verify the context lifecycle and implement essential indicators that appear automatically with combat.
- [ ] Complete all phases using the adapted presentation; verify boomerang targeting, information timing and overrides.

Evidence: `tests/rom/fraaz_boss_entry.txt` and the bounded survey above. Exploratory sword/element interactions
are not complete phase coverage. Manual map peek cannot substitute for compulsory identity cues.

## P3 — Consolidate the reusable composition and input components

Build the shared pieces during P1/P2, then consolidate what those two different cases actually require:

- [ ] Explicit source regions, destination placement, stacking and pointer mappings in the app presentation layer.
- [ ] Small game-specific profiles selected from verified state; no per-frame pixel/OCR classification.
- [ ] Tests for context entry/exit, stale or missing evidence, clipping, resize, display swap and drag capture/release.
- [ ] Independent settings and Original DS override; preserve both source images and guest state.

Engine changes, if needed, remain generic patches. Layout and game policy call no graphics API. Avoid a speculative
universal framework: a new context reuses a proven component only after its information and interactions are checked.

## P4 — Train and Zelda/Phantom gameplay

- [ ] TR02/TR03: route editing, travel radar, junctions and threats, including entry/exit from interactive maps.
- [ ] GP04–GP06: remote partner control, route drawing, viewpoint changes and cooperative puzzles.
- [ ] Preserve required information automatically; validate each distinct variant before expanding its profile.

Keep the other menu, cinematic, boss, duet and optional-content checks in
[SINGLE_SCREEN_COVERAGE.md](SINGLE_SCREEN_COVERAGE.md). The discreet map-discovery hint remains Proposed there.

## P5 — Reviewable developer alpha

- [ ] Define a supported route and adapted contexts explicitly; list unverified campaign/content separately.
- [ ] Verify ROM selection, preparation/progress, cache reuse/rebuild, launch, settings and return to launcher.
- [ ] Verify save/load and backup behavior without overwriting a player's original save during tests.
- [ ] Run a real-time session for frame pacing/audio separately from diagnostic hash runs.
- [ ] Run the applicable ROM-free checks/CI and a fresh setup by another developer, with reproduction instructions.
- [ ] Assemble and scan a source/distribution bundle containing no ROM, generated game module, save or game assets.
- [ ] Maintainer review/publication under the existing authorization and publishing rules.

An alpha may cover a declared subset while other contexts retain the development fallback. Making automatic
Single screen the definitive default still requires the campaign-wide acceptance gates in the coverage register.
Windows is the initial execution target; cross-platform buildability remains a requirement throughout.

## Completion evidence

For every delivered context, link its input replay, verified detector provenance, off/on results, visual/input
review and remaining limitations. Raw game data stays below ignored `build/`. A reached checkpoint, clean hashes
or a successful build alone cannot tick the adaptation's completion box. Update this plan and the campaign
register together when evidence changes; do not mark a whole duet/boss family complete from one member.
