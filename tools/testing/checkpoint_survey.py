#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only checkpoint bootstrap, not encounter acceptance. Requires local ROM/module/saves.

python -I tools/testing/checkpoint_survey.py --rom <own.nds> --module <own.dll> \
  --saves <local-collection-directory> --case FL02:3:2 --case BO02:8:2 --out build/survey-01

Case syntax: register ID : SavesNN directory number : slot. Outputs stay below build/.
No screenshots, save edits, game text, disassembly or protected-code tracing are requested.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def parse_case(value: str) -> tuple[str, int, int]:
    match = re.fullmatch(r"([A-Za-z][A-Za-z0-9_-]{0,39}):([0-9]{1,2}):([12])", value)
    if not match or not 1 <= int(match[2]) <= 23:
        raise ValueError("case must be ID:1..23:1..2 (for example FL02:3:2)")
    return match[1], int(match[2]), int(match[3])


def output_path(root: Path, value: Path) -> Path:
    path = value.resolve()
    if not path.is_relative_to((root / "build").resolve()) or path == (root / "build").resolve():
        raise ValueError("survey output must be a subdirectory of this checkout's build/")
    if path.exists() and (not path.is_dir() or any(path.iterdir())):
        raise ValueError("choose a new or empty output directory; existing results are not overwritten")
    return path


def load_script(slot: int, frames: int) -> str:
    if slot not in (1, 2) or frames < 2200:
        raise ValueError("slot must be 1/2; at least 2200 frames are required for this load recipe")
    return ("# Own input: bootstrap only, no encounter-success claim.\n"
            f"1000 press START\n1100 touch 128 120\n1300 touch 130 {62 if slot == 1 else 110}\n"
            f"1500 touch 128 118\n1700 touch 130 91\n{frames} quit\n")


def summarize_profile(path: Path, start: int = 2000) -> dict:
    required = {"frame", "capture", "swap", "scene", "perspective_positive", "perspective_negative"}
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("incomplete graphics profile")
        frames, samples, capture, swap, off_axis, scenes = 0, 0, 0, 0, 0, set()
        for row in reader:
            values = {key: int(row[key]) for key in required}
            if values["frame"] != frames + 1 or values["capture"] not in (0, 1) or values["swap"] not in (0, 1):
                raise ValueError("graphics profile is not a dense sequential frame series")
            frames += 1
            if frames < start:
                continue
            samples += 1
            capture += values["capture"]
            swap += values["swap"]
            off_axis += bool(values["perspective_positive"] + values["perspective_negative"])
            scenes.add(values["scene"])
    return dict(frames=frames, sample_start=start, samples=samples, capture_samples=capture,
                swap_samples=swap, off_axis_samples=off_axis, existing_profile_ids=sorted(scenes))


def digest(path: Path) -> bytes:
    return hashlib.sha256(path.read_bytes()).digest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("rom", "module", "saves", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--case", action="append", required=True)
    parser.add_argument("--frames", type=int, default=3000)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    try:
        cases = [parse_case(case) for case in args.case]
        if len({case[0].casefold() for case in cases}) != len(cases):
            raise ValueError("case IDs must be unique, including on case-insensitive filesystems")
        load_script(1, args.frames)
        if args.timeout <= 0:
            raise ValueError("timeout must be positive")
        out = output_path(ROOT, args.out)
        for path in (args.rom, args.module):
            if not path.is_file():
                raise ValueError("ROM and module must be existing local files")
        saves = [(args.saves / f"Saves{number:02}" / "SPIRITTRACKS.sav").resolve() for _, number, _ in cases]
        if not all(path.is_file() for path in saves):
            raise ValueError("one or more requested save files are missing")
        host = ROOT / ".tools/upstream/ffcc/build/release/st_native.exe"
        if not host.is_file():
            raise ValueError("build the Windows host first with tools/build/build_program.cmd")
        out.mkdir(parents=True, exist_ok=True)
        results = []
        for (case_id, number, slot), save in zip(cases, saves):
            directory = out / case_id
            directory.mkdir()
            script = directory / "load.txt"
            script.write_text(load_script(slot, args.frames), encoding="utf-8", newline="\n")
            before = digest(save)
            command = [str(host), str(args.rom.resolve()), "--module", str(args.module.resolve()), "--save", str(save),
                       "--read-only-save", "--headless", "--script", str(script), "--screen-features", "0",
                       "--scene-profile", str(directory / "graphics.csv"), "--scene-stride", "1",
                       "--hashes", str(directory / "state.hashes"), "--wireless-group", "coverage-" + case_id]
            try:
                with (directory / "run.log").open("w", encoding="utf-8") as log:
                    run = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout,
                                         creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                graphics = summarize_profile(directory / "graphics.csv")
                unchanged = before == digest(save)
                passed = run.returncode == 0 and graphics["frames"] == args.frames and unchanged
                result = dict(id=case_id, save_number=number, slot=slot, bootstrap_passed=passed,
                              save_unchanged=unchanged, encounter_confirmed=False, presentation_accepted=False,
                              graphics=graphics)
            except (OSError, ValueError, subprocess.TimeoutExpired) as error:
                result = dict(id=case_id, bootstrap_passed=False, save_unchanged=before == digest(save),
                              encounter_confirmed=False, presentation_accepted=False, error=type(error).__name__)
            results.append(result)
            (out / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
            print(case_id, "bootstrap PASS" if result["bootstrap_passed"] else "bootstrap FAIL",
                  "(encounter and presentation remain unaccepted)", flush=True)
        return 0 if all(result["bootstrap_passed"] for result in results) else 1
    except ValueError as error:
        parser.error(str(error))
    return 2


if __name__ == "__main__":
    sys.exit(main())
