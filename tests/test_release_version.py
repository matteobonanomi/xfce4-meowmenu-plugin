#!/usr/bin/env python3

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from release_fixtures import write_news


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build-aux" / "news-version.py"
SPEC = importlib.util.spec_from_file_location("news_version", HELPER)
NEWS_VERSION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NEWS_VERSION)


class NewsVersionTest(unittest.TestCase):
    def test_candidate_mappings(self):
        self.assertEqual(
            NEWS_VERSION.native_versions("0.9.0-rc1"),
            {
                "tag": "v0.9.0-rc1",
                "debian": "0.9.0~rc1-1",
                "rpm": "0.9.0~rc1",
                "rpm_release": "1",
                "arch": "0.9.0rc1",
                "arch_release": "1",
            },
        )

    def test_stable_mappings_remain_supported(self):
        mapped = NEWS_VERSION.native_versions("1.0.0")
        self.assertEqual(mapped["debian"], "1.0.0-1")
        self.assertEqual(mapped["rpm"], "1.0.0")
        self.assertEqual(mapped["arch"], "1.0.0")

    def test_malformed_first_header_is_not_skipped(self):
        with tempfile.TemporaryDirectory() as temporary:
            news = Path(temporary) / "NEWS"
            news.write_text(
                "0.9.0-rc1 (2026-07-xx)\n=====\n\n0.8.0 (2026-07-11)\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "Malformed top NEWS entry"):
                NEWS_VERSION.parse_top_entry(news)

    def test_invalid_calendar_date_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            news = write_news(Path(temporary) / "NEWS", date="2026-02-31")
            with self.assertRaisesRegex(ValueError, "Invalid release date"):
                NEWS_VERSION.parse_top_entry(news)

    def test_cli_outputs_all_native_versions(self):
        with tempfile.TemporaryDirectory() as temporary:
            news = write_news(Path(temporary) / "NEWS")
            expected = {
                "--version": "0.9.0-rc1",
                "--tag": "v0.9.0-rc1",
                "--debian-version": "0.9.0~rc1-1",
                "--rpm-version": "0.9.0~rc1",
                "--rpm-release": "1",
                "--arch-version": "0.9.0rc1",
            }
            for option, value in expected.items():
                result = subprocess.run(
                    [sys.executable, str(HELPER), option, "--news", str(news)],
                    check=True,
                    text=True,
                    stdout=subprocess.PIPE,
                )
                self.assertEqual(result.stdout.strip(), value)

    def test_ordering_forms_are_structurally_correct(self):
        rc1 = NEWS_VERSION.native_versions("0.9.0-rc1")
        rc2 = NEWS_VERSION.native_versions("0.9.0-rc2")
        self.assertTrue(rc1["debian"].endswith("~rc1-1"))
        self.assertTrue(rc2["debian"].endswith("~rc2-1"))
        self.assertNotIn("-", rc1["arch"])


if __name__ == "__main__":
    unittest.main()
