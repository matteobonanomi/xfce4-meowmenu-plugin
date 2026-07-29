#!/usr/bin/env python3

import importlib.util
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build-aux" / "translation-status.py"
SPEC = importlib.util.spec_from_file_location("translation_status", HELPER)
STATUS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STATUS)


class TranslationStatusTest(unittest.TestCase):
    def test_inventory_covers_all_catalogs(self):
        records = STATUS.inventory(ROOT)
        self.assertEqual(len(records), 56)
        self.assertEqual(
            {record["locale"] for record in records},
            set((ROOT / "po/LINGUAS").read_text(encoding="utf-8").split()),
        )
        self.assertTrue(all(sum(record[key] for key in (
            "translated", "fuzzy", "untranslated")) > 0 for record in records))

    def test_review_and_provenance_are_separate(self):
        records = {record["locale"]: record for record in STATUS.inventory(ROOT)}
        self.assertEqual(records["it"]["review"], "Italian maintainer review")
        self.assertEqual(records["am"]["review"], "Review invited")
        self.assertIn("machine-assisted", records["am"]["provenance"])

    def test_contributor_urls_are_current(self):
        for catalog in (ROOT / "po").glob("*.po"):
            content = catalog.read_text(encoding="utf-8")
            stale = "github.com/" + "matteob/xfce4-meowmenu-plugin"
            self.assertNotIn(stale, content)

    def test_rendered_page_is_current(self):
        subprocess.run(
            [
                sys.executable,
                str(HELPER),
                "--root",
                str(ROOT),
                "--output",
                str(ROOT / "docs/translations.md"),
                "--check",
            ],
            check=True,
        )


if __name__ == "__main__":
    unittest.main()
