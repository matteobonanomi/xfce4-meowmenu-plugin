#!/usr/bin/env python3

import importlib.util
import re
import tempfile
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

    def test_gitless_source_tree_ignores_build_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text("# Project\n", encoding="utf-8")
            generated = root / "build"
            generated.mkdir()
            (generated / "generated.md").write_text(
                "Spec" + "-Kit\n",
                encoding="utf-8",
            )
            self.assertEqual(
                [relative for relative, _path in PRESENTATION.repository_files(root)],
                ["README.md"],
            )

    def test_selected_maintainer_note_is_in_presentation_scope(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            note = root / "dev/docs"
            note.mkdir(parents=True)
            (note / "ci.md").write_text("Current CI.\n", encoding="utf-8")
            self.assertEqual(
                [relative for relative, _path in PRESENTATION.repository_files(root)],
                ["dev/docs/ci.md"],
            )

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

    def test_release_guide_is_evergreen_and_automatic(self):
        releasing = (ROOT / "RELEASING.md").read_text(encoding="utf-8")
        self.assertIn("Automatic publication", releasing)
        self.assertIn("annotated or lightweight", releasing)
        self.assertIn("becomes public automatically", releasing)
        self.assertIn("Existing-tag recovery", releasing)
        self.assertIn("Select `main`", releasing)
        self.assertNotRegex(releasing, r"\bv\d+\.\d+\.\d+(?:-rc\d+)?\b")
        for retired in (
            "Private candidate workflow",
            "Authorization exactly",
            "live-evidence URL",
            "release-candidate versions are published as prereleases",
        ):
            self.assertNotIn(retired, releasing)

    def test_public_dependency_text_has_no_internal_workflow_ids(self):
        documents = [ROOT / "README.md", *(ROOT / "docs").glob("*.md")]
        forbidden = re.compile(
            re.escape(".spec" + "ify/")
            + r"|\b(?:FR|SC|T)\-\d{3}\b|"
            + re.escape("Spec" + "-Kit")
        )
        for document in documents:
            self.assertIsNone(
                forbidden.search(document.read_text(encoding="utf-8")),
                document,
            )

    def test_readme_describes_release_packages_without_arch_binary(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertRegex(
            " ".join(readme.split()),
            r"native packages for Ubuntu.+Debian.+Fedora",
        )
        self.assertIn("source archive", readme)
        self.assertIn("SHA256SUMS", readme)
        self.assertRegex(readme, r"(?i)Arch.{0,100}(?:AUR|not attached)")

    def test_keyboard_document_describes_the_supported_model(self):
        keyboard = (ROOT / "docs/keyboard-navigation.md").read_text(encoding="utf-8")
        for required in ("Tab", "Backspace", "Ctrl+Tab", "Wayland"):
            self.assertIn(required, keyboard)
        for obsolete in (
            "canonical focus-area cycling",
            ".spec" + "ify/",
            "Spec" + "-Kit",
        ):
            self.assertNotIn(obsolete, keyboard)

    def test_current_configuration_and_upgrade_presentation(self):
        configuration = (ROOT / "docs/configuration.md").read_text(encoding="utf-8")
        presets = (ROOT / "docs/presets.md").read_text(encoding="utf-8")
        installation = (ROOT / "docs/installation.md").read_text(encoding="utf-8")
        testing = (ROOT / "docs/testing.md").read_text(encoding="utf-8")

        for required in ("show profile", "show session controls", "left", "right", "horizontal"):
            self.assertIn(required, configuration.lower())
        for retired in (
            "`profile-position`",
            "`commands-position`",
            "`unified-bar`",
            "Grid columns",
            "Grid rows",
        ):
            self.assertNotIn(retired, configuration)
        self.assertRegex(presets, r"(?i)Classic|Modern|Minimal|Full Screen")
        for document in (presets, installation, testing):
            self.assertNotRegex(
                document,
                r"(?i)one-time reset|resets exactly once|legacy[- ]key|"
                r"retired (?:layout|sidebar|grid)",
            )

    def test_current_release_surfaces_use_news_or_no_version_seed(self):
        news = (ROOT / "NEWS").read_text(encoding="utf-8")
        current_version = news.split(maxsplit=1)[0]
        surfaces = (
            ".github/SECURITY.md",
            ".github/ISSUE_TEMPLATE/bug-report.yml",
            ".github/ISSUE_TEMPLATE/compatibility-report.yml",
            "dist/rpm/xfce4-meowmenu-plugin.spec",
            "dist/arch/PKGBUILD",
        )
        stale = re.compile(r"\b(?:0\.9\.0(?:-rc\d+)?|1\.0\.0-rc1)\b")
        for relative in surfaces:
            content = (ROOT / relative).read_text(encoding="utf-8")
            self.assertIsNone(stale.search(content), relative)
        self.assertRegex(current_version, r"^\d+\.\d+\.\d+(?:-rc\d+)?$")

    def test_synthetic_fixture_versions_are_not_current_claims(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "NEWS").write_text(
                "0.8.1 (2026-08-17)\n=====\n",
                encoding="utf-8",
            )
            fixture = root / "tests"
            fixture.mkdir()
            (fixture / "release-fixture.txt").write_text(
                "Synthetic version 9.9.9-rc4\n",
                encoding="utf-8",
            )
            self.assertEqual(PRESENTATION.current_state_violations(root), [])

    def test_public_pages_reject_retired_controls_but_allow_editorial_growth(self):
        forbidden = re.compile(
            r"(?i)one-time reset|resets exactly once|legacy[- ]key|"
            r"historical upgrade|retired (?:layout|sidebar|grid)"
        )
        for document in (ROOT / "docs").glob("*.md"):
            self.assertIsNone(
                forbidden.search(document.read_text(encoding="utf-8")),
                document,
            )


if __name__ == "__main__":
    unittest.main()
