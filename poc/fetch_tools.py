#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Download the third-party reverse-engineering tools used by the proof of concept, pinned by version.

Each tool goes into its own directory under .tools/ (git-ignored). SHA-256 digests are recorded in
poc/tools.lock.json on first download and checked on every later run, so a changed upstream file is noticed.
Nothing here is redistributed by the project; every tool keeps its own licence (see TOOLS table).

  python poc/fetch_tools.py [--only dsd,objdiff,m2c,ghidra]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS_DIR = ROOT / ".tools"
LOCK = Path(__file__).resolve().parent / "tools.lock.json"

# name: (version, kind, source, licence)
TOOLS = {
    "dsd": ("v0.12.1", "file",
            "https://github.com/AetiasHax/ds-decomp/releases/download/v0.12.1/dsd-windows-x86_64.exe", "MIT"),
    "objdiff": ("v3.8.3", "file",
                "https://github.com/encounter/objdiff/releases/download/v3.8.3/objdiff-cli-windows-x86_64.exe",
                "MIT OR Apache-2.0"),
    "m2c": ("git", "git", "https://github.com/matt-kempster/m2c.git", "GPL-3.0"),
    "ghidra": ("12.2", "zip",
               "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.2_build/"
               "ghidra_12.2_PUBLIC_20261016.zip", "Apache-2.0"),
}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="", help="comma-separated subset of: " + ",".join(TOOLS))
    args = ap.parse_args()
    wanted = [t for t in args.only.split(",") if t] or list(TOOLS)
    lock = json.loads(LOCK.read_text()) if LOCK.exists() else {}

    for name in wanted:
        version, kind, url, licence = TOOLS[name]
        dest = TOOLS_DIR / f"{name}-{version}"
        dest.mkdir(parents=True, exist_ok=True)
        if kind == "git":
            if not (dest / ".git").exists():
                subprocess.run(["git", "clone", "--quiet", "--depth", "1", url, str(dest)], check=True)
            rev = subprocess.run(["git", "-C", str(dest), "rev-parse", "HEAD"], capture_output=True, text=True,
                                 check=True).stdout.strip()
            prev = lock.get(name, {}).get("commit")
            lock[name] = {"version": version, "commit": prev or rev, "url": url, "licence": licence}
            print(f"{name}: {url} @ {rev}" + (f" (lock pins {prev})" if prev and prev != rev else ""))
            continue
        file = dest / url.rsplit("/", 1)[1]
        if not file.exists():
            print(f"{name}: downloading {url}")
            urllib.request.urlretrieve(url, file)
        digest = sha256(file)
        pinned = lock.get(name, {}).get("sha256")
        if pinned and pinned != digest:
            print(f"{name}: SHA-256 mismatch: expected {pinned}, got {digest}", file=sys.stderr)
            return 1
        lock[name] = {"version": version, "sha256": digest, "url": url, "licence": licence}
        if kind == "zip" and not any(p.is_dir() for p in dest.iterdir()):
            with zipfile.ZipFile(file) as z:
                z.extractall(dest)
        print(f"{name} {version}: {file.name} sha256 {digest}")

    LOCK.write_text(json.dumps(lock, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
