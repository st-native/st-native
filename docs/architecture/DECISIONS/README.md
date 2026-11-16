# Architecture Decision Records

One file per decision that is costly to reverse. Status values: Proposed → Provisional (recommended, waiting for a
validation gate) → Accepted (maintainer approval) → Superseded (link the replacement). Keep each record short:
context, options, decision, consequences, verification.

| ADR | Title | Status |
|---|---|---|
| [0001](0001-engine-foundation.md) | Project foundation: AI-driven source reconstruction on a temporary recompiled scaffold (option C) | **Superseded** by 0008 (2026-11-14) |
| [0002](0002-distribution-model.md) | Releases contain no game-derived code; build on the player's machine | **Accepted** (2026-11-14) |
| [0003](0003-project-license.md) | GPL-3.0-or-later | Proposed |
| [0004](0004-function-boundary-provenance.md) | Function boundaries from our discovery and ds-decomp on the player's ROM, not zeldaret/st | Proposed |
| [0005](0005-presentation-and-context.md) | Presentation in the frontend; context from verified game state, never pixels | Proposed |
| [0006](0006-microphone.md) | Real mic via melonDS's mic interface; synthesized virtual blowing | Proposed |
| [0007](0007-graphics-backends.md) | GL family first, backend-neutral presentation, Software fallback everywhere | Proposed |
| [0008](0008-static-recompilation-port.md) | A port by static recompilation; source reconstruction retired | **Accepted** (2026-11-14) |
