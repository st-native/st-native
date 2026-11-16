# SPDX-License-Identifier: GPL-3.0-or-later
"""Nintendo DS ROM reading: header, identification, ARM9/ARM7 static modules, ARM9 overlays, NitroSDK module params.

  python -m tools.re.ndsrom info <rom.nds>
  python -m tools.re.ndsrom extract <rom.nds> <out_dir>     # decompressed modules + manifest.json

Only facts about the ROM (hashes, addresses, sizes, version strings) are printed or written to the manifest. The
extracted module binaries are game data when the ROM is a game: they are written only where you point, never into
the repository (the guard refuses them).
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

from . import blz

MODULE_PARAMS_MAGIC = struct.pack("<II", 0xDEC00621, 0x2106C0DE)
# Game codes known to the project (facts: header IDs).
KNOWN_GAME_CODES = {"BKIE": "Spirit Tracks (USA)", "BKIP": "Spirit Tracks (Europe)", "BKIJ": "Spirit Tracks (Japan)"}
# Whole-file SHA-1 of dumps we have verified ourselves (hashes only; never game data).
KNOWN_DUMPS = {
    "1f0921b5e99db9d92d24710f1b004e4f3c997b73": "Spirit Tracks (USA, Australia) Rev 0 [BKIE] - verified 2026-11-12",
}


class RomError(ValueError):
    pass


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


@dataclass
class Header:
    title: str
    game_code: str
    maker_code: str
    revision: int
    arm9_rom_offset: int
    arm9_entry: int
    arm9_ram: int
    arm9_size: int
    arm7_rom_offset: int
    arm7_entry: int
    arm7_ram: int
    arm7_size: int
    fnt_offset: int
    fnt_size: int
    fat_offset: int
    fat_size: int
    ovt9_offset: int
    ovt9_size: int
    ovt7_offset: int
    ovt7_size: int
    header_crc: int
    header_crc_ok: bool

    @staticmethod
    def parse(rom: bytes) -> "Header":
        if len(rom) < 0x200:
            raise RomError("file too small for a DS header")
        title = rom[0:12].split(b"\0")[0].decode("ascii", "replace")
        game_code = rom[12:16].decode("ascii", "replace")
        maker = rom[16:18].decode("ascii", "replace")
        revision = rom[0x1E]
        a9 = struct.unpack_from("<4I", rom, 0x20)
        a7 = struct.unpack_from("<4I", rom, 0x30)
        fnt_fat = struct.unpack_from("<4I", rom, 0x40)
        ovt = struct.unpack_from("<4I", rom, 0x50)
        hcrc = struct.unpack_from("<H", rom, 0x15E)[0]
        return Header(title, game_code, maker, revision, *a9, *a7, *fnt_fat, *ovt, hcrc, crc16(rom[:0x15E]) == hcrc)


@dataclass
class Overlay:
    id: int
    ram: int
    ram_size: int
    bss_size: int
    sinit_start: int
    sinit_end: int
    file_id: int
    compressed: bool
    stored_size: int


@dataclass
class Module:
    name: str
    base: int
    size: int  # decompressed
    sha1: str  # of the decompressed bytes
    compressed: bool
    data: bytes = field(repr=False, default=b"")
    kind: str = "code"  # "code", or "data" for modules that hold no code (DTCM)
    bss: int = 0        # zero-initialised bytes that follow the module in memory (autoload blocks)


@dataclass
class RomInfo:
    sha1: str
    size: int
    header: Header
    known_title: str | None
    known_dump: str | None
    module_params: dict | None
    sdk_strings: list[str]
    modules: list[Module]
    overlays: list[Overlay]


def find_module_params(arm9: bytes, base: int) -> dict | None:
    i = arm9.find(MODULE_PARAMS_MAGIC)
    if i < 0x1C:
        return None
    words = struct.unpack_from("<7I", arm9, i - 0x1C)
    keys = ["autoload_list_start", "autoload_list_end", "autoload_start", "static_bss_start", "static_bss_end",
            "compressed_static_end", "sdk_version"]
    params = dict(zip(keys, words))
    params["address"] = base + i - 0x1C
    return params


def split_autoloads(arm9: bytes, base: int, params: dict | None) -> list[Module]:
    """The autoload blocks (ITCM code, DTCM data) stored after the static module, which the SDK copies to their own
    addresses at start-up. List entries are (ram address, size, bss size); the data starts at autoload_start."""
    if not params or params["autoload_list_end"] <= params["autoload_list_start"]:
        return []
    lst, src = params["autoload_list_start"] - base, params["autoload_start"] - base
    out = []
    for k in range((params["autoload_list_end"] - params["autoload_list_start"]) // 12):
        ram, size, bss = struct.unpack_from("<III", arm9, lst + 12 * k)
        data = arm9[src:src + size]
        if ram < 0x02000000:
            name, kind = "itcm", "code"
        elif 0x02700000 <= ram < 0x02800000:
            name, kind = "dtcm", "data"
        else:
            name, kind = f"autoload{k}", "code"
        out.append(Module(name, ram, len(data), hashlib.sha1(data).hexdigest(), False, data, kind, bss))
        src += size
    return out


def read_rom(path: Path) -> RomInfo:
    rom = path.read_bytes()
    h = Header.parse(rom)
    if not h.header_crc_ok:
        raise RomError("header CRC mismatch (not a DS ROM, or a damaged one)")
    modules: list[Module] = []

    arm9 = rom[h.arm9_rom_offset:h.arm9_rom_offset + h.arm9_size]
    params = find_module_params(arm9, h.arm9_ram)
    compressed = bool(params and params["compressed_static_end"])
    if compressed:
        n = params["compressed_static_end"] - h.arm9_ram
        arm9 = blz.decompress(arm9[:n]) + arm9[n:]
    autoloads = split_autoloads(arm9, h.arm9_ram, params)
    if autoloads:  # the static module proper ends where the autoload blocks begin
        arm9 = arm9[:params["autoload_start"] - h.arm9_ram]
    modules.append(Module("arm9", h.arm9_ram, len(arm9), hashlib.sha1(arm9).hexdigest(), compressed, arm9))
    modules += autoloads

    arm7 = rom[h.arm7_rom_offset:h.arm7_rom_offset + h.arm7_size]
    modules.append(Module("arm7", h.arm7_ram, len(arm7), hashlib.sha1(arm7).hexdigest(), False, arm7))

    overlays: list[Overlay] = []
    for k in range(h.ovt9_size // 32):
        e = struct.unpack_from("<8I", rom, h.ovt9_offset + 32 * k)
        ov = Overlay(e[0], e[1], e[2], e[3], e[4], e[5], e[6], bool(e[7] >> 24 & 1), e[7] & 0xFFFFFF)
        start, end = struct.unpack_from("<II", rom, h.fat_offset + 8 * ov.file_id)
        data = rom[start:end]
        if ov.compressed:
            data = blz.decompress(data)
        overlays.append(ov)
        modules.append(Module(f"ov{ov.id:03d}", ov.ram, len(data), hashlib.sha1(data).hexdigest(), ov.compressed,
                              data))

    sdk = sorted({m.decode("ascii") for m in re.findall(rb"\[SDK\+[ -~]{4,80}\]", b"".join(m.data for m in modules))})
    sha1 = hashlib.sha1(rom).hexdigest()
    return RomInfo(sha1, len(rom), h, KNOWN_GAME_CODES.get(h.game_code), KNOWN_DUMPS.get(sha1), params, sdk,
                   modules, overlays)


def manifest(info: RomInfo) -> dict:
    d = asdict(info)
    for m in d["modules"]:
        m.pop("data", None)
    return d


def main(argv: list[str]) -> int:
    if len(argv) < 2 or argv[0] not in ("info", "extract"):
        print(__doc__)
        return 2
    info = read_rom(Path(argv[1]))
    if argv[0] == "info":
        print(json.dumps(manifest(info), indent=2))
        return 0
    out = Path(argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for m in info.modules:
        (out / f"{m.name}.bin").write_bytes(m.data)
    (out / "manifest.json").write_text(json.dumps(manifest(info), indent=2))
    print(f"{len(info.modules)} modules written to {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
