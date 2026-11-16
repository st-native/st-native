#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""M1a: the synthetic cartridge as the engine accepts it.

The engine's ROM parser (common/src/rom.cpp) and so both ntr-recomp and the runtime refuse an image whose header
lacks the Nintendo logo checksum (0xCF56 at 0x15C). Our synthetic ROM (poc/out/sample.nds) has neither the logo nor
its checksum. This writes only the 2-byte checksum value, never the logo bitmap, and recomputes the header CRC; the
rest of the image is unchanged.

  python spike/m1a/zpoc_rom.py [poc/out/sample.nds] [spike/m1a/results/zpoc/zpoc.nds]
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.re.ndsrom import crc16  # noqa: E402


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "poc" / "out" / "sample.nds"
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "spike" / "m1a" / "results" / "zpoc" / "zpoc.nds"
    rom = bytearray(src.read_bytes())
    struct.pack_into("<H", rom, 0x15C, 0xCF56)
    struct.pack_into("<H", rom, 0x15E, crc16(bytes(rom[:0x15E])))
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(rom)
    print(f"{dst} ({len(rom)} bytes) sha1 {hashlib.sha1(rom).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
