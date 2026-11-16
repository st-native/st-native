#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the synthetic sample: generate C, compile it for the DS's ARM946E-S, link, and pack a synthetic ROM.

Outputs (poc/out/, git-ignored):
  sample.nds            the "original" the pipeline works on (stands in for the user's game dump)
  truth/                ground truth for scoring only: generated sources, ELF files with symbols, build flags.
                        The reconstruction work must never read it.

  python poc/sample/build_sample.py [--seed 1]
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT))

from elftools.elf.elffile import ELFFile  # noqa: E402

from poc.sample.gen_sample import Gen  # noqa: E402
from poc.sample.mkrom import build_rom  # noqa: E402

LLVM = Path(os.environ.get("LLVM_BIN", r"C:\Program Files\LLVM\bin"))
CLANG, LLD, OBJCOPY = LLVM / "clang.exe", LLVM / "ld.lld.exe", LLVM / "llvm-objcopy.exe"
CFLAGS = ["--target=armv5te-none-eabi", "-mcpu=arm946e-s", "-mfloat-abi=soft", "-O2", "-ffreestanding",
          "-fno-builtin", "-fwrapv", "-ffunction-sections", "-fdata-sections", "-fno-unwind-tables",
          "-fno-asynchronous-unwind-tables", "-fno-exceptions", "-fno-common", "-c"]
MODE_FLAGS = {"rt.c": ["-marm"], "main_arm.c": ["-marm"], "start.c": ["-marm"], "data.c": ["-marm"],
              "main_thumb.c": ["-mthumb", "-fno-jump-tables"], "ovl_thumb.c": ["-mthumb", "-fno-jump-tables"]}


def run(cmd: list) -> None:
    subprocess.run([str(c) for c in cmd], check=True)


def sym(elf_path: Path, name: str) -> int:
    with elf_path.open("rb") as f:
        s = ELFFile(f).get_section_by_name(".symtab").get_symbol_by_name(name)
        return s[0]["st_value"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    out = ROOT / "poc" / "out"
    truth = out / "truth"
    # start clean, but never touch poc/out/st (the real-ROM analysis of run_rom.py)
    for child in (out.iterdir() if out.exists() else []):
        if child.name == "st":
            continue
        shutil.rmtree(child) if child.is_dir() else child.unlink()
    src, obj = truth / "src", truth / "obj"
    src.mkdir(parents=True)
    obj.mkdir(parents=True)

    for name, text in Gen(a.seed).run(60, 60, 30).items():
        (src / name).write_text(text)
    run([CLANG, "--target=armv5te-none-eabi", "-mcpu=arm946e-s", "-c", HERE / "crt0.s", "-o", obj / "crt0.o"])
    shutil.copy(HERE / "rt.c", src / "rt.c")
    for c in MODE_FLAGS:
        run([CLANG, *CFLAGS, *MODE_FLAGS[c], "-I", src, src / c, "-o", obj / (c[:-2] + ".o")])

    main_elf, ovl_elf = truth / "main.elf", truth / "ovl000.elf"
    run([LLD, "-T", HERE / "main.ld", "-o", main_elf, obj / "crt0.o", obj / "start.o", obj / "main_arm.o",
         obj / "main_thumb.o", obj / "data.o", obj / "rt.o"])
    run([LLD, "-T", HERE / "ovl.ld", f"--just-symbols={main_elf}", "-o", ovl_elf, obj / "ovl_thumb.o"])
    run([OBJCOPY, "-O", "binary", "--only-section=.text", "--only-section=.rodata", "--only-section=.data", main_elf,
         out / "main.bin"])
    run([OBJCOPY, "-O", "binary", "--only-section=.text", "--only-section=.rodata", "--only-section=.data", ovl_elf,
         out / "ovl000.bin"])

    arm7 = bytes.fromhex("FEFFFFEA")  # b . (an idle ARM7: the sample has no ARM7 program)
    rom, arm9 = build_rom((out / "main.bin").read_bytes(), 0x02000000, sym(main_elf, "_start"), arm7, 0x02380000,
                          [(0, 0x02200000, (out / "ovl000.bin").read_bytes(), 0)])
    (out / "sample.nds").write_bytes(rom)
    (truth / "arm9_expected.bin").write_bytes(arm9)
    (out / "main.bin").unlink()
    (out / "ovl000.bin").replace(truth / "ovl000_expected.bin")
    (truth / "build.json").write_text(json.dumps({
        "seed": a.seed, "compiler": "clang 22.1.8", "cflags": CFLAGS, "mode_flags": MODE_FLAGS,
        "link": ["main.ld", "ovl.ld"]}, indent=2))
    print(f"sample.nds: {len(rom)} bytes; ground truth in {truth}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
