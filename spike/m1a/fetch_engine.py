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
import os
import tempfile
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


def apply_patch_stack(dest: Path, patches: list[Path]) -> None:
    """Match the complete applied prefix, including patches that edit earlier patches' hunks.

    The temporary index validates expected trees without staging or overwriting local files.
    A patch-by-patch reverse check cannot recognize an older patch after a later one edits it.
    """
    with tempfile.TemporaryDirectory(prefix="st-native-patches-") as directory:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(directory) / "index"))
        def index_git(*args: str) -> subprocess.CompletedProcess[str]:
            result = subprocess.run(["git", *args], cwd=dest, env=env, capture_output=True, text=True)
            if result.returncode:
                raise SystemExit(f"patch stack check failed: {result.stderr.strip()}")
            return result
        def matches() -> bool:
            result = subprocess.run(["git", "diff", "--quiet", "--ignore-submodules=dirty"],
                                    cwd=dest, env=env, capture_output=True)
            if result.returncode not in (0, 1):
                raise SystemExit("cannot compare the engine with the expected patch stack")
            return result.returncode == 0
        index_git("read-tree", "HEAD")
        for patch in patches:
            index_git("apply", "--cached", str(patch.resolve()))
        final_files = set(index_git("ls-files").stdout.splitlines())
        prefix = len(patches)
        while not matches():
            if prefix == 0:
                raise SystemExit("engine edits do not match a known patch prefix; preserve them and create a patch first")
            prefix -= 1
            index_git("apply", "--cached", "--reverse", str(patches[prefix].resolve()))
        prefix_files = set(index_git("ls-files").stdout.splitlines())
        for name in final_files - prefix_files:
            target = dest / name
            if target.exists() or target.is_symlink():
                raise SystemExit(f"untracked file blocks the pending patch stack: {name}")
        for i, patch in enumerate(patches):
            if i < prefix:
                print(f"{patch.name}: already applied")
            else:
                git("apply", str(patch.resolve()), cwd=dest)
                print(f"{patch.name}: applied")


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
    apply_patch_stack(a.dest, sorted((HERE / "patches").glob("*.patch")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
