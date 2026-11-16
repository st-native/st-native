# SPDX-License-Identifier: GPL-3.0-or-later
"""Nintendo "backwards LZ" (BLZ, MI_UncompressBackward), used for DS ARM9 static modules and overlays.

Layout of a compressed file: [raw prefix][compressed stream][padding][footer]. The footer's last 8 bytes are
  u32: bits 0-23 = length of the compressed part counted back from the end (stream + padding + footer),
       bits 24-31 = footer length (padding + 8)
  u32: number of bytes the file grows by when decompressed.
The stream is read backwards: a flags byte (MSB first), then per flag either a literal byte or a two-byte reference
(read high byte first): length = (v >> 12) + 3, displacement = (v & 0xFFF) + 3, copying from higher addresses.

Decoder semantics follow FFCCEOT-Recomp/PH Recomp `common/src/blz.cpp` (GPL-3.0); this is an independent Python
implementation plus an encoder of our own for tests.
"""
from __future__ import annotations

import struct


class BlzError(ValueError):
    pass


def decompress(data: bytes, in_place: bool = True) -> bytes:
    """Decode. With `in_place` (the default), decode the way the console does, inside one buffer, and refuse a stream
    whose output would overwrite compressed bytes not yet read (real loaders decompress in place, so valid files
    never do this)."""
    if len(data) < 8:
        raise BlzError("input too small")
    enc_info, extra = struct.unpack_from("<II", data, len(data) - 8)
    footer_len, enc_len = enc_info >> 24, enc_info & 0xFFFFFF
    if enc_len > len(data) or footer_len > enc_len or footer_len < 8:
        raise BlzError("bad footer")
    out = bytearray(data) + bytearray(extra)
    inp = out if in_place else bytes(data)
    src, dst, end = len(data) - footer_len, len(out), len(data) - enc_len
    while src > end:
        src -= 1
        flags = inp[src]
        for _ in range(8):
            if src <= end:
                break
            if flags & 0x80:
                if src < end + 2:
                    raise BlzError("truncated reference")
                src -= 2
                v = inp[src] | (inp[src + 1] << 8)
                disp, length = (v & 0x0FFF) + 3, (v >> 12) + 3
                if length > dst - end:
                    raise BlzError("output underflow")
                for _ in range(length):
                    dst -= 1
                    if dst + disp >= len(out):
                        raise BlzError("bad displacement")
                    out[dst] = out[dst + disp]
            else:
                if dst <= end:
                    raise BlzError("output underflow")
                dst -= 1
                src -= 1
                out[dst] = inp[src]
            if in_place and dst < src:
                raise BlzError(f"output overtakes unread input at {dst:#x} (not decodable in place)")
            flags = (flags << 1) & 0xFF
    if dst != end:
        raise BlzError(f"stream ended early ({dst - end} bytes not produced)")
    return bytes(out)


def compress(data: bytes, prefix: int = 0) -> bytes:
    """Greedy encoder (not Nintendo's). At least `prefix` bytes at the start stay uncompressed; more are left raw if
    needed so that the result decompresses in place, as the console requires."""
    while True:
        out = _compress_once(data, prefix)
        try:
            decompress(out, in_place=True)
            return out
        except BlzError as e:
            if "overtakes" not in str(e):
                raise
            prefix = min(len(data), prefix + max(4, (len(data) - prefix) // 16)) & ~3


def _compress_once(data: bytes, prefix: int) -> bytes:
    body = data[prefix:][::-1]  # work on the reversed body: references then point backwards, as in plain LZ77
    n = len(body)
    stream = bytearray()
    i = 0
    index: dict[bytes, list[int]] = {}
    while i < n:
        flags_pos = len(stream)
        stream.append(0)
        flags = 0
        for bit in range(8):
            if i >= n:
                break
            best_len, best_disp = 0, 0
            key = bytes(body[i:i + 3])
            if len(key) == 3:
                for j in reversed(index.get(key, [])):
                    d = i - j
                    if d > 0x1002:
                        break
                    if d < 3:
                        continue
                    k = 0
                    # length <= distance: no overlapping copies (ds-rom/dsd refuse them; Nintendo's tool avoids them)
                    while k < 18 and k < d and i + k < n and body[j + k] == body[i + k]:
                        k += 1
                    if k > best_len:
                        best_len, best_disp = k, d
                        if k == 18:
                            break
            if best_len >= 3:
                v = ((best_len - 3) << 12) | (best_disp - 3)
                stream += bytes((v >> 8, v & 0xFF))  # high byte first in reversed order
                flags |= 0x80 >> bit
                step = best_len
            else:
                stream.append(body[i])
                step = 1
            for p in range(i, min(i + step, n - 2)):
                index.setdefault(bytes(body[p:p + 3]), []).append(p)
            i += step
        stream[flags_pos] = flags
    comp = bytes(stream[::-1])
    pad = (-(prefix + len(comp))) % 4
    footer_len = 8 + pad
    enc_len = len(comp) + footer_len
    total = prefix + enc_len
    extra = len(data) - total
    if extra < 0:
        raise BlzError("data does not compress")
    return data[:prefix] + comp + b"\xFF" * pad + struct.pack("<II", (footer_len << 24) | enc_len, extra)
