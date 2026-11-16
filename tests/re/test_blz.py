# SPDX-License-Identifier: GPL-3.0-or-later
"""BLZ codec round trips on generated data (no game data)."""
import os
import random
import unittest

from tools.re import blz


class BlzTest(unittest.TestCase):
    def roundtrip(self, data: bytes, prefix: int = 0) -> None:
        self.assertEqual(blz.decompress(blz.compress(data, prefix)), data)

    def test_repetitive(self):
        self.roundtrip(b"ABCD" * 5000)

    def test_mixed(self):
        # code-like data: common instruction words mixed with random ones
        r = random.Random(7)
        for n in (400, 4097, 20000):
            words = [r.choice([b"\x00\x00\xa0\xe3", b"\x1e\xff\x2f\xe1", b"\x04\xe0\x2d\xe5", os.urandom(4)])
                     for _ in range(n // 4)]
            self.roundtrip(b"".join(words), prefix=r.choice([0, 4, 0x40]))

    def test_incompressible_is_refused(self):
        with self.assertRaises(blz.BlzError):
            blz.compress(os.urandom(64))

    def test_long_runs_and_overlap(self):
        self.roundtrip(b"\x00" * 70000 + b"x" + b"\x00" * 7)

    def test_rejects_garbage(self):
        with self.assertRaises(blz.BlzError):
            blz.decompress(b"\x00" * 4)


if __name__ == "__main__":
    unittest.main()
