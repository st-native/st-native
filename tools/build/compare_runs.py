#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare two runs of the same session frame by frame (st_native --hashes files).

    python -I tools/build/compare_runs.py <reference hashes> <other hashes> [--ram A.bin B.bin]

Each hashes line is "frame state_hash interpreter_steps ram_hash". The state hash covers main RAM, both work RAMs and
the CPU clock; the RAM hash the same memory without the clock. Prints how many frames agree on each, the first frame
where each differs, and whether the instructions run on the fallback interpreter agree.

With --ram, also compares two RAM dumps (st_native --ram-dump) of the first differing frame: how many bytes differ
and in which address ranges (addresses only, never contents). Exit 0 when both hashes agree on every frame.
"""
from __future__ import annotations

import sys
from pathlib import Path


def read(path: Path) -> list[tuple[int, str, int, str]]:
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        f = line.split()
        if len(f) < 4:
            sys.exit(f"{path}: needs four columns (frame, state hash, interpreter steps, RAM hash): {line!r}")
        rows.append((int(f[0]), f[1], int(f[2]), f[3]))
    return rows


def ram_ranges(a: Path, b: Path, base: int = 0x02000000) -> str:
    da, db = a.read_bytes(), b.read_bytes()
    diff = [i for i in range(min(len(da), len(db))) if da[i] != db[i]]
    if not diff:
        return "the two RAM dumps are identical"
    ranges, start, prev = [], diff[0], diff[0]
    for i in diff[1:]:
        if i > prev + 16:
            ranges.append((start, prev))
            start = i
        prev = i
    ranges.append((start, prev))
    shown = ", ".join(f"{base + s:08x}-{base + e:08x}" for s, e in ranges[:12])
    return f"{len(diff):,} bytes differ in {len(ranges)} range(s): {shown}" + (" ..." if len(ranges) > 12 else "")


def main() -> int:
    args = sys.argv[1:]
    ram = None
    if "--ram" in args:
        i = args.index("--ram")
        ram = (Path(args[i + 1]), Path(args[i + 2]))
        del args[i:i + 3]
    if len(args) != 2:
        print(__doc__)
        return 2
    a, b = read(Path(args[0])), read(Path(args[1]))
    n = min(len(a), len(b))
    if len(a) != len(b):
        print(f"frame counts differ: {len(a)} vs {len(b)} (comparing the first {n})")
    same_state = sum(1 for i in range(n) if a[i][1] == b[i][1])
    same_ram = sum(1 for i in range(n) if a[i][3] == b[i][3])
    first_state = next((a[i][0] for i in range(n) if a[i][1] != b[i][1]), None)
    first_ram = next((a[i][0] for i in range(n) if a[i][3] != b[i][3]), None)
    interp_a, interp_b = sum(r[2] for r in a[:n]), sum(r[2] for r in b[:n])
    print(f"frames compared: {n}")
    print(f"state hash (RAM + clock): {same_state} identical" + (f", first difference at frame {first_state}" if first_state else ""))
    print(f"RAM hash (no clock):      {same_ram} identical" + (f", first difference at frame {first_ram}" if first_ram else ""))
    print(f"instructions on the interpreter: {interp_a} vs {interp_b}")
    if ram:
        print("RAM at the dumped frame: " + ram_ranges(*ram))
    return 0 if same_state == same_ram == n and len(a) == len(b) else 1


if __name__ == "__main__":
    sys.exit(main())
