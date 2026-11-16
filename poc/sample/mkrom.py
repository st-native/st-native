# SPDX-License-Identifier: GPL-3.0-or-later
"""Pack the sample's modules into a synthetic DS ROM image (our own code only: no logo, no banner, no firmware data).

The image follows the DS cartridge layout closely enough to exercise the same extraction path as a real dump:
header with CRC16, ARM9 static module BLZ-compressed after its module parameters, ARM9 overlay table, FAT, a minimal
FNT, an ARM7 module, and one BLZ-compressed overlay file.
"""
from __future__ import annotations

import struct

from tools.re import blz
from tools.re.ndsrom import MODULE_PARAMS_MAGIC, crc16


def _align(buf: bytearray, n: int) -> None:
    buf += b"\xFF" * ((-len(buf)) % n)


def build_rom(arm9: bytes, arm9_ram: int, arm9_entry: int, arm7: bytes, arm7_ram: int,
              overlays: list[tuple[int, int, bytes, int]]) -> tuple[bytes, bytes]:
    """Returns (rom image, the ARM9 module as it is after decompression, with its compressed-end field set)."""
    arm9 = bytearray(arm9)
    p = arm9.find(MODULE_PARAMS_MAGIC)
    if p < 0x1C:
        raise ValueError("module params not found in the ARM9 module")
    params = p - 0x1C
    prefix = (p + 8 + 3) & ~3
    # Compress once to learn the size, write it into the params (inside the raw prefix), then compress again: the
    # prefix is stored raw, so the second result has the same length.
    comp = blz.compress(bytes(arm9), prefix)
    struct.pack_into("<I", arm9, params + 0x14, arm9_ram + len(comp))
    comp = blz.compress(bytes(arm9), prefix)

    rom = bytearray(0x4000)
    arm9_off = len(rom)
    rom += comp
    # The ARM9 footer SDK-built ROMs carry after the module: magic, offset of the module params, 0.
    rom += struct.pack("<III", 0xDEC00621, params, 0)
    _align(rom, 0x200)
    ovt_off = len(rom)
    ovt = bytearray()
    files: list[bytes] = []
    for fid, (ovid, ram, data, bss) in enumerate(overlays):
        c = blz.compress(data)
        files.append(c)
        ovt += struct.pack("<8I", ovid, ram, len(data), bss, 0, 0, fid, len(c) | (1 << 24))
    rom += ovt
    _align(rom, 0x200)
    fat = bytearray()
    for c in files:
        start = len(rom)
        rom += c
        fat += struct.pack("<II", start, len(rom))
        _align(rom, 0x200)
    arm7_off = len(rom)
    rom += arm7
    _align(rom, 0x200)
    fnt_off = len(rom)
    fnt = struct.pack("<IHH", 8, len(files), 1) + b"\0"  # root directory, no named files
    rom += fnt
    _align(rom, 0x200)
    fat_off = len(rom)
    rom += fat
    _align(rom, 0x200)
    # Banner version 1 (0x840 bytes): blank icon and palette, our own title in six languages, CRC16 over 0x20-0x83F.
    banner = bytearray(0x840)
    title = "Spirit Tracks Native\nproof of concept\nsynthetic sample".encode("utf-16-le")
    for lang in range(6):
        banner[0x240 + 0x100 * lang:0x240 + 0x100 * lang + len(title)] = title
    struct.pack_into("<HH", banner, 0, 1, crc16(bytes(banner[0x20:0x840])))
    banner_off = len(rom)
    rom += banner
    _align(rom, 0x200)

    h = bytearray(0x200)
    h[0:12] = b"STNATIVE POC"
    h[12:16] = b"ZPOC"
    h[16:18] = b"00"
    struct.pack_into("<4I", h, 0x20, arm9_off, arm9_entry, arm9_ram, len(comp))
    struct.pack_into("<4I", h, 0x30, arm7_off, arm7_ram, arm7_ram, len(arm7))
    struct.pack_into("<4I", h, 0x40, fnt_off, len(fnt), fat_off, len(fat))
    struct.pack_into("<4I", h, 0x50, ovt_off, len(ovt), 0, 0)
    struct.pack_into("<I", h, 0x68, banner_off)
    struct.pack_into("<II", h, 0x80, len(rom), 0x4000)
    struct.pack_into("<H", h, 0x15E, crc16(bytes(h[:0x15E])))
    rom[0:0x200] = h
    return bytes(rom), bytes(arm9)
