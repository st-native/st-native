# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("checkpoint_survey", Path(__file__).parents[1] / "tools/testing/checkpoint_survey.py")
survey = importlib.util.module_from_spec(spec)
spec.loader.exec_module(survey)


class CheckpointSurveyTests(unittest.TestCase):
    def test_case_ids_cannot_escape_output_and_slots_are_explicit(self):
        self.assertEqual(survey.parse_case("FL02:3:2"), ("FL02", 3, 2))
        for value in ("../escape:3:2", "C:/escape:3:2", "BO02:0:1", "BO02:24:1", "BO02:8:3"):
            with self.assertRaises(ValueError):
                survey.parse_case(value)
        self.assertIn("1300 touch 130 110", survey.load_script(2, 3000))

    def test_outputs_stay_private_and_existing_results_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "build").mkdir()
            good = root / "build" / "survey"
            self.assertEqual(survey.output_path(root, good), good.resolve())
            for path in (root / "docs", root / "build"):
                with self.assertRaises(ValueError):
                    survey.output_path(root, path)
            good.mkdir()
            sentinel = good / "result.txt"
            sentinel.write_text("keep", encoding="utf-8")
            with self.assertRaises(ValueError):
                survey.output_path(root, good)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")

    def test_profile_rejects_missing_frames_and_reports_metadata_only(self):
        with tempfile.TemporaryDirectory() as directory:
            profile = Path(directory) / "graphics.csv"
            header = "frame,capture,swap,scene,perspective_positive,perspective_negative\n"
            profile.write_text(header + "1,0,1,0,0,0\n2,1,0,0,1,0\n", encoding="utf-8")
            result = survey.summarize_profile(profile, start=1)
            self.assertEqual((result["frames"], result["capture_samples"], result["swap_samples"]), (2, 1, 1))
            self.assertNotIn("accepted", result)
            profile.write_text(header + "2,0,1,0,0,0\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                survey.summarize_profile(profile)
