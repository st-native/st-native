#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""M1a: the recompiler config for our synthetic cartridge (poc/sample, game code ZPOC; our own code, no game data).

Writes functions.txt (every function of the sample from its build's symbol tables: address, size, a/t, name),
an empty hooks.txt (the sample uses no NitroSDK, so no thread hooks) and an empty regions.txt, in the format of the
engine's game/config/<code>/ files.

  python spike/m1a/make_zpoc_config.py [--truth poc/out/truth] [--out spike/m1a/zpoc_config]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[2]


def functions(elf_path: Path) -> list[tuple[int, int, bool, str]]:
    out = []
    with elf_path.open("rb") as fh:
        elf = ELFFile(fh)
        syms = [s for s in elf.get_section_by_name(".symtab").iter_symbols() if s["st_shndx"] != "SHN_ABS" and s.name]
        starts = sorted({s["st_value"] & ~1 for s in syms})
        for s in syms:
            if s["st_info"]["type"] != "STT_FUNC":
                continue
            v = s["st_value"]
            size = s["st_size"]
            if not size:  # hand-written code (e.g. _start): up to the next symbol (_module_params follows _start)
                later = [a for a in starts if a > (v & ~1)]
                size = later[0] - (v & ~1) if later else 0
            if size:
                out.append((v & ~1, size, bool(v & 1), s.name))
    return sorted(set(out))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--truth", type=Path, default=ROOT / "poc" / "out" / "truth")
    ap.add_argument("--out", type=Path, default=ROOT / "spike" / "m1a" / "zpoc_config")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    lines = ["# Function list for the synthetic cartridge (poc/sample, ZPOC): our own code, from its build's symbols.",
             "# <start> <size> <a|t> <name>"]
    total = 0
    for section, elf in (("main", "main.elf"), ("ov000", "ovl000.elf")):
        fs = functions(a.truth / elf)
        total += len(fs)
        lines.append(f"\n[{section}]")
        lines += [f"{addr:08x} {size:x} {'t' if thumb else 'a'} {name}" for addr, size, thumb, name in fs]
    (a.out / "functions.txt").write_text("\n".join(lines) + "\n")
    (a.out / "hooks.txt").write_text("# Recompiler hooks for ZPOC: none (no NitroSDK, no threads).\n")
    (a.out / "regions.txt").write_text("# Code regions for ZPOC: none.\n")
    print(f"{total} functions -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
