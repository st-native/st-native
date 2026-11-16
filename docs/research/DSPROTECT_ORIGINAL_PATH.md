# Preserving the game's own anti-piracy checks (DS Protect)

Dates: **2026-11-14** (first attempt, then the fix). Windows x64, the maintainer's USA/Australia rev. 0 dump, isolated
outputs under `build/dsprot/` and `build/p9/` (git-ignored). Questions: can the build (1) avoid having the extractor
decrypt the protected routines, and (2) run the game's original path with the current replacements turned off, from
the title screen to a new game?

**Answer.**

- **(2) Yes (Verified).** With the replacements off, the game's own checks run on the runtime's interpreter, as the
  game decrypts them in emulated memory, and the scripted boot **reaches the new game**: 6,200 frames, the same ten
  screenshots as the default build, and the same per-frame RAM state except in the frames where the checks run
  (section 3). It took two engine patches (0008, 0009).
- **(1) Yes (Verified, on the boot script).** dsd cannot avoid the decryption, but a function list from our own
  extractor and discovery, which decrypt nothing, now boots to the new game too, **with the game's own checks and no
  replacements at all**: same ten screenshots as the default build (section 4). It runs about a million instructions
  on the interpreter (functions our list still lacks), so its timing, and therefore some RAM bytes, differ from the
  default build's; the differing addresses are the ones R4 traced to timing alone.

- **The replacements are not enough past the first scene (Verified, 2026-11-14, section 5).** On the first train
  ride, builds that replace the six entry points (the default build, and ours with replacements) never show the
  train's controls (gearbox, whistle, timer, Menu); the train cannot be sped up, stopped or cleared of animals, and
  the session cannot go on. This matches the game's documented anti-piracy response (it removes the train control
  UI). Builds that run the game's own checks (`--dsprot original`, with either function list) drive the train to
  Castle Town, save there, and reload the save. **Playing past Aboda needs `--dsprot original`.**

How the routines are handled follows the project's rules (AGENTS.md): we do not decrypt, analyse or trace them; the
game's own code decrypts them in emulated memory and melonDS's interpreter executes them, as the console would. Only
addresses, frame numbers, counts and the runtime's messages are recorded here.

## 1. What changed (all optional; the default build is byte-identical)

- **Engine patch 0008, `interpret.txt`:** guest code ranges the recompiler never translates; calls into them go
  through the runtime, which runs them on its fallback interpreter. The ranges are also written into the module's
  table as interpreted regions, "resident" while the overlay that carries them is. A hooked entry point inside a range
  stays a hook.
- **Engine patch 0009, the interpreter calls recompiled code:** interpreted code that reaches the entry of a recompiled
  or hooked function runs that function's recompiled version (so the runtime's own replacements, such as thread
  switches, stay in force) and resumes where it returns. Code that starts inside an interpreted range runs purely on
  the interpreter, with everything it calls: it may depend on memory that differs from the ROM.
- **Engine patch 0010, residency of a lone candidate:** the runtime used an overlay's function at an address
  whenever it was the only one listed there, even with another overlay resident; it now checks residency (section 4).
- **`game/st/config/bkie/interpret_dsprot.txt`:** the block of the encrypted routines, ov001
  `0x020C0A04`-`0x020C2550` (addresses from dsd 0.12.1's listing in R1).
- **`st_build.py --dsprot original`:** leaves out the six "genuine cartridge" replacements and adds that range.
- **`st_build.py --functions ours`:** the function list from our extractor and discovery instead of dsd (section 4).
- **Regression checks:** with neither option, the module is still SHA-256 `6b3b3a41…` (all artifacts identical), and
  the scripted boot gives the same 6,200 per-frame hashes, with 0 instructions on the interpreter (Verified after each
  patch).

## 2. The first attempt, and what it taught

Before patch 0009, with the replacements off, the run stopped at the first of the six entry points (`0x020C0F04`,
called from game code at `0x020B790A`), runtime frame 130: `fatal: interpreter left guest code at ffff0010`. The
first version of 0009 then stopped even earlier, inside a routine of the block that had run fine before: code that
starts in the block must not be handed to recompiled versions made from the ROM's bytes. Both points are fixed by the
final 0009 (pure interpretation inside interpreted ranges, recompiled calls elsewhere).

## 3. The original checks, replacements off (dsd's list) — passes

Build `--dsprot original` (`build/p9/st-dsd-orig`), script `tests/rom/boot_new_game.txt`, compared with the default
build (`build/p9/st-default`) on the same script:

| | Default (replacements) | Original checks |
|---|---|---|
| Frames run | 6,200 (to the prologue of a new game) | **6,200** |
| Screenshots (10 checkpoints) | reference | **all 10 identical** |
| Interpreted instructions | 0 | 192,542, in frames 35, 131, 1,157, 3,179 and 3,908 only |
| Entry points first run on the interpreter | none | 6: five routines inside the block (four called by the start-up initialisers) and the entry point `0x020C0F04` |
| Per-frame RAM hash | reference | **identical in 6,182 of 6,200 frames**; differs only in the frames where the checks run and up to 7 frames after, then equal again |

- **Exercised: yes.** The checks ran five times in the script, the first at frame 35.
- **Outcome:** the game took the same path as with the "genuine cartridge" replacements, through title screen, save
  creation, file select, name entry and the start of a new game, and its memory returns to the same state after each
  check. The transient differences are the checks' own working memory and timing (Likely; we do not look at what the
  routines do).
