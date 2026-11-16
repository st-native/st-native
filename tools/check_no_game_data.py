#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Game-data guard: fail if anything derived from the game is in the files to be published or committed.

  python -I tools/check_no_game_data.py --tree <folder>   # scan every file under a folder (the publication bundle)
  python -I tools/check_no_game_data.py --staged          # scan staged files (git pre-commit hook)
  python -I tools/check_no_game_data.py                   # scan all tracked files (CI)
  python -I tools/check_no_game_data.py <file>...         # scan the given files

Blocks: ROMs and saves, DS game file formats, the user's dump by SHA-1, NDS cartridge headers, NitroSDK build tags
(dumped game code), recompiler-generated C, reconstructed game source, screenshots, and large binaries not on the
allowlist. Prints file names and reasons only. Adapted from FFCCEOT-Recomp's tools/check_no_game_data.py
(JonathanDC64, commit 934c743, GPL-3.0).
"""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

FORBIDDEN_EXTS = {
    ".nds", ".srl", ".dsi", ".ids", ".sav", ".dsv",  # ROMs and saves
    ".rsb", ".mods", ".sdat", ".sseq", ".sbnk", ".swar", ".strm", ".nsbmd", ".nsbtx", ".nsbca", ".nsbta", ".nsbtp",
    ".nsbma", ".narc", ".bin",  # DS game files and extracted modules (arm9.bin, ov001.bin, ...)
    ".bmp",  # st_native screenshots
    ".dll", ".so", ".dylib", ".obj", ".o", ".lib", ".exe",  # built modules and programs never go in the sources
}
FORBIDDEN_MAGICS = [b"MODS", b"SDAT", b"SSEQ", b"SBNK", b"SWAR", b"STRM", b"NARC", b"BMD0", b"BTX0", b"BCA0",
                    b"BTA0", b"BTP0", b"BMA0", b"BVA0", b"RGCN", b"RLCN", b"RCSN", b"RECN", b"RNAN"]
KNOWN_ROM_SHA1 = {"1f0921b5e99db9d92d24710f1b004e4f3c997b73"}  # Spirit Tracks USA/Australia rev. 0 (BKIE)
# Text markers: recompiler output, and the reconstructed-source conventions of the historical R0-R4 research.
TEXT_MARKERS = [(b"@ntr-recomp-generated", "recompiler-generated code"),
                (b"generated from the user's ROM", "recompiler-generated code"),
                (b"ST_EXPORT ", "reconstructed game source (historical research, private)"),
                (b"ST_IMPORT", "reconstructed game source (historical research, private)")]
CODE_EXTS = {".c", ".h", ".cc", ".cpp", ".hpp", ".cxx", ".inc", ".s", ".asm"}  # where the markers mean code
MAX_BINARY_SIZE = 256 * 1024
ALLOWLIST = REPO / "tools" / "game_data_allowlist.txt"  # "<sha1> <path>": our own binaries (the synthetic cartridge)
SELF = {"tools/check_no_game_data.py"}


def is_binary(data: bytes) -> bool:
    if b"\0" in data[:8192]:
        return True
    try:
        data[:8192].decode("utf-8")
        return False
    except UnicodeDecodeError:
        return True


def load_allowlist() -> set[str]:
    if not ALLOWLIST.exists():
        return set()
    return {line.split()[0] for line in ALLOWLIST.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.startswith("#")}


def check(rel: str, data: bytes, allowed: set[str]) -> list[str]:
    if hashlib.sha1(data).hexdigest() in allowed:
        return []
    problems = []
    if Path(rel).suffix.lower() in FORBIDDEN_EXTS:
        problems.append(f"forbidden file type '{Path(rel).suffix}'")
    if hashlib.sha1(data).hexdigest() in KNOWN_ROM_SHA1:
        problems.append("the game's ROM")
    if is_binary(data):
        if data[:4] in FORBIDDEN_MAGICS:
            problems.append(f"DS game data signature {data[:4]!r}")
        if len(data) >= 0x200 and data[0x15C:0x15E] == b"\x56\xCF":
            problems.append("an NDS cartridge header")
        if b"[SDK+NINTENDO" in data:
            problems.append("NitroSDK build tags (dumped game code?)")
        if len(data) > MAX_BINARY_SIZE:
            problems.append(f"binary larger than {MAX_BINARY_SIZE // 1024} KiB")
    elif rel.replace("\\", "/") not in SELF and Path(rel).suffix.lower() in CODE_EXTS:
        for marker, what in TEXT_MARKERS:
            if marker in data:
                problems.append(what)
                break
    return problems


def git(*args: str) -> list[str]:
    out = subprocess.run(["git", *args], cwd=REPO, check=True, capture_output=True, text=True).stdout
    return [line for line in out.splitlines() if line]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tree", type=Path, help="scan every file under this folder")
    ap.add_argument("--staged", action="store_true")
    ap.add_argument("paths", nargs="*")
    a = ap.parse_args()
    allowed = load_allowlist()
    if a.tree:
        root = a.tree.resolve()
        if (root / "tools" / "game_data_allowlist.txt").exists():
            allowed |= {line.split()[0] for line in (root / "tools" / "game_data_allowlist.txt").read_text(
                encoding="utf-8").splitlines() if line.strip() and not line.startswith("#")}
        files = [(p.relative_to(root).as_posix(), p.read_bytes()) for p in sorted(root.rglob("*"))
                 if p.is_file() and ".git" not in p.relative_to(root).parts]
    elif a.paths:
        files = [(p, Path(p).read_bytes()) for p in a.paths]
    elif a.staged:
        files = [(p, subprocess.run(["git", "show", f":{p}"], cwd=REPO, check=True, capture_output=True).stdout)
                 for p in git("diff", "--cached", "--name-only", "--diff-filter=ACMR")]
    else:
        files = [(p, (REPO / p).read_bytes()) for p in git("ls-files") if (REPO / p).is_file()]
    failed = 0
    for rel, data in files:
        for problem in check(rel, data, allowed):
            print(f"BLOCKED {rel}: {problem}", file=sys.stderr)
            failed += 1
    if failed:
        print(f"\n{failed} problem(s). Nothing derived from the game may be published (AGENTS.md).", file=sys.stderr)
        return 1
    print(f"game-data guard: {len(files)} file(s) clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
