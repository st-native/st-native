#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""M1a V3 blocker, minimal reproduction: the engine's difftest DLL crashes on its first call.

One function, `<insn> ; bx lr` (default `mov r0, #1`), recompiled with ntr-recomp --raw and built exactly as
tests/difftest/difftest.py builds it (clang -shared -O1 stub.c funcs_000.c func_table.c), then called once through
dt_run. Prints the generated C and where the call ends.

  python spike/m1a/v3_min_repro.py [--engine .tools/upstream/ffcc] [--out spike/m1a/results/v3_repro]
Run inside a VS x64 developer environment (vcvars64) with LLVM on PATH (spike/m1a/run_v3_windows.cmd sets both).
"""
from __future__ import annotations

import argparse
import ctypes
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", type=Path, default=ROOT / ".tools" / "upstream" / "ffcc")
    ap.add_argument("--out", type=Path, default=ROOT / "spike" / "m1a" / "results" / "v3_repro")
    ap.add_argument("--define", action="append", default=[], help="extra -D for the DLL build (diagnosis)")
    ap.add_argument("--insn", default="e3a00001", help="ARM instruction (hex) before the bx lr")
    ap.add_argument("--lr", default="0x11111111", help="initial lr")
    ap.add_argument("--r12", default="0x11111111", help="initial r12")
    a = ap.parse_args()
    eng, out = a.engine.resolve(), a.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    (out / "gen").mkdir(parents=True)
    base = 0x08000000
    blob = struct.pack("<II", int(a.insn, 16), 0xE12FFF1E) + b"\0" * (0x1000 - 8)  # <insn> ; bx lr
    (out / "blob.bin").write_bytes(blob)
    (out / "funcs.txt").write_text(f"{base:08x} a\n")
    recomp = eng / "build" / "release" / "recompiler" / "ntr-recomp.exe"
    cmd = [str(recomp), "--raw", str(out / "blob.bin"), "--raw-base", f"{base:x}", "--raw-funcs",
           str(out / "funcs.txt"), "--out", str(out / "gen")]
    print("$", " ".join(cmd))
    r = subprocess.run(cmd, capture_output=True, text=True)
    print("ntr-recomp exit", r.returncode, (r.stdout + r.stderr).strip()[:400])
    for f in sorted((out / "gen").iterdir()):
        print(f"--- {f.name} ({f.stat().st_size} bytes)")
    print((out / "gen" / "funcs_000.c").read_text()[:2500])
    cc = shutil.which("clang") or r"C:/Program Files/LLVM/bin/clang.exe"
    dll = out / "difftest.dll"
    cmd = [cc, "-shared", "-O1", "-w", "-o", str(dll), "-I", str(eng / "runtime" / "include"), "-I", str(out / "gen"),
           *[f"-D{d}" for d in a.define], str(eng / "tests" / "difftest" / "stub.c"), str(out / "gen" / "funcs_000.c"),
           str(out / "gen" / "func_table.c")]
    print("$", " ".join(cmd))
    r = subprocess.run(cmd, capture_output=True, text=True)
    print("clang exit", r.returncode, (r.stdout + r.stderr).strip()[:800])
    if r.returncode:
        return 2
    d = ctypes.CDLL(str(dll))
    code = ctypes.create_string_buffer(blob, len(blob))
    d.dt_set_code(code, base, len(blob))
    init = [0x11111111] * 15
    init[12], init[14] = int(a.r12, 0), int(a.lr, 0)
    regs = (ctypes.c_uint32 * 16)(*init, 0)
    flags = (ctypes.c_uint32 * 5)(0, 0, 0, 0, 0)
    print("calling dt_run ...", flush=True)
    rc = d.dt_run(base, regs, flags)
    print("dt_run returned", rc, "r0 =", hex(regs[0]), "lr =", hex(regs[14]), "calls", d.dt_calls())
    return 0


if __name__ == "__main__":
    sys.exit(main())
