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
