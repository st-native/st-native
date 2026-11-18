# ADR-0005: Presentation is a frontend concern; context-aware layouts read verified game state, never pixels

- Status: **Proposed**
- Date: 2026-11-12

## Context

The runtime already delivers both DS screens independently (`ntr::Frame`). FFCC implements single + PiP, main + side,
stacked, side-by-side and a second window entirely in its frontend. Wind Waker HD Recomp's automatic GamePad PiP
started with brightness/pixel heuristics and replaced them with a read-only game-state reader at the frame boundary.

## Decision

- The runtime is not changed for layouts. `app/present` owns windows, layout, composition and hit testing.
- The layout engine is a pure function of (window sizes, settings, `GameContext`, frame metadata), unit-tested.
- Automatic switching uses only `GameContext` flags published by `game/st/context` from verified guest fields, plus the
  generic display-swap state (which physical screen shows the 3D engine). No pixel sampling, no guest writes.
- Every automatic choice has an immediate manual override and a pin; "Classic" (stacked, original orientation) is
  always available as the safe fallback.
- Touch goes only to the DS Touch Screen. A view of the top screen is never treated as touchable; interactions with it
  go through game-specific, verified integrations (e.g. pressing the game's own "map to Touch Screen" shortcut).

## Consequences

- Automatic mode is only as good as the verified context flags; until they exist, Auto is limited to the display-swap
  signal and is labelled experimental.
- Game-state readers need per-release address tables and tests on a synthetic guest memory (WWHD pattern).

## Implementation note (2026-11-17)

The game layer identifies the opening by residency/capture, and Cole's interval by residency plus fresh opposed
projection pairs (`game/st/hooks/screen_context.hpp`, `screen_policy.hpp`). The frontend fits the optional cinematic view to a centered 4:3
rectangle and routes that view's pointer to the original Touch Screen coordinates. The runtime supplies an extra,
recentered camera image and its UI layers; it does not choose a layout or window geometry. This is a camera
rendering enhancement, separate from laying out the two unmodified screen images. Both original images remain
available through the manual override. The camera pass restores the original geometry and never writes guest RAM.
The generic asymmetric span is used only in a headless Tower preview until inverse touch mapping is adapted.
See [scene validation](../../design/DUAL_SCREEN_VALIDATION.md) for measured coverage and remaining acceptance work.

**Narrated prologue (2026-11-18):** the game layer verifies residency and SDK display state; the app composes
optional software foreground text over the original Touch Screen art. Alpha bounds are used for rendering only,
never context detection. No additional camera pass or guest write is needed. Automatic presentation releases this
rule before gameplay's display-window state. See [Narrative presentation](../../design/NARRATIVE_PRESENTATION.md).
