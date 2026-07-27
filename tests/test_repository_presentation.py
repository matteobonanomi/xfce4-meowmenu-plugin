#!/usr/bin/env python3

import importlib.util
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build-aux" / "repository-presentation.py"
SPEC = importlib.util.spec_from_file_location("repository_presentation", HELPER)
PRESENTATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PRESENTATION)


class RepositoryPresentationTest(unittest.TestCase):
    def test_tracked_presentation_policy(self):
        self.assertEqual(PRESENTATION.violations(ROOT), [])

    def test_compiled_bug_report_route_is_current(self):
        meson = (ROOT / "meson.build").read_text(encoding="utf-8")
        self.assertIn(
            'PACKAGE_BUGREPORT="https://github.com/'
            'matteobonanomi/xfce4-meowmenu-plugin/issues"',
            meson,
        )

    def test_appstream_identity_is_repository_owned(self):
        metainfo = ROOT / (
            "data/metainfo/"
            "io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml"
        )
        self.assertTrue(metainfo.is_file())
        self.assertIn(
            "<id>io.github.matteobonanomi.xfce4-meowmenu-plugin</id>",
            metainfo.read_text(encoding="utf-8"),
        )

    def test_release_guide_separates_examples_from_current_candidate(self):
        releasing = (ROOT / "RELEASING.md").read_text(encoding="utf-8")
        self.assertIn("`v1.0.0`", releasing)
        self.assertIn("`v0.9.0-rc1`", releasing)
        self.assertNotIn("git tag -a v0.9.0-rc1", releasing)


if __name__ == "__main__":
    unittest.main()
