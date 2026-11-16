#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""M1a: assemble the private CI repository from the project (the project stays the source of truth).

Copies spike/m1a (without results/), the line-ending rules and the workflow into <dest>, writes its README and
.gitignore, and refuses to continue if anything outside that set, or anything that looks like game data, would be
included.

  python spike/m1a/ci/make_ci_repo.py [--dest .tools/ci/st-native-m1a-spike]
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SPIKE = ROOT / "spike" / "m1a"
FIXTURE_SHA1 = "faacbe2826041d1d0029943793127d08505a661f"  # our synthetic cartridge (poc/sample + logo checksum only)

README = """# st-native M1a spike (private)

Continuous integration for the M1a portability spike of the st-native project (working name): it builds a
recompilation base for Nintendo DS programs (FFCCEOT-Recomp's ntr-recomp / ntr-runtime, at the commits in
`spike/m1a/deps.lock.json`) and runs checks V1-V3 on Linux x64/arm64, macOS arm64 and Android arm64.

- No game code or data. `spike/m1a/fixtures/zpoc_synthetic.rom` is a synthetic program of our own (generated C, built
  with clang 22.1.8 for the ARM946E-S); its header carries the 2-byte logo checksum the engine's ROM parser requires,
  not the logo. `zpoc_hashes_windows_x64.txt` is its per-frame state hashes on Windows x64, the reference.
- Third-party code is fetched at run time by `spike/m1a/fetch_engine.py`; `spike/m1a/patches/` are our changes to it.
- Licence: GPL-3.0-or-later (SPDX headers in each file).
"""

GITIGNORE = "spike/m1a/results/\n__pycache__/\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dest", type=Path, default=ROOT / ".tools" / "ci" / "st-native-m1a-spike")
    a = ap.parse_args()
    dest = a.dest
    dest.mkdir(parents=True, exist_ok=True)
    target = dest / "spike" / "m1a"
    if target.exists():
        shutil.rmtree(target)
    files = [p for p in SPIKE.rglob("*") if p.is_file() and "results" not in p.relative_to(SPIKE).parts
             and "__pycache__" not in p.parts]
    for p in files:
        rel = p.relative_to(ROOT)
        if p.suffix.lower() in (".nds", ".srl", ".sav", ".dsv"):
            raise SystemExit(f"refusing to copy {rel}")
        if p.suffix == ".rom" and hashlib.sha1(p.read_bytes()).hexdigest() != FIXTURE_SHA1:
            raise SystemExit(f"{rel} is not the known synthetic fixture")
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, out)
    wf = dest / ".github" / "workflows"
    wf.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SPIKE / "ci" / "m1a.yml", wf / "m1a.yml")
    shutil.copy2(ROOT / ".gitattributes", dest / ".gitattributes")
    (dest / "README.md").write_bytes(README.encode())
    (dest / ".gitignore").write_bytes(GITIGNORE.encode())
    print(f"{len(files)} files from spike/m1a + workflow, README, .gitignore, .gitattributes -> {dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
