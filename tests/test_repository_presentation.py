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
            "Publish the prerelease",
            "Authorization exactly",
            "live-evidence URL",
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
        self.assertIn(
            "native packages for Ubuntu 26.04, Debian 13, and\nFedora 44",
            readme,
        )
        self.assertIn("source archive and `SHA256SUMS`", readme)
        self.assertIn("not attached as a binary", readme)

    def test_public_ci_wording_does_not_claim_continuous_source_stack(self):
        documents = (
            ROOT / "README.md",
            ROOT / "docs/testing.md",
            ROOT / "docs/support.md",
            ROOT / "docs/known-limitations.md",
        )
        for document in documents:
            content = document.read_text(encoding="utf-8")
            self.assertNotRegex(
                content,
                r"(?i)continuous(?:ly)? (?:source|source-stack)",
                document,
            )

    def test_keyboard_document_describes_the_supported_model(self):
        keyboard = (ROOT / "docs/keyboard-navigation.md").read_text(
            encoding="utf-8"
        )
        for required in (
            "There is no wrapping",
            "Ctrl+Tab",
            "consumed no-ops",
            "Calculator is the first visual result",
            "first current result",
            "configured global shortcut",
            "Wayland remains experimental",
        ):
            self.assertIn(required, keyboard)
        for obsolete in (
            "moves focus through the areas",
            "wrapping around at the ends",
            "canonical focus-area cycling",
            ".spec" + "ify/",
            "Spec" + "-Kit",
        ):
            self.assertNotIn(obsolete, keyboard)

    def test_composition_and_reset_presentation_is_current(self):
        configuration = (ROOT / "docs/configuration.md").read_text(encoding="utf-8")
        presets = (ROOT / "docs/presets.md").read_text(encoding="utf-8")
        installation = (ROOT / "docs/installation.md").read_text(encoding="utf-8")
        testing = (ROOT / "docs/testing.md").read_text(encoding="utf-8")

        for required in (
            "Show profile",
            "Show session controls",
            "**left**, **right**, or in a **Horizontal** strip",
            "`show-profile`",
            "`show-session`",
            "`left`, `right`, or `horizontal`",
            "logical trailing edge of its Results",
            "icons use the same effective size",
            "whenever Profile or a vertical sidebar is visible",
            "Only when Profile, the vertical sidebar, and Session are all hidden",
            "Search remains visible with a positive usable allocation",
            "logical-leading, Search is",
        ):
            self.assertIn(required, configuration)
        for retired in (
            "`profile-position`",
            "`commands-position`",
            "`unified-bar`",
            "**top**, or **bottom**",
        ):
            self.assertNotIn(retired, configuration)

        for name in ("Classic", "Modern", "Minimal", "Full Screen"):
            self.assertIn(f"### {name}", presets)
        self.assertIn("fresh installation starts on the **Modern**", presets)
        self.assertIn("incompatible and is rejected", presets)
        self.assertIn("resets each existing pre-1.0 MeowMenu", installation)
        self.assertIn("preserves\nthe panel item and its position", installation)
        self.assertIn("every eligible instance resets exactly once to Modern", testing)


if __name__ == "__main__":
    unittest.main()
