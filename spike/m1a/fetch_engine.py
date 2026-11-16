#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""M1a: fetch the recompilation base at the exact revisions in spike/m1a/deps.lock.json and apply our patches.

  python spike/m1a/fetch_engine.py [--dest .tools/upstream/ffcc] [--no-submodules]

Clones the engine, checks out the pinned commit, initialises the melonDS submodule (the engine pins it) and verifies
it is at the locked commit, then applies spike/m1a/patches/*.patch (see the patch headers and docs). Fails loudly if
any revision differs from the lock file. Third-party code stays outside the project tree's tracked files.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def git(*args: str, cwd: Path | None = None) -> str:
    r = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"git {' '.join(args)} failed:\n{r.stderr.strip()}")
    return r.stdout.strip()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dest", type=Path, default=ROOT / ".tools" / "upstream" / "ffcc")
    ap.add_argument("--no-submodules", action="store_true", help="skip melonDS (enough for the recompiler alone)")
    a = ap.parse_args()
    lock = json.loads((HERE / "deps.lock.json").read_text())
    eng = lock["engine"]
    if not a.dest.exists():
        git("clone", "--quiet", eng["repo"], str(a.dest))
    git("checkout", "--quiet", eng["commit"], cwd=a.dest)
    head = git("rev-parse", "HEAD", cwd=a.dest)
    if head != eng["commit"]:
        raise SystemExit(f"engine at {head}, lock says {eng['commit']}")
    print(f"engine {head}")
    if not a.no_submodules:
        for path, sub in lock["submodules"].items():
            git("submodule", "update", "--init", "--quiet", path, cwd=a.dest)
            got = git("rev-parse", "HEAD", cwd=a.dest / path)
            if got != sub["commit"]:
                raise SystemExit(f"{path} at {got}, lock says {sub['commit']}")
            print(f"{path} {got}")
    for patch in sorted((HERE / "patches").glob("*.patch")):
        if subprocess.run(["git", "apply", "--reverse", "--check", str(patch)], cwd=a.dest,
                          capture_output=True).returncode == 0:
            print(f"{patch.name}: already applied")
            continue
        git("apply", str(patch), cwd=a.dest)
        print(f"{patch.name}: applied")
    return 0


if __name__ == "__main__":
    sys.exit(main())
