# SPDX-License-Identifier: GPL-3.0-or-later
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("engine_fetch", Path(__file__).parents[1] / "spike/m1a/fetch_engine.py")
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


class PatchStackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="st-patch-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        self.repo.mkdir()
        self.git("init", "--quiet")
        self.git("config", "core.autocrlf", "false")
        (self.repo / "item").write_text("one\n", encoding="utf-8", newline="\n")
        self.git("add", "item")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--quiet", "-m", "base")
        self.patches = []
        for i, (before, after) in enumerate([("one", "two"), ("two", "three")]):
            p = self.root / f"{i}.patch"
            p.write_text(f"--- a/item\n+++ b/item\n@@ -1 +1 @@\n-{before}\n+{after}\n", encoding="utf-8", newline="\n")
            self.patches.append(p)
        p = self.root / "2.patch"
        p.write_text("--- /dev/null\n+++ b/extra\n@@ -0,0 +1 @@\n+ours\n", encoding="utf-8", newline="\n")
        self.patches.append(p)

    def git(self, *args):
        return subprocess.run(["git", *args], cwd=self.repo, check=True, capture_output=True)

    def apply(self, patches):
        with contextlib.redirect_stdout(io.StringIO()):
            fetch.apply_patch_stack(self.repo, patches)

    def test_overlapping_stack_and_upgrade_preserve_real_index(self):
        index = (self.repo / ".git/index").read_bytes()
        self.apply(self.patches[:1])
        self.apply(self.patches)
        self.apply(self.patches)
        self.assertEqual((self.repo / "item").read_text(), "three\n")
        self.assertEqual((self.repo / "extra").read_text(), "ours\n")
        self.assertEqual((self.repo / ".git/index").read_bytes(), index)

    def test_local_edits_are_not_overwritten(self):
        (self.repo / "item").write_text("local\n", encoding="utf-8", newline="\n")
        with self.assertRaises(SystemExit):
            self.apply(self.patches)
        self.assertEqual((self.repo / "item").read_text(), "local\n")
        self.assertFalse((self.repo / "extra").exists())

    def test_future_untracked_file_is_checked_before_applying_any_patch(self):
        (self.repo / "extra").write_text("local\n", encoding="utf-8", newline="\n")
        with self.assertRaises(SystemExit):
            self.apply(self.patches)
        self.assertEqual((self.repo / "item").read_text(), "one\n")
        self.assertEqual((self.repo / "extra").read_text(), "local\n")