- **Later in play** (section 5): checks run during play too, and on the first train ride the replacements and the
  original checks no longer lead to the same game.

## 4. Avoiding the extractor's decryption — boots

| Step | Result |
|---|---|
| dsd without decryption | **Impossible:** `dsd rom extract` has no such option; `dsd init` on the bytes as stored stops at "Local function call from 0x020b790a in overlay 1 to 0x020c0f04 leads to no function" |
| Our discovery, as R1 left it | stops at frame 6: a false start found as a branch target (`0x02027754`) split a real function, whose second part returned through an address it never saw in LR |
| `st_build.py --functions ours` fixes, each found at the first failing frame | drop branch-target-only starts (36); add every overlay's static-initialiser table as seeds (867 entries, 88 new); keep hook targets even where encrypted bytes cannot be decoded; set ARM for adjacency starts whose first four words carry ARM's "always" condition (17); drop starts the recompiler reports as called in the other instruction set, or as switch cases reached from a jump table (patch 0008 reports them) |
| Still stopping at runtime frame 133 (`no recompiled function at 30303030`) | a mix of our list and dsd's, module by module and then entry by entry, isolated one difference: ov088's function `0x02166EE4`, missing from our list. Cause: an **engine bug**: when an address has one candidate function, the runtime used it without checking that its overlay is resident, so ov088's call ran ov093's function at the same address. **Patch 0010** checks residency; a missing function then runs on the interpreter as intended |
| **With patches 0008-0010** | `--functions ours` (replacements) and `--functions ours --dsprot original` (the game's own checks) both **run the 6,200-frame boot to the new game**, with the same ten screenshots as the default build |

Comparison with the default build on the same script (`build/p11/`):

| | Default (dsd list) | Ours, replacements | Ours, original checks |
|---|---|---|---|
| Frames | 6,200 | 6,200 | 6,200 |
| Screenshots identical to the default | — | 10/10 | 10/10 |
| Interpreted instructions | 0 | 1,005,246 (54 entry points) | 1,127,343 (55) |
| Main RAM vs the default | — | — | identical at frame 1,100; 1,150 bytes differ at frame 3,000 and 25 at frame 6,199, at addresses R4 found to change with timing alone (`0x0226C6A8`/`0x0226CAA8`, `0x022E4FD8`, `0x02049CB0`...) |

Our list has 32,320 starts against dsd's 32,524 (measured against dsd's list only to evaluate, never as an input). The
functions it lacks run on the interpreter: correct but slower, and with other cycle counts. Seeding only starts with
strong evidence is worse (the boot stops before the first frame).

## 5. Longer sessions

Same four builds, current program (patches 0008-0010), compared with the default build on each session (outputs
under `build/long3/` and `build/route/s4/`, git-ignored):

