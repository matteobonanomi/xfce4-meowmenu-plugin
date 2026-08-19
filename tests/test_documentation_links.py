#!/usr/bin/env python3

import importlib.util
import re
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build-aux" / "documentation-links.py"
SPEC = importlib.util.spec_from_file_location("documentation_links", HELPER)
LINKS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LINKS)


def markdown_tables(content):
    """Yield Markdown tables as lists of parsed cells."""
    lines = content.splitlines()
    tables = []
    index = 0
    while index + 1 < len(lines):
        if not lines[index].lstrip().startswith("|"):
            index += 1
            continue
        if not lines[index + 1].lstrip().startswith("|"):
            index += 1
            continue
        rows = []
        while index < len(lines) and lines[index].lstrip().startswith("|"):
            rows.append(
                [cell.strip() for cell in lines[index].strip().strip("|").split("|")]
            )
            index += 1
        tables.append(rows)
    return tables


class DocumentationLinksTest(unittest.TestCase):
    def test_repository_links_and_commands(self):
        self.assertEqual(LINKS.violations(ROOT), [])

    def test_extensionless_jekyll_route(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            document = docs / "index.md"
            target = docs / "support.md"
            document.write_text("[Support](support)\n", encoding="utf-8")
            target.write_text("# Support\n", encoding="utf-8")
            self.assertTrue(LINKS.resolve_link(root, document, "support"))

    def test_missing_route_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = root / "README.md"
            document.write_text("[Missing](docs/nope)\n", encoding="utf-8")
            self.assertFalse(LINKS.resolve_link(root, document, "docs/nope"))

    def test_gitless_source_tree_ignores_build_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            (docs / "support.md").write_text("# Support\n", encoding="utf-8")
            generated = root / "build"
            generated.mkdir()
            (generated / "generated.md").write_text(
                "[Missing](nowhere)\n",
                encoding="utf-8",
            )
            self.assertEqual(
                [relative for relative, _path in LINKS.markdown_files(root)],
                ["docs/support.md"],
            )

    def test_selected_maintainer_note_is_checked(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            note = root / "dev/docs"
            note.mkdir(parents=True)
            (note / "ci.md").write_text(
                "[Missing](not-a-real-maintainer-route)\n",
                encoding="utf-8",
            )
            self.assertEqual(
                LINKS.violations(root),
                [
                    "dev/docs/ci.md:1: missing link target "
                    "not-a-real-maintainer-route"
                ],
            )

    def test_public_navigation_has_unique_order_and_required_pages(self):
        navigation = {}
        for document in (ROOT / "docs").glob("*.md"):
            content = document.read_text(encoding="utf-8")
            if not content.startswith("---\n"):
                continue
            front_matter = content.split("---\n", 2)[1]
            match = re.search(r"^nav_order:\s*(\d+)\s*$", front_matter, re.MULTILINE)
            self.assertIsNotNone(match, document)
            order = int(match.group(1))
            self.assertNotIn(order, navigation, f"duplicate nav_order {order}")
            navigation[order] = document.name
        self.assertTrue(
            {
                "support.md",
                "known-limitations.md",
                "testing.md",
                "translations.md",
            }.issubset(set(navigation.values()))
        )

    def test_public_entry_points_are_current_and_linked(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        home = (ROOT / "docs/index.md").read_text(encoding="utf-8")
        for target in (
            "docs/support.md",
            "docs/known-limitations.md",
            "docs/testing.md",
            "docs/translations.md",
        ):
            self.assertIn(f"]({target})", readme)
        for target in (
            "support",
            "known-limitations",
            "testing",
            "translations",
        ):
            self.assertIn(f"]({target})", home)
        self.assertIn("MeowMenu", readme)
        self.assertIn("X11", readme)
        self.assertIn("Wayland", readme)
        self.assertNotRegex(home, r"(?i)current release candidate|check RC1")

    def test_public_release_and_configuration_policy_is_semantic(self):
        documents = [
            ROOT / "README.md",
            ROOT / "docs/installation.md",
            ROOT / "docs/support.md",
            ROOT / ".github/SECURITY.md",
        ]
        content = " ".join(document.read_text(encoding="utf-8") for document in documents)
        self.assertRegex(content, r"(?is)0\.x.{0,180}experimental")
        self.assertRegex(content, r"(?is)rc.{0,180}(?:stable|stabl|testing)")
        self.assertRegex(content, r"(?is)newest 0\.x.{0,180}feature")
        self.assertRegex(content, r"(?is)before final 1\.0\.0.{0,180}not guaranteed")
        self.assertRegex(content, r"(?is)final 1\.0\.0.{0,180}preserv")

    def test_public_pages_avoid_historical_and_obsolete_framing(self):
        pages = tuple((ROOT / "docs").glob("*.md"))
        forbidden = re.compile(
            r"(?i)one-time reset|resets exactly once|legacy[- ]key|"
            r"retired (?:layout|sidebar|grid) (?:setting|key)|"
            r"upgrade chronology|historical reset"
        )
        current_whisker_identity = re.compile(
            r"(?im)^#\s+Whisker Menu\b|\bWhisker Menu is\b|"
            r"\bcurrent (?:product|launcher) is Whisker Menu\b"
        )
        for document in pages:
            content = document.read_text(encoding="utf-8")
            self.assertIsNone(forbidden.search(content), document)
            self.assertIsNone(current_whisker_identity.search(content), document)

    def test_configuration_tables_keep_role_specific_shapes(self):
        configuration = (ROOT / "docs/configuration.md").read_text(encoding="utf-8")
        tables = markdown_tables(configuration)
        self.assertTrue(
            any(table[0][:2] == ["Option", "Description"] for table in tables)
        )
        self.assertTrue(
            any(
                table[0][:4] == ["Key", "Type", "Default", "Description"]
                for table in tables
            )
        )
        for table in tables:
            width = len(table[0])
            self.assertGreaterEqual(width, 2)
            for row in table[2:]:
                self.assertEqual(len(row), width, table[0])

    def test_current_controls_and_layout_positions_are_documented(self):
        configuration = (ROOT / "docs/configuration.md").read_text(encoding="utf-8")
        testing = (ROOT / "docs/testing.md").read_text(encoding="utf-8")
        self.assertRegex(configuration, r"(?i)sidebar.*left.*right.*horizontal")
        self.assertNotIn("Grid columns", configuration)
        self.assertNotIn("Grid rows", configuration)
        for obsolete in ("`profile-position`", "`commands-position`", "`unified-bar`"):
            self.assertNotIn(obsolete, configuration)
        self.assertNotRegex(testing, r"(?i)sidebar (?:at|on) the top|sidebar (?:at|on) the bottom")

    def test_support_and_testing_pages_record_scope_without_snapshots(self):
        support = (ROOT / "docs/support.md").read_text(encoding="utf-8")
        testing = (ROOT / "docs/testing.md").read_text(encoding="utf-8")
        for field in (
            "MeowMenu version",
            "Distribution/version",
            "Xfce version",
            "Architecture",
            "Session type",
            "Installation method",
        ):
            self.assertIn(field, testing)
        self.assertIn("X11", support)
        self.assertIn("Wayland", support)
        self.assertIn("not guaranteed", support)
        self.assertNotIn("compatibility matrix", testing.lower())

    def test_keyboard_surface_is_cross_linked_and_current(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        keyboard = (ROOT / "docs/keyboard-navigation.md").read_text(encoding="utf-8")
        self.assertIn("docs/keyboard-navigation.md", readme)
        for key in ("Tab", "Backspace", "Ctrl+Tab", "Wayland"):
            self.assertIn(key, keyboard)

    def test_source_build_and_optional_fallback_guidance_is_present(self):
        installation = (ROOT / "docs/installation.md").read_text(encoding="utf-8")
        limitations = (ROOT / "docs/known-limitations.md").read_text(encoding="utf-8")
        for integration in ("AccountsService", "gtk-layer-shell"):
            self.assertIn(integration, installation)
            self.assertIn(integration, limitations)
        self.assertRegex(installation, r"(?i)optional.{0,120}source")
        self.assertRegex(installation, r"(?i)X11.{0,120}officially supported")
        self.assertRegex(installation, r"(?i)Wayland.{0,160}experimental")


if __name__ == "__main__":
    unittest.main()
