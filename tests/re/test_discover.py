# SPDX-License-Identifier: GPL-3.0-or-later
from importlib.util import find_spec
import unittest


@unittest.skipUnless(find_spec("capstone"), "function discovery requires Capstone")
class DiscoveryLookupTests(unittest.TestCase):
    def test_preferred_module_and_static_fallback_priority(self):
        from tools.re.discover import Discovery, Module

        current = Module("current", 8, bytes(8))
        other = Module("other", 16, bytes(20))
        itcm = Module("itcm", 12, bytes(12))
        arm9 = Module("arm9", 16, bytes(12))
        discovery = Discovery([current, other, itcm, arm9])

        self.assertIs(discovery.module_of(12, current.name), current)
        self.assertIs(discovery.module_of(16, current.name), itcm)
        self.assertIs(discovery.module_of(24, current.name), arm9)
        self.assertIsNone(discovery.module_of(28, current.name))
