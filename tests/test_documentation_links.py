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


if __name__ == "__main__":
    unittest.main()
