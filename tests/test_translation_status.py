#!/usr/bin/env python3

import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build-aux" / "translation-status.py"
SPEC = importlib.util.spec_from_file_location("translation_status", HELPER)
STATUS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STATUS)
AUDIT_HELPER = ROOT / "build-aux" / "localization-audit.py"
AUDIT_SPEC = importlib.util.spec_from_file_location(
    "localization_audit", AUDIT_HELPER
)
AUDIT = importlib.util.module_from_spec(AUDIT_SPEC)
AUDIT_SPEC.loader.exec_module(AUDIT)


POT = '''msgid ""
msgstr ""
"Project-Id-Version: fixture\\n"
"Language: C\\n"

msgid "translated"
msgstr "Translated"

msgctxt "menu"
msgid "empty"
msgstr ""

msgid "fuzzy"
msgstr ""

msgid "items"
msgid_plural "items"
msgstr[0] ""
msgstr[1] ""
'''

CATALOG = '''msgid ""
msgstr ""
"Project-Id-Version: fixture\\n"
"Language: xx\\n"
"Plural-Forms: nplurals=2; plural=(n != 1);\\n"

msgid "translated"
msgstr "Translated"

msgctxt "menu"
msgid "empty"
msgstr ""

#, fuzzy
msgid "fuzzy"
msgstr "Old wording"

msgid "items"
msgid_plural "items"
msgstr[0] "one item"
msgstr[1] "many items"

msgid "retired"
msgstr "Retired"
'''


class TranslationStatusTest(unittest.TestCase):
    def write_fixture(self, directory, name, content):
        path = Path(directory) / name
        path.write_text(content, encoding="utf-8")
        return path

    def test_intersection_counts_distinct_states(self):
        with tempfile.TemporaryDirectory() as temporary:
            pot = self.write_fixture(temporary, "fixture.pot", POT)
            catalog = self.write_fixture(temporary, "xx.po", CATALOG)
            stats = STATUS.catalog_statistics(catalog, pot)
            self.assertEqual(stats, {
                "translated": 2,
                "review_needed": 1,
                "untranslated": 1,
            })

    def test_absent_and_retired_messages_do_not_receive_credit(self):
        catalog_text = CATALOG.replace(
            '\nmsgctxt "menu"\nmsgid "empty"\nmsgstr ""\n', ""
        ).replace(
            '\nmsgid "items"\nmsgid_plural "items"\nmsgstr[0] "one item"\nmsgstr[1] "many items"\n',
            "",
        )
        with tempfile.TemporaryDirectory() as temporary:
            pot = self.write_fixture(temporary, "fixture.pot", POT)
            catalog = self.write_fixture(temporary, "xx.po", catalog_text)
            stats = STATUS.catalog_statistics(catalog, pot)
            self.assertEqual(stats["translated"], 1)
            self.assertEqual(stats["review_needed"], 1)
            self.assertEqual(stats["untranslated"], 0)
            self.assertEqual(sum(stats.values()), 2)

    def test_malformed_catalog_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            pot = self.write_fixture(temporary, "fixture.pot", POT)
            catalog = self.write_fixture(
                temporary,
                "xx.po",
                'msgid "unterminated\nmsgstr "value"\n',
            )
            with self.assertRaises(STATUS.CatalogError):
                STATUS.catalog_statistics(catalog, pot)

    def test_generated_section_has_one_kpi_and_no_provenance(self):
        records = [{
            "locale": "xx",
            "technical": "Pass",
            "translated": 1,
            "review_needed": 0,
            "untranslated": 1,
            "absent": 0,
            "coverage": 50,
            "fluent_review": "Not reviewed",
            "inventory_total": 2,
        }]
        section = STATUS.generated_section(records)
        self.assertIn("| Review needed |", section)
        self.assertIn("| Coverage |", section)
        self.assertNotIn("Provenance", section)
        self.assertEqual(section.count("%"), 1)

    def test_document_update_preserves_surrounding_guidance(self):
        records = [{
            "locale": "xx",
            "technical": "Pass",
            "translated": 2,
            "review_needed": 0,
            "untranslated": 0,
            "absent": 0,
            "coverage": 100,
            "fluent_review": "Not reviewed",
            "inventory_total": 2,
        }]
        existing = "# Translation status\n\nKeep this guidance.\n\n"
        updated = STATUS.update_document(existing, records)
        self.assertIn("Keep this guidance.", updated)
        self.assertIn(STATUS.BEGIN_MARKER, updated)
        self.assertIn(STATUS.END_MARKER, updated)

    def test_real_status_page_is_current(self):
        subprocess.run(
            [
                "python3",
                str(HELPER),
                "--root",
                str(ROOT),
                "--output",
                str(ROOT / "docs/translations.md"),
                "--check",
            ],
            check=True,
        )

    def test_review_map_is_explicit(self):
        self.assertEqual(
            STATUS.FLUENT_REVIEW,
            {
                "en_GB": "Pending maintainer review",
                "it": "Pending maintainer review",
            },
        )

    def test_catalog_credits_do_not_use_generic_placeholders(self):
        for catalog in (ROOT / "po").glob("*.po"):
            content = catalog.read_text(encoding="utf-8")
            self.assertNotRegex(
                content,
                r'msgstr(?:\[\d+\])? "(?:translator-credits|Translator credits)"',
            )

    def test_localization_audit_self_test(self):
        self.assertTrue(AUDIT.run_self_test())


if __name__ == "__main__":
    unittest.main()
