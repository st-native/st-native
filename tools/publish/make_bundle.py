#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble the publication bundle locally, for the maintainer's review (docs/PUBLISHING.md).

    python -I tools/publish/make_bundle.py [--out build/publish/st-native]

Copies only what an explicit allowlist names (the app, our Spirit Tracks hooks and config, the build/verification
tools, the engine's fetch script and patches, the synthetic tests and the documentation), removes the
"<!-- private:start --> ... <!-- private:end -->" blocks from Markdown files, runs the game-data guard over the result
and writes REVIEW.md next to the bundle (what is in, what was left out, what still needs a decision). It creates no
git repository and sends nothing anywhere.
"""
from __future__ import annotations

import argparse
import hashlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

INCLUDE = [
    "README.md", "AGENTS.md", "CLAUDE.md", "CONTRIBUTING.md", "LICENSE", "NOTICE.md", ".gitignore", ".gitattributes",
    ".github/workflows/", "Launch.cmd",
    "app/", "platform/windows/launcher.py", "platform/windows/launcher_worker.cpp",
    "platform/windows/launcher.rc", "platform/windows/launcher.ico",
    "game/st/config/", "game/st/hooks/",
    "tools/__init__.py", "tools/check_no_game_data.py", "tools/game_data_allowlist.txt",
    "tools/build/build_program.cmd", "tools/build/st_build.py", "tools/build/st_build.cmd",
    "tools/build/compare_builds.py", "tools/build/compare_runs.py", "tools/publish/",
    "tools/re/__init__.py", "tools/re/ndsrom.py", "tools/re/blz.py", "tools/re/discover.py", "tools/re/requirements.txt",
    "tests/",
    "spike/m1a/",
    "poc/sample/", "poc/fetch_tools.py", "poc/tools.lock.json",
    "docs/BUILDING.md", "docs/ROADMAP.md", "docs/RISKS.md", "docs/PUBLISHING.md",
    "docs/architecture/", "docs/design/",
    "docs/research/M1A_SPIKE.md", "docs/research/M1B_FIRST_BOOT.md", "docs/research/DSPROTECT_ORIGINAL_PATH.md",
    "docs/research/EXISTING_PROJECTS.md", "docs/research/PLATFORM_PORTABILITY.md",
]
SKIP_PARTS = {"__pycache__", "results", ".git"}
SKIP_SUFFIXES = {".pyc"}
PRIVATE = re.compile(r"<!-- private:start.*?<!-- private:end -->\n?", re.S)
# Kept private (AGENTS.md "Historical research"); listed in REVIEW.md so nothing is left out silently.
PRIVATE_NOTE = {
    "game/st (src, include, manifest, status, host)": "reconstructed game functions and their verification (historical, private)",
    "game/st/runtime, tools/build/st_recon.*": "R4 reconstruction bridge and tools (historical, private)",
    "tools/re (other than ndsrom, blz, discover)": "reconstruction harness and disassembly tools (historical, private)",
    "poc (other than sample, fetch_tools)": "R0 reconstruction proof of concept (historical, private)",
    "docs/research R0-R4 reports, SPIRIT_TRACKS_ANALYSIS, HISTORICAL_RESEARCH": "quote game addresses, behaviour and disassembly",
    "docs/legal, reports, research_notes": "legal analysis: the maintainer decides what to publish, after counsel",
    "docs/REPOSITORY_PLAN.md": "superseded by docs/PUBLISHING.md (GitHub plan)",
    "build, poc/out, spike/m1a/results, .tools, .venv": "derived from the ROM, third-party downloads, or local results",
}


def files_of(entry: str) -> list[Path]:
    p = ROOT / entry
    if not p.exists():
        sys.exit(f"allowlisted path missing: {entry}")
    items = [p] if p.is_file() else sorted(q for q in p.rglob("*") if q.is_file())
    return [q for q in items if not (set(q.relative_to(ROOT).parts) & SKIP_PARTS) and q.suffix not in SKIP_SUFFIXES]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=ROOT / "build" / "publish" / "st-native")
    a = ap.parse_args()
    out = a.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    copied, stripped = [], []
    for entry in INCLUDE:
        for src in files_of(entry):
            rel = src.relative_to(ROOT)
            dst = out / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            if src.suffix == ".md":
                text = src.read_text(encoding="utf-8")
                cleaned = PRIVATE.sub("", text)
                if cleaned != text:
                    stripped.append(rel.as_posix())
                dst.write_bytes(cleaned.encode("utf-8"))
            else:
                shutil.copyfile(src, dst)
            copied.append(rel.as_posix())
    guard = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "check_no_game_data.py"), "--tree", str(out)],
                           capture_output=True, text=True)
    manifest = "".join(f"{hashlib.sha256((out / f).read_bytes()).hexdigest()}  {f}\n" for f in sorted(copied))
    (out.parent / "BUNDLE_MANIFEST.txt").write_text(manifest, encoding="utf-8", newline="\n")
    review = [
        "# Source publication bundle: review before uploading\n\n",
        f"Bundle: `{out}`, {len(copied)} files ({sum((out / f).stat().st_size for f in copied):,} bytes). SHA-256 of "
        "each: `BUNDLE_MANIFEST.txt`.\n\n",
        "## Game-data guard\n\n```\n" + (guard.stdout + guard.stderr).strip() + "\n```\n\n",
        "## Private blocks removed from\n\n" + "".join(f"- `{s}`\n" for s in stripped) + "\n",
        "## Left out on purpose\n\n| What | Why |\n|---|---|\n" + "".join(f"| {k} | {v} |\n" for k, v in PRIVATE_NOTE.items()),
        "\n## Top-level folders\n\n" + "".join(f"- `{d}/`: {sum(1 for f in copied if f.startswith(d + '/'))} files\n"
                                             for d in sorted({f.split('/')[0] for f in copied if '/' in f})),
        "\nOpen decisions: docs/PUBLISHING.md section 4.\n",
    ]
    (out.parent / "REVIEW.md").write_text("".join(review), encoding="utf-8", newline="\n")
    print(f"{len(copied)} files -> {out}")
    print((guard.stdout + guard.stderr).strip())
    print(f"review notes: {out.parent / 'REVIEW.md'}")
    return guard.returncode


if __name__ == "__main__":
    sys.exit(main())
