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

    def test_current_candidate_and_evidence_terms_are_consistent(self):
        news = (ROOT / "NEWS").read_text(encoding="utf-8")
        version = re.match(
            r"^(\S+) \(\d{4}-\d{2}-\d{2}\)$", news.splitlines()[0]
        ).group(1)
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        support = (ROOT / "docs/support.md").read_text(encoding="utf-8")
        self.assertIn(version, readme)
        self.assertIn(version, support)
        for label in (
            "Source-build compatible",
            "Staged-install compatible",
            "Published package target",
            "Maintainer-published source recipe",
            "Live validated",
        ):
            self.assertIn(label, support)
        self.assertRegex(support, r"not a prebuilt\s+project package")

    def test_release_specific_dependency_classes_are_documented(self):
        installation = (
            ROOT / "docs/installation.md"
        ).read_text(encoding="utf-8")
        required = {
            "Ubuntu 26.04": "libexo-2-dev",
            "Debian 13": "libexo-2-dev",
            "Fedora 44": "exo-devel",
            "Arch / Manjaro / EndeavourOS": "exo",
        }
        for heading, dependency in required.items():
            section = installation.split(f"### {heading}", maxsplit=1)[1]
            section = section.split("\n### ", maxsplit=1)[0]
            self.assertIn(dependency, section)
        self.assertIn("Optional integrations on Ubuntu 26.04", installation)
        self.assertIn("Optional integrations on Debian 13", installation)
        self.assertIn("Optional integrations on Fedora 44", installation)
        self.assertIn("Optional integrations on Arch", installation)
        self.assertIn("libxfce4ui 4.21", installation)


if __name__ == "__main__":
    unittest.main()