| Session | Frames | Default | dsd list + original checks | Ours + replacements | Ours + original checks |
|---|---|---|---|---|---|
| `tests/rom/first_scene.txt`: boot, prologue, first scene's dialogue up to its first choice | 17,400 | runs, 0 interpreted | runs; checks run 5 times (frames 35-3,908), no more; RAM identical in 17,382 frames | runs; 3/3 screenshots | runs; 3/3 screenshots |
| `tests/rom/early_gameplay.txt`: also both choices and the first minutes of play (walking around Niko's house by touch, talking, dismissing dialogue) | 31,000 | runs, 0 interpreted | runs; **checks run 10 times, 5 of them during play** (frames 23,674, 23,686, 25,432, 26,818, 27,014); RAM identical in 30,946 frames, differing only inside those windows (up to 15 frames each); 7/7 screenshots | runs; 7/7 screenshots | runs (3,997,167 interpreted instructions); 7/7 screenshots |

| `tests/rom/castle_town.txt`: early_gameplay's first 26,600 frames, then out of Niko's house to the station, the first train ride (Alfonzo's final exam: gearbox, whistle, a rail switch, stopping at the station) to Castle Town, and an in-game save there; `castle_town_load.txt` reloads it | 52,400 + 3,000 | **blocked on the train**: runs to the end without an error, 0 interpreted, but the train's controls never appear; stuck behind the animals on the tracks from frame ~36,300; no save | **reaches Castle Town, saves, reloads**; checks in 9 windows (35, 131, 1,157, 3,179, 3,908, 23,674-23,686, 25,432, **31,422-31,436, 49,743-49,756**) | **blocked on the train**, like the default | **reaches Castle Town, saves, reloads** (16,875,295 interpreted instructions); two serial runs identical in all 52,400 + 3,000 frames, the same save (SHA-1 `b64d3fc5…`) |

- **The checks also run during play**, not only at start-up. Up to the first scene memory returns to the same state
  after each window and every screenshot matches the replacements.
- **The first train ride separates the two paths.** With the replacements (default build, ours + replacements) the
  train leaves Aboda but its bottom-screen controls (gearbox lever, whistle rope, the 300-second timer, Menu) are never
  shown, every touch turns the camera instead, and the train cannot pass the animals on the tracks. With the original
  checks (dsd's list or ours) the controls are there and the whole ride plays. This is the game's documented
  anti-piracy response, which removes the train control UI
  ([The Cutting Room Floor](https://tcrf.net/The_Legend_of_Zelda:_Spirit_Tracks)). The replacements answer the six
  entry points "genuine"; some later check still concludes otherwise (Likely; we do not look at which, AGENTS.md).
  The ARM9 clock and cartridge timing were ruled out only partly: with `--cpu-speed 1` / `--slow-cart` the scripted
  route desyncs inside Niko's house, so those runs never reach the train.
- **New check windows** in this session (dsd list, where the interpreter column shows only the checks): frames
  31,422-31,436, as the train leaves Aboda and before its controls appear, and 49,743-49,756, on arrival at Castle
  Town. The windows at 26,818 and 27,014 of early_gameplay do not occur, as the route differs after frame 26,600.
  Ours + original runs the same block, but its interpreter column also holds the functions its list lacks.
- **Save and reload** (ours + original, the user's own separate test save): "Save complete." at frame ~52,260; on a
  new launch Select File lists file 1 (name, three hearts), Start, Adventure, and the game resumes in Castle Town at
  the station beside Alfonzo and the train (banner, town map, Menu with Save / Collection / Map).
- **Real-time cost** (ours + original, headless, `--perf`, one run alone, this PC). Before the save fix below: 5.4 ms
  of host time per frame on average (32% of a DS frame), 59 of 52,400 frames over 16.7 ms, and one real stall of
  1,169 ms at frame 2,835: the program wrote the whole 1 MB save file for every one of the game's backup writes while
  the new save was created. The program now writes the file once the game has stopped writing for half a second (and
  when it stops). **After the fix:** 4.0 ms per frame on average (24% of a DS frame; 3.6 before play, 4.1 on foot,
  4.5 on the train, 4.2 in Castle Town and saving), slowest frame 19.4 ms, and only 2 frames over 16.7 ms (31,431,
  inside a check window, and 35,911 on the train); per-frame hashes and the save are unchanged. A late frame is caught
  up by the next ones (the window's pacer keeps a deadline), so the game keeps its speed. Not yet watched in a window
  for the whole session (M2's 30-minute soak).
- **Comparisons run one at a time:** each `st_native` running at the same time gets its own
  local-wireless instance and MAC address, so their memory differs from frame 7 (five parallel runs all reached
  Castle Town nonetheless). Comparisons are run one at a time (instance 0).
- **Our list** stays functionally on the same path as dsd's in every session, with the timing differences of
  section 4 throughout.

## 6. What is still to show

1. **Fewer functions on the interpreter** in the list that decrypts nothing: the runtime names each first entry
   (`interpreter: first entry at ...`); improving our discovery so that those become seeds would make its timing,
   and so its RAM, match the default build's frame for frame.
2. **Further into the game:** Castle Town is reached (section 5); next, the castle, the Tower of Spirits and the
   first dungeon, where later checks may run again.
3. **Legal review** (Q4, Q14) before choosing the default: running the original checks needs no change to them, and
   building without dsd decrypts nothing. Until then the default build keeps dsd's list and the replacements, **which
   cannot play past the first train ride** (section 5): a player build would have to be `--dsprot original`.
