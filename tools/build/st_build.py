#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the Spirit Tracks game module from the user's own ROM, into one clean output folder.

    python -I tools/build/st_build.py --rom <your.nds> --out <empty or new folder>

Steps (each logged in <out>/logs/):
  1. check the dump: SHA-1 must be a known release (only BKIE revision 0 so far);
  2. dsd 0.12.1: extract the ROM, run its analysis (`dsd init`), then delete the extraction (it holds every game file,
     and dsd decrypts the anti-piracy routines while extracting; only dsd's function lists are kept);
  3. functions.txt from dsd's lists (the engine's tools/import_dsd.py, patch 0006), plus our hooks.txt and regions.txt
     (game/st/config/<code>/) -> <out>/config/;
  4. the engine's recompiler (ntr-recomp) -> <out>/gen/ (C) and <out>/recomp_report.txt;
  5. clang-cl + lld-link with the engine's game-module flags -> <out>/obj/, <out>/st_game.dll;
  6. <out>/manifest.txt: the SHA-256 of every artifact above, in pipeline order (tools/build/compare_builds.py).

Everything written stays in <out> (derived from the ROM: never commit or share it). Run it from a VS 2022 x64
developer environment, or through tools/build/st_build.cmd, which sets one up.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGINE = ROOT / ".tools" / "upstream" / "ffcc"
DSD = ROOT / ".tools" / "dsd-v0.12.1" / "dsd-windows-x86_64.exe"
DSD_SHA256 = "5917df26bffc05d4752276a2e0b1edd21bd6e920aabe1992be8a50efc9d10932"
KNOWN = {  # SHA-1 of the whole dump -> (game code, release)
    "1f0921b5e99db9d92d24710f1b004e4f3c997b73": ("BKIE", "USA/Australia, revision 0"),
}
# The engine's game-module flags (game/CMakeLists.txt, clang-cl branch): no CRT, no stack checks, /O1. Plus /Brepro
# (ours): without it clang-cl and lld-link write the build time into every object's and the module's TimeDateStamp,
# the only difference between two builds from the same ROM (M1b, docs/research/M1B_FIRST_BOOT.md §3).
CFLAGS = ["/nologo", "/c", "/w", "/GS-", "/Zl", "/O1", "/Brepro", "-DNTR_GAME_MODULE=1"]
LDFLAGS = ["/nologo", "/dll", "/noentry", "/nodefaultlib", "/machine:x64", "/Brepro"]
STAGES = ["dsd", "config", "gen", "report", "obj", "module"]
TOOL_PROCESS_FLAGS = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


