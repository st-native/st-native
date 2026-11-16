# ADR-0001: Project foundation — AI-driven source reconstruction

> **Superseded on 2026-11-14 by [ADR-0008](0008-static-recompilation-port.md):** the project is a port by static
> recompilation; source reconstruction is retired and its work kept as private historical research. Gate item 4 below
> was met on 2026-11-13 (boot to a new game; R4) before the change of direction. Kept unchanged below for the record.

- Status: **Provisional** (third revision, 2026-11-12). Recommendation below is backed by an executed proof of
  concept on synthetic code ([SOURCE_RECONSTRUCTION_POC.md](../../research/SOURCE_RECONSTRUCTION_POC.md)); it becomes
  Accepted only after the ROM-based gate at the end of this record.
- History: rev. 1 recommended forking FFCCEOT-Recomp; rev. 2 an independent project around a split-ready recompiler
  engine (for portability). Rev. 3 follows the maintainer's change of direction: **the goal is an independently
  reconstructed Spirit Tracks source port**, with the recompiler at most a temporary aid.

## Context

The product is a Ship of Harkinian–style source port of Spirit Tracks for Windows, Linux, Steam Deck, Android ARM64
and macOS. The game's source must be reconstructed by us (AI-driven, human-reviewed), without zeldaret/st (its
maintainers reject AI-agent use). Evidence from the proof of concept (synthetic sample, not the game):

- Discovery, decompiler-assisted reconstruction and automated verification work end to end and are measurable
  (155/156 functions found; 10/11 reconstructions byte-identical, 1 behaviour-verified; 2 wrong first attempts caught).
- Byte-matching needs the **original compiler and flags**. For Spirit Tracks that is believed to be Metrowerks
  CodeWarrior for NITRO (`mwccarm`), proprietary, with no lawful public distribution found.
- Behavioural verification against the original machine code needs no compiler, but random testing caught 87.5 % of
  injected bugs, not all: it is evidence, not proof.
- The same reconstructed source compiled natively (x86-64, guest-layout pointers) behaved identically to the original
  in 11/11 functions, once DS integer semantics (`-fwrapv`) were kept.
- Unit-level tests cannot exercise system behaviour (threads, interrupts, DMA, timing, the GPU): that needs the whole
  game running as an oracle.

## Options

**A. AI-assisted matching decompilation, then a native source port.** Every function byte-identical with the original
compiler; the matching tree is then ported (SoH's path: zeldaret's OoT decomp, then libultraship).
- + strongest guarantee per function; whole-ROM rebuild as the final check.
- − requires lawful access to the exact `mwccarm` version (open legal question); long tail of hard-to-match functions
  (seen even on the sample); source shaped to please the compiler; nothing playable until the port layer exists.

**B. AI-assisted reconstruction verified by behaviour, non-matching where matching gives no benefit.**
- + compiler-independent; readable source; matching optional where it is cheap.
- − equivalence is statistical; system-level behaviour cannot be verified function by function; nothing runs until the
  whole game and the native platform layer exist.

**C. Hybrid: a running scaffold (static recompilation + DS runtime) from day one; reconstructed functions replace
recompiled ones progressively; the scaffold shrinks to nothing.**
- + the running game is the system-level oracle (record a session, replay with one function replaced, compare state
  every frame); continuous playability lets the dual-screen UX, controls, mic and settings advance in parallel;
  reconstructed code shares the DS memory layout through the guest-layout convention demonstrated in the PoC.
- − two code paths to maintain during the transition; the guest-layout style is less idiomatic than native pointers;
  risk of the scaffold becoming permanent if replacement stalls.

## Decision (recommended)

**C, with B's verification standard and A's matching as an optional accelerator** — chosen for verification and
incremental delivery, not for convenience:

1. **Source of truth:** reconstructed C++ in `game/st/src/`, written in the guest-layout convention (`st_guest.h`:
   DS pointers are pointers on ARM, 32-bit guest handles on the host), compiled natively with DS integer semantics.
2. **Verification ladder** per function (status vocabulary in the PoC report §6): behaviour-verified (tests +
   100 % coverage + harness mutation score on file) is the minimum to replace recompiled code; `matching` is pursued
   only if the original compiler can be used lawfully and only where it pays; `in-game-verified` (session replay with
   no divergence) is required before a subsystem's recompiled code is deleted.
3. **Scaffold:** the `ntr` recompiler and runtime lineage (rev. 2's split-ready `engine/` subtree) runs the full game
   meanwhile. It is explicitly temporary: progress is measured as the share of executed code (by instruction count
   in recorded sessions) that is reconstructed.
4. **Native platform layer:** DS services are replaced at NitroSDK/NitroSystem API boundaries (PoC report §8), so the
   ARM7, the GPU emulation and finally the recompiler can be removed.
5. **Independence:** function boundaries from our own discovery and `dsd` on the user's dump (ADR-0004); no
   zeldaret/st material; every reconstructed function records its provenance (inputs used, model/tool, verification).

## Consequences

- The roadmap gains a reconstruction track (R-milestones) beside the product track; see ROADMAP.md.
- **Legal profile changes:** reconstructed game source is game-derived and is far more exposed than a recompiler that
  ships no game code. ADR-0002's "no game-derived code in the repository" cannot hold for a source port: publication
  of `game/st/src/` is blocked on legal review (`docs/legal/LICENSING_AND_PROVENANCE.md` §5).
- Tooling to build next: dsd-based discovery cross-check, SDK signature identification, session record/replay with
  per-frame state hashes on the scaffold, SMT-guided inputs for uncovered branches.

## Gate to Accept (needs the maintainer's dump; local only)

1. Extraction + identification + SDK/compiler fingerprinting on the real ROM. **Met 2026-11-12**
   ([R1 report](../../research/R1_REAL_ROM_TOOLCHAIN.md)).
2. Discovery agreement between our tool, `dsd init` and Ghidra on the ARM9 module and three overlays. **Met
   2026-11-12** (ours finds 99.6 % of the starts confirmed by two tools; 0.37 % of ours unconfirmed).
3. 30 real functions reconstructed across SDK math, a NitroSystem utility and one game actor: report behaviour-
   verified rate, attempts per function, harness mutation score. **Met 2026-11-13**
   ([R2 report](../../research/R2_FIRST_REAL_FUNCTIONS.md): 34/34 verified, 1 attempt each; small, self-contained
   functions only).
4. Scaffold boots to the title (M2), and one reconstructed function replaces its recompiled version in a replayed
   session with no state divergence.
If (3) shows real game code cannot reach behaviour-verified at a sustainable rate, revisit B vs. a longer-lived
recompilation (rev. 2).
