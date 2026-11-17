# SPDX-License-Identifier: GPL-3.0-or-later
import contextlib
import hashlib
import io
import os
from pathlib import Path
import runpy
import tempfile
import unittest
from unittest.mock import patch


@unittest.skipUnless(os.name == "nt", "Windows launcher worker")
class LauncherCacheTests(unittest.TestCase):
    def setUp(self):
        temp = self.enterContext(tempfile.TemporaryDirectory())
        self.root = Path(temp).resolve()
        self.build = self.root / "build/launcher"
        self.cache = self.build / "cache"
        self.cache.mkdir(parents=True)
        worker = Path(__file__).resolve().parents[1] / "platform/windows/launcher.py"
        self.launcher = runpy.run_path(str(worker))
        self.enterContext(patch.dict(self.launcher["prepare"].__globals__, ROOT=self.root, BUILD=self.build))
        self.active = self.cache / ("a" * 64)
        self.backup = self.active.with_suffix(".previous")

    def module(self, folder, data):
        folder.mkdir(parents=True)
        (folder / "st_game.dll").write_bytes(data)
        (folder / "sha256").write_text(hashlib.sha256(data).hexdigest(), encoding="ascii")

    def test_failed_promotion_restores_previous_game_module(self):
        self.module(self.active, b"old test module")
        ready = self.build / "runs/test/ready"
        self.module(ready, b"new test module")
        replace = Path.replace

        def fail_on_publish(source, destination):
            if source == ready:
                raise OSError("simulated promotion error")
            return replace(source, destination)

        with patch.object(Path, "replace", fail_on_publish), self.assertRaises(OSError):
            self.launcher["publish_generation"](ready, self.active, self.backup)
        self.assertEqual(self.launcher["cached_module"](self.active).read_bytes(), b"old test module")
        self.assertFalse(self.backup.exists())

    def test_remove_cache_preserves_other_directories_and_logs(self):
        self.module(self.active, b"test module")
        log = self.build / "runs/tmp01234567/prepare.log"
        log.parent.mkdir(parents=True)
        log.write_text("test log", encoding="utf-8")
        unrelated = self.cache / "notes.txt"
        unrelated.touch()
        with contextlib.redirect_stdout(io.StringIO()):
            self.launcher["remove_cache"]()
        self.assertFalse(self.active.exists())
        self.assertTrue(log.is_file())
        self.assertTrue(unrelated.is_file())

    def test_remove_checks_every_generation_before_deleting(self):
        self.module(self.active, b"test module")
        other = self.cache / ("b" * 64)
        self.module(other, b"second test module")
        unexpected = other / "unrelated.txt"
        unexpected.touch()
        with self.assertRaises(self.launcher["SetupError"]):
            self.launcher["remove_cache"]()
        self.assertTrue((self.active / "st_game.dll").is_file())
        self.assertTrue(unexpected.is_file())
