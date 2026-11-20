#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the ROM-free launcher smoke test's synthetic GL captures (no Pillow required).

Run st_launcher --smoke-test <existing empty output directory>, then:
    python -I tests/check_screen_composition.py <output directory>
Only the synthetic single-*.bmp fixtures produced by that test are expected here.
"""
from pathlib import Path
import struct
import sys


def load(path):
    raw = path.read_bytes()
    assert raw[:2] == b"BM", path.name
    offset = struct.unpack_from("<I", raw, 10)[0]
    width, signed_height = struct.unpack_from("<ii", raw, 18)
    bits = struct.unpack_from("<H", raw, 28)[0]
    assert width > 0 and signed_height and bits in (24, 32), path.name
    stride = ((width * bits + 31) // 32) * 4
    height = abs(signed_height)
    assert len(raw) >= offset + height * stride, path.name

    def pixel(x, y):
        row = height - 1 - y if signed_height > 0 else y
        pos = offset + row * stride + x * (bits // 8)
        return tuple(raw[pos:pos + 3][::-1])

    unit = min(width // 4, height // 3)
    return width, height, ((width - 4 * unit) // 2, (height - 3 * unit) // 2, 4 * unit, 3 * unit), pixel


def main():
    root = Path(sys.argv[1])
    top, touch = (0x31, 0x53, 0x74), (0x66, 0x84, 0x5c)
    for opacity in (50, 75, 100):
        width, height, (x, y, w, h), pixel = load(root / f"single-overlay-{opacity}.bmp")
        unit, margin = w * 20 // 400, max(2, w // 64)
        actual = pixel(x + w - 2 * unit - margin, y + margin + unit)
        expected = tuple(round(a * opacity / 100 + b * (1 - opacity / 100)) for a, b in zip(top, touch))
        assert all(abs(a - b) <= 1 for a, b in zip(actual, expected)), (opacity, actual, expected)
        assert pixel(width // 2, height // 2) == touch, opacity
    for name, expected in (("hidden", touch), ("peek", top), ("cinematic", (0x9f, 0x73, 0x50))):
        width, height, (x, y, w, h), pixel = load(root / f"single-{name}.bmp")
        assert pixel(width // 2, height // 2) == expected, name
        # Asymmetric markers detect a vertical flip, wrong atlas plane, and cross-plane sampling.
        assert pixel(x + w * 4 // 256, y + h * 4 // 192) == (0xdd, 0, 0x22), name
        assert pixel(x + w * 4 // 256, y + h * 188 // 192) == (0x22, 0xdd, 0), name
        if x:
            assert pixel(x - 1, height // 2) == (0, 0, 0), name
        if y:
            assert pixel(width // 2, y - 1) == (0, 0, 0), name
    print("Synthetic GL composition: opacity, atlas planes, orientation and aspect bars PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
