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
    def test_distribution_seeds_are_aligned_with_news(self):
        news_version, _date = NEWS_VERSION.parse_top_entry(ROOT / "NEWS")
        expected = NEWS_VERSION.native_versions(news_version)
        control = (ROOT / "debian/changelog").read_text(encoding="utf-8")
        spec = (ROOT / "dist/rpm/xfce4-meowmenu-plugin.spec").read_text(
            encoding="utf-8"
        )
        pkgbuild = (ROOT / "dist/arch/PKGBUILD").read_text(encoding="utf-8")
        self.assertIn(f"xfce4-meowmenu-plugin ({expected['debian']})", control)
        self.assertIn(f"Version:        {expected['rpm']}", spec)
        self.assertIn(f"_upstream_version={news_version}", pkgbuild)
        self.assertIn(f"pkgver={expected['arch']}", pkgbuild)
        self.assertIn("-Daccountsservice=disabled", pkgbuild)
        self.assertIn("-Dgtk-layer-shell=disabled", pkgbuild)

    def test_release_candidate_mappings_are_version_independent(self):
        self.assertEqual(
            NEWS_VERSION.native_versions("2.4.1-rc12"),
            {
                "tag": "v2.4.1-rc12",
                "debian": "2.4.1~rc12-1",
                "rpm": "2.4.1~rc12",
                "rpm_release": "1",
                "arch": "2.4.1rc12",
                "arch_release": "1",
            },
        )

    def test_stable_mappings_remain_supported(self):
        mapped = NEWS_VERSION.native_versions("3.7.2")
        self.assertEqual(mapped["tag"], "v3.7.2")
        self.assertEqual(mapped["debian"], "3.7.2-1")
        self.assertEqual(mapped["rpm"], "3.7.2")
        self.assertEqual(mapped["arch"], "3.7.2")

    def test_unsupported_versions_are_rejected(self):
        for version in ("1.0", "1.0.0-beta1", "v1.0.0", "1.0.0-rc"):
            with self.subTest(version=version):
                with self.assertRaisesRegex(ValueError, "Unsupported release version"):
                    NEWS_VERSION.native_versions(version)

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
        for candidate in ("4.5.6-rc2", "4.5.6-rc11"):
            with self.subTest(candidate=candidate):
                mapped = NEWS_VERSION.native_versions(candidate)
                number = candidate.rsplit("-rc", maxsplit=1)[1]
                self.assertTrue(mapped["debian"].endswith(f"~rc{number}-1"))
                self.assertTrue(mapped["rpm"].endswith(f"~rc{number}"))
                self.assertTrue(mapped["arch"].endswith(f"rc{number}"))
                self.assertNotIn("-", mapped["arch"])


if __name__ == "__main__":
    unittest.main()
