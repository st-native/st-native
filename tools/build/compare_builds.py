#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare two st_build.py output folders: per stage, how many artifacts are identical, and the first one that is not.

    python -I tools/build/compare_builds.py <out A> <out B>

For the first differing artifact it shows where: the first differing line of a text file, or the first differing byte
of a binary, named when it falls in a PE header field (COFF time stamp, checksum, debug directory). Exit 0 when every
artifact is identical, 1 otherwise. Prints paths, hashes and offsets only, never file contents.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

STAGES = ["dsd", "config", "gen", "report", "obj", "module"]


def read_manifest(out: Path) -> dict[str, dict[str, str]]:
    stages: dict[str, dict[str, str]] = {s: {} for s in STAGES}
    for line in (out / "manifest.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        stage, digest, path = line.split(" ", 2)
        stages[stage][path] = digest
    return stages


def pe_fields(data: bytes) -> list[tuple[int, int, str]]:
    """(offset, size, name) of the PE header fields that commonly vary between links."""
    fields: list[tuple[int, int, str]] = []
    if data[:2] == b"MZ":
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] == b"PE\0\0":
            coff = pe + 4
            fields.append((coff + 4, 4, "COFF header TimeDateStamp"))
            opt = coff + 20
            fields.append((opt + 64, 4, "optional header CheckSum"))
            magic = struct.unpack_from("<H", data, opt)[0]
            dirs = opt + (96 if magic == 0x10B else 112)
            fields.append((dirs + 6 * 8, 8, "debug data directory"))
    elif len(data) >= 20:  # a COFF object file
        fields.append((4, 4, "COFF object TimeDateStamp"))
    return fields


def explain(a: Path, b: Path) -> str:
    da, db = a.read_bytes(), b.read_bytes()
    if len(da) != len(db):
        size = f"sizes differ ({len(da):,} vs {len(db):,} bytes); "
    else:
        size = ""
    if b"\0" not in da[:4096]:  # text
        la, lb = da.decode("utf-8", "replace").splitlines(), db.decode("utf-8", "replace").splitlines()
        for i, (x, y) in enumerate(zip(la, lb)):
            if x != y:
                return f"{size}first differing line {i + 1}"
        return f"{size}one file continues after line {min(len(la), len(lb))}"
    n = next((i for i, (x, y) in enumerate(zip(da, db)) if x != y), min(len(da), len(db)))
    diffs = sum(1 for x, y in zip(da, db) if x != y)
    where = next((f" ({name})" for off, sz, name in pe_fields(da) if off <= n < off + sz), "")
    return f"{size}first differing byte at offset {n:#x}{where}; {diffs:,} differing bytes in all"


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    A, B = Path(sys.argv[1]), Path(sys.argv[2])
    ma, mb = read_manifest(A), read_manifest(B)
    first = None
    for stage in STAGES:
        a, b = ma[stage], mb[stage]
        same = sum(1 for p in a if b.get(p) == a[p])
        only = sorted(set(a) ^ set(b))
        diff = sorted(p for p in a if p in b and a[p] != b[p])
        print(f"{stage:7} {same:6} identical, {len(diff):5} different, {len(only):3} in one build only"
              + (f"  (e.g. {only[0]})" if only else ""))
        if first is None and (diff or only):
            first = (stage, diff[0] if diff else only[0], bool(diff))
    if first is None:
        print("every artifact is identical")
        return 0
    stage, path, both = first
    print(f"first divergent artifact: {stage} {path}")
    if both:
        print("  " + explain(A / path, B / path))
    return 1


if __name__ == "__main__":
    sys.exit(main())