class Pipeline:
    def __init__(self, out: Path, progress: bool):
        self.out = out
        self.progress = progress
        self.logs = out / "logs"
        self.logs.mkdir(parents=True, exist_ok=True)
        self.times: list[tuple[str, float]] = []

    def announce(self, kind: str, detail: str) -> None:
        if self.progress:
            print(f"{kind} {detail}", flush=True)

    def run(self, name: str, cmd: list, *, stream_progress: bool = False, **kw) -> None:
        t0 = time.monotonic()
        self.announce("BEGIN", name)
        with open(self.logs / f"{name}.log", "w", encoding="utf-8", newline="\n") as log:
            log.write("$ " + " ".join(str(c) for c in cmd) + "\n")
            log.flush()
            argv = [str(c) for c in cmd]
            if stream_progress:
                argv.append("--progress")
                with subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                      text=True, encoding="utf-8", errors="replace",
                                      creationflags=TOOL_PROCESS_FLAGS, **kw) as child:
                    for line in child.stdout:
                        log.write(line)
                        if line.startswith("SCAN "):
                            self.announce("SCAN", line[5:].rstrip())
                    rc = child.wait()
            else:
                rc = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT,
                                    creationflags=TOOL_PROCESS_FLAGS, **kw).returncode
        elapsed = time.monotonic() - t0
        self.times.append((name, elapsed))
        if rc != 0:
            sys.exit(f"step '{name}' failed (exit {rc}); see {self.logs / (name + '.log')}")
        print(f"  {name}: {elapsed:.1f} s")
        self.announce("END", f"{name} {elapsed:.1f}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rom", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True, help="a new or empty folder")
    ap.add_argument("--recomp", type=Path, default=ENGINE / "build" / "release" / "recompiler" / "ntr-recomp.exe")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--progress", action="store_true", help=argparse.SUPPRESS)
    ap.add_argument("--extra-hooks", type=Path, action="append", default=[],
                    help="hooks appended to game/st/config/<code>/hooks.txt for this build (R4 instrumentation and "
                         "replacements); may be given more than once")
    ap.add_argument("--functions", choices=("dsd", "ours"), default="dsd",
                    help="where the function list comes from: dsd 0.12.1 (decrypts DS Protect while extracting), or "
                         "our own extractor and discovery (tools/re; keeps every byte as stored, decrypts nothing)")
    ap.add_argument("--dsprot", choices=("noop", "original"), default="noop",
                    help="noop: the six DS Protect entry points report a genuine cartridge (hooks.txt); original: no "
                         "DS Protect replacement, its block (interpret_dsprot.txt) runs on the runtime's interpreter "
                         "as the game itself decrypts it")
    ap.add_argument("--functions-file", type=Path,
                    help="use this functions.txt instead of producing one (diagnosis: mixing lists); implies the "
                         "interpret range for the anti-piracy block")
    ap.add_argument("--seeds", choices=("all", "strong"), default="all",
                    help="with --functions ours: which discovered starts become seeds (strong: called, pointed to, "
                         "in a table or an initialiser list; not those found by adjacency alone)")
    a = ap.parse_args()

    out = a.out.resolve()
    if out.exists() and any(out.iterdir()):
        sys.exit(f"{out} is not empty: give a new folder (every build starts clean)")
    tools = [a.recomp, shutil.which("clang-cl"), shutil.which("lld-link")] + (
        [DSD] if a.functions == "dsd" and not a.functions_file else [])
    for tool in tools:
        if not tool or not Path(tool).exists():
            sys.exit(f"missing tool: {tool} (see docs/BUILDING.md)")
    if a.functions == "dsd" and not a.functions_file and sha256(DSD) != DSD_SHA256:
        sys.exit(f"{DSD} is not dsd 0.12.1 (SHA-256 differs)")

    # 1. the dump
    t0 = time.monotonic()
    sha1 = hashlib.sha1(a.rom.read_bytes()).hexdigest()
    if sha1 not in KNOWN:
        sys.exit(f"{a.rom}: SHA-1 {sha1} is not a known Spirit Tracks dump (docs/BUILDING.md lists them)")
    code, release = KNOWN[sha1]
    print(f"ROM: {release} ({code}), SHA-1 {sha1}")
    p = Pipeline(out, a.progress)
    work = out / "work"

    # 2-3. the function list, then the config
    cfg = out / "config"
    cfg.mkdir(parents=True)
    funcs = cfg / "functions.txt"
    if a.functions_file:
        shutil.copyfile(a.functions_file, funcs)
    elif a.functions == "dsd":
        p.run("dsd_extract", [DSD, "rom", "extract", "--rom", a.rom.resolve(), "--output-path", work / "extract"])
        p.run("dsd_init", [DSD, "init", "--rom-config", work / "extract" / "config.yaml", "--output-path",
                           work / "dsd", "--build-path", work / "dsd-build"])
        shutil.rmtree(work / "extract")
        shutil.rmtree(work / "dsd-build", ignore_errors=True)
        p.run("import_dsd", [sys.executable, "-I", ENGINE / "tools" / "import_dsd.py", work / "dsd" / "arm9",
                             funcs, "--code", code])
        text = funcs.read_text(encoding="utf-8").splitlines(keepends=True)
        text[0] = f"# Function list for Spirit Tracks, {release} ({code}).\n"  # the engine's header names its own game
        funcs.write_text("".join(text), encoding="utf-8", newline="\n")
    else:
        # Our extractor keeps every module as stored on the cartridge (BLZ-decompressed only); our discovery reads it.
        # (-m from the project folder: our own tools.)
        p.run("extract", [sys.executable, "-m", "tools.re.ndsrom", "extract", a.rom.resolve(), work / "extract"],
              cwd=ROOT)
        (work / "analysis").mkdir(parents=True)
        p.run("discover", [sys.executable, "-m", "tools.re.discover", work / "extract", "--out",
                           work / "analysis" / "functions.json"], cwd=ROOT, stream_progress=a.progress)
        # Static initialisers: each overlay's sinit table lists functions the overlay loader calls through pointers
        # (dsd names them __sinit_*); add any our discovery missed, or they run on the interpreter.
        manifest = json.loads((work / "extract" / "manifest.json").read_text(encoding="utf-8"))
        # Starts found by adjacency take the previous function's instruction set, which can be wrong: a "Thumb" start
        # whose first four words all carry ARM's "always" condition (0xE) is ARM. (Measured against dsd's list on the
        # USA dump: fixes 9 of 16 such errors and breaks none; a recompiled body in the wrong mode is nonsense.)
        bases = {m["name"]: int(m["base"]) for m in manifest["modules"]}
        bases.update({f"ov{o['id']:03d}": o["ram"] for o in manifest["overlays"]})
        found_pre = json.loads((work / "analysis" / "functions.json").read_text(encoding="utf-8"))
        flips, cache = 0, {}
        for f in found_pre:
            if f["found_by"] == "next" and f["thumb"] and f["ok"] and f["module"] in bases:
                if f["module"] not in cache:
                    cache[f["module"]] = (work / "extract" / f"{f['module']}.bin").read_bytes()
                off = f["addr"] - bases[f["module"]]
                words = [int.from_bytes(cache[f["module"]][off + i:off + i + 4], "little") for i in range(0, 16, 4)]
                if len(words) == 4 and all(w >> 28 == 0xE for w in words):
                    f["thumb"], flips = False, flips + 1
        (work / "analysis" / "functions.json").write_text(json.dumps(found_pre), encoding="utf-8")
        print(f"  instruction set corrected for {flips} adjacency start(s)")
        sinit = []
        for o in manifest["overlays"]:
            if o["sinit_start"] and o["sinit_end"] > o["sinit_start"]:
                data = (work / "extract" / f"ov{o['id']:03d}.bin").read_bytes()
                for at in range(o["sinit_start"], o["sinit_end"], 4):
                    v = int.from_bytes(data[at - o["ram"]:at - o["ram"] + 4], "little")
                    if v:
                        sinit.append((f"ov{o['id']:03d}", v & ~1, bool(v & 1)))
        shutil.rmtree(work / "extract")
        found = json.loads((work / "analysis" / "functions.json").read_text(encoding="utf-8"))
        known = {(f["module"], f["addr"]) for f in found}
        for mod, addr, thumb in sinit:
            if (mod, addr) not in known:
                found.append({"module": mod, "addr": addr, "size": 0, "thumb": thumb, "ok": True, "found_by": "sinit"})
        print(f"  sinit tables: {len(sinit)} entries, {sum(1 for f in found if f['found_by'] == 'sinit')} new")
        names = {"arm9": "main", "itcm": "itcm"}
        # Hook targets are kept even where our discovery could not decode a body (the anti-piracy entry points: their
        # stored bytes are encrypted); a hooked function's body is the hook.
        hooked = set()
        for line in (ROOT / "game" / "st" / "config" / code.lower() / "hooks.txt").read_text(encoding="utf-8").splitlines():
            parts = line.split("#")[0].split()
            if len(parts) >= 4:
                hooked.add((parts[0], int(parts[1], 16)))
        for f in found:
            mod = names.get(f["module"], f["module"] if f["module"].startswith("ov") else None)
            if mod and not f["ok"] and (mod, f["addr"]) in hooked:
                f["ok"], f["size"] = True, 0
        # Starts with only weak evidence can be the inside of a function (split there, the second part returns
        # through a stacked address it never saw in LR: a false start at 0x02027754 did exactly that; M1B report).
        # "tail" (a branch target) is always weak; with --seeds strong, so are "next" and "gap" (found by adjacency
        # only): the recompiler still finds every function the code it translates calls.
        weak = {"tail"} | ({"next", "gap"} if a.seeds == "strong" else set())
        by_module: dict[str, list] = {}
        for f in found:
            mod = names.get(f["module"], f["module"] if f["module"].startswith("ov") else None)
            if mod and f["ok"] and f["found_by"] not in weak:
                by_module.setdefault(mod, []).append(f)
        order = ["main", "itcm"] + sorted(m for m in by_module if m.startswith("ov"))
        lines = [f"# Function list for Spirit Tracks, {release} ({code}).\n",
                 "# Facts only: start address, size, instruction set (a = ARM, t = Thumb). From our own discovery\n",
                 "# (tools/re/discover.py) on the modules as stored on the cartridge; nothing was decrypted.\n"]
        for mod in order:
            lines.append(f"\n[{mod}]\n")
            for f in sorted(by_module.get(mod, []), key=lambda f: f["addr"]):
                lines.append(f"{f['addr']:08x} {f['size']:x} {'t' if f['thumb'] else 'a'}\n")
        funcs.write_text("".join(lines), encoding="utf-8", newline="\n")
        print(f"  functions.txt: {sum(len(v) for v in by_module.values())} functions from our discovery")
    src_cfg = ROOT / "game" / "st" / "config" / code.lower()
    shutil.copyfile(src_cfg / "regions.txt", cfg / "regions.txt")
    hooks = (src_cfg / "hooks.txt").read_text(encoding="utf-8")
    if a.dsprot == "original":
        hooks = "".join(l for l in hooks.splitlines(keepends=True) if "ntr_dsprot_not_detected" not in l)
        hooks += "\n# (--dsprot original: the DS Protect replacements above are left out of this build)\n"
    if a.dsprot == "original" or a.functions == "ours" or a.functions_file:
        # Never translate the encrypted block: with our list (which sees its stored bytes as code) or when the game's
        # own checks run. Hooked entry points inside it stay hooks (patch 0008).
        shutil.copyfile(src_cfg / "interpret_dsprot.txt", cfg / "interpret.txt")
    (cfg / "hooks.txt").write_text(hooks, encoding="utf-8", newline="\n")
    if a.extra_hooks:
        with open(cfg / "hooks.txt", "a", encoding="utf-8", newline="\n") as h:
            for extra in a.extra_hooks:
                h.write(f"\n# ---- appended for this build from {extra.name}\n")
                h.write(extra.read_text(encoding="utf-8"))

    # 4. recompiler (the engine's table of known dumps lists only its own game, hence --allow-unknown-rom)
    gen = out / "gen"
    p.run("recompile", [a.recomp, "--rom", a.rom.resolve(), "--config", cfg, "--out", gen,
                        "--report", out / "recomp_report.txt", "--allow-unknown-rom"])
    if a.functions == "ours" and not a.functions_file:
        # Two kinds of wrong starts the recompiler reports: a "mode conflict" (code calls the start in the other
        # instruction set: its body would be translated nonsense) and a "jump table target is a function entry" (a
        # switch case our discovery listed as a function, splitting the real one: the boot failed on two such switches
        # in ov088, found by mixing our list with dsd's module by module). Drop them (never a hook target) and let the
        # recompiler find what is really called.
        report = (out / "recomp_report.txt").read_text(encoding="utf-8")
        wrong = {(m.group(1), m.group(2)) for m in re.finditer(
            r"(?:mode conflict for|jump table target is a function entry:) fn_([a-z0-9]+)_([0-9a-f]{8})", report)}
        wrong = {w for w in wrong if (w[0], int(w[1], 16)) not in hooked}
        if wrong:
            module, kept = None, []
            for line in funcs.read_text(encoding="utf-8").splitlines(keepends=True):
                if line.startswith("["):
                    module = line.strip().strip("[]")
                elif line[:8].isalnum() and (module, line[:8]) in wrong:
                    continue
                kept.append(line)
            funcs.write_text("".join(kept), encoding="utf-8", newline="\n")
            print(f"  dropped {len(wrong)} start(s) the recompiler found wrong (instruction set, or a switch case)")
            shutil.rmtree(gen)
            p.run("recompile_2", [a.recomp, "--rom", a.rom.resolve(), "--config", cfg, "--out", gen,
                                  "--report", out / "recomp_report.txt", "--allow-unknown-rom"])

    # 5. compile and link, in sorted order (the link order is part of the module's bytes)
    obj = out / "obj"
    obj.mkdir()
    sources = sorted(gen.glob("*.c"))
    inc = ["/I", str(gen), "/I", str(ENGINE / "runtime" / "include")]
    t1 = time.monotonic()

    def compile_one(src: Path) -> tuple[Path, int, str]:
        r = subprocess.run(["clang-cl", *CFLAGS, *inc, str(src), f"/Fo{obj / (src.stem + '.obj')}"],
                           capture_output=True, text=True, creationflags=TOOL_PROCESS_FLAGS)
        return src, r.returncode, r.stdout + r.stderr

    failed = []
    p.announce("BEGIN", "compile")
    p.announce("COMPILE", f"0 {len(sources)}")
    with open(p.logs / "compile.log", "w", encoding="utf-8", newline="\n") as log:
        log.write(f"clang-cl {' '.join(CFLAGS + inc)} <source> /Fo<obj>  ({len(sources)} sources, {a.jobs} jobs)\n")
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
            for done, (src, rc, msg) in enumerate(pool.map(compile_one, sources), 1):
                if msg.strip():
                    log.write(f"--- {src.name}\n{msg}")
                if rc:
                    failed.append(src.name)
                if done % 4 == 0 or done == len(sources):
                    p.announce("COMPILE", f"{done} {len(sources)}")
    p.times.append(("compile", time.monotonic() - t1))
    print(f"  compile: {time.monotonic() - t1:.1f} s ({len(sources)} files)")
    if failed:
        sys.exit(f"{len(failed)} files failed to compile, e.g. {failed[:3]}; see {p.logs / 'compile.log'}")
    p.announce("END", f"compile {p.times[-1][1]:.1f}")
    objs = [obj / (s.stem + ".obj") for s in sources]
    rsp = out / "link.rsp"
    rsp.write_text("\n".join(f'"{o}"' for o in objs) + "\n", encoding="utf-8", newline="\n")
    p.run("link", ["lld-link", *LDFLAGS, f"/out:{out / 'st_game.dll'}", f"@{rsp}"])

    # 6. manifest
    t2 = time.monotonic()
    p.announce("BEGIN", "manifest")
    groups = {
        "dsd": sorted(f for d in ("dsd", "analysis") for f in (work / d).rglob("*") if f.is_file()),
        "config": sorted(cfg.iterdir()),
        "gen": sorted(f for f in gen.rglob("*") if f.is_file()),
        "report": [out / "recomp_report.txt"],
        "obj": objs,
        "module": sorted(f for f in out.iterdir() if f.is_file() and f.name.startswith("st_game.")),
    }
    lines = [f"# st_build manifest: ROM SHA-1 {sha1} ({code}); SHA-256 of each artifact, by stage\n"]
    for stage in STAGES:
        for f in groups[stage]:
            lines.append(f"{stage} {sha256(f)} {f.relative_to(out).as_posix()}\n")
    (out / "manifest.txt").write_text("".join(lines), encoding="utf-8", newline="\n")
    p.announce("END", f"manifest {time.monotonic() - t2:.1f}")
    with open(out / "timings.txt", "w", encoding="utf-8", newline="\n") as t:
        for name, s in p.times:
            t.write(f"{name} {s:.1f}\n")
        t.write(f"total {time.monotonic() - t0:.1f}\n")
    dll = out / "st_game.dll"
    print(f"module: {dll} ({dll.stat().st_size:,} bytes), SHA-256 {sha256(dll)}")
    print(f"total {time.monotonic() - t0:.0f} s; manifest: {out / 'manifest.txt'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
