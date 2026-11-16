# ADR-0004: Derive function boundaries with ds-decomp on the player's ROM, not from zeldaret/st

- Status: **Proposed**
- Date: 2026-11-12

## Context

The recompiler needs a seed list of ARM9/overlay functions (address, size, ARM/Thumb). PH Recomp imported zeldaret/ph's
ds-decomp configuration. zeldaret/st has equivalent configs for four versions (CC0-1.0), but its maintainers publish an
`AGENTS.md`/`CLAUDE.md` (no-agents.md initiative) rejecting AI-agent use of the repository. This project is developed
with AI agents and wants the trust of the Zelda community, including zeldaret.

## Options

1. Use zeldaret/st configs anyway (licence allows) — contradicts the maintainers' stated wishes; reputational risk.
2. Ask zeldaret/st maintainers; use their data only as they agree (e.g. humans only, or not at all).
3. **Run ds-decomp (`dsd`, MIT) on the player's own ROM with NitroSDK/NitroSystem signatures, import with
   `tools/import_dsd.py`, refine by hand and by recompiler discovery** (how FFCC built its list).

## Decision

Option 3 as the default path, option 2 pursued in parallel by a human maintainer. AI agents are instructed (our
`AGENTS.md`) not to read zeldaret/st. Names we commit come from SDK signatures or are written by us.

## Consequences

- More manual naming work; the recompiler does not need names to work.
- Quality of `dsd`'s automatic analysis on ST is Unknown: measured in M1b by recompiler statistics (discovered
  functions, undefined instructions reached, interpreter fallback time).
- If zeldaret/st maintainers agree to some form of use, revisit this ADR.
