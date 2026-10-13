#!/usr/bin/env python3
"""Generate the public translation status from the current POT intersection."""

import argparse
import os
import re
import subprocess
from pathlib import Path


EXPECTED_LOCALES = (
    "am", "ar", "ast", "be", "bg", "ca", "ca@valencia", "cs", "cy",
    "da", "de", "el", "en_GB", "eo", "es", "et", "eu", "fa", "fi",
    "fr", "gl", "he", "hr", "hu", "id", "ie", "is", "it", "ja",
    "ka", "kk", "ko", "lt", "lv", "ms", "nb", "ne", "nl", "oc",
    "pl", "pt", "pt_BR", "ro", "ru", "sk", "sl", "sr", "sr@latin",
    "sv", "th", "tr", "uk", "uz", "vi", "zh_CN", "zh_TW",
)

# This map is deliberately explicit. A locale is not marked reviewed because
# it has a high percentage or a technically valid catalog.
FLUENT_REVIEW = {
    "en_GB": "Pending maintainer review",
    "it": "Pending maintainer review",
}

STAT_RE = {
    "translated": re.compile(r"(\d+) translated message"),
    "review_needed": re.compile(r"(\d+) fuzzy translation"),
    "untranslated": re.compile(r"(\d+) untranslated message"),
}
BEGIN_MARKER = "<!-- BEGIN GENERATED TRANSLATION STATUS -->"
END_MARKER = "<!-- END GENERATED TRANSLATION STATUS -->"
BASE_DOCUMENT = """---
layout: default
title: Translations
nav_order: 8
---

# Translation status

MeowMenu includes 56 gettext catalogs. Technical validity, translation
coverage, and fluent review are separate properties. English is the safe
fallback when a translated value is absent or has not been accepted.

To improve a language, edit `po/<locale>.po`, run
`msgfmt --check --check-format -o /dev/null po/<locale>.po`, and
[open a pull request](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/pulls).
"""


class CatalogError(RuntimeError):
    """Raised when a catalog cannot be checked or reconciled."""


def _run(command, input_text=None):
    environment = dict(os.environ)
    environment["LC_ALL"] = "C"
    result = subprocess.run(
        command,
        check=False,
        input=input_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
    )
    if result.returncode:
        detail = (result.stderr or result.stdout).strip()
        raise CatalogError(f"{' '.join(command)}: {detail}")
    # gettext writes catalog data to stdout and diagnostics/statistics to
    # stderr. Keeping those streams separate prevents a warning from being
    # fed back into msgfmt when msgcomm produces an intersection.
    return result.stdout if result.stdout else result.stderr


def _parse_statistics(output):
    return {
        key: int(expression.search(output).group(1))
        if expression.search(output) else 0
        for key, expression in STAT_RE.items()
    }


def _msgfmt_statistics(catalog):
    output = _run([
        "msgfmt", "--check", "--check-format", "--statistics",
        "-o", os.devnull, str(catalog),
    ])
    return _parse_statistics(output)


def catalog_statistics(catalog, pot=None):
    """Return current translated, fuzzy, and empty counts for one catalog."""
    catalog = Path(catalog)
    if pot is None:
        return _msgfmt_statistics(catalog)

    # The POT is untranslated, so msgcomm retains the catalog state for the
    # common identity set while excluding retired catalog-only entries.
    _msgfmt_statistics(catalog)
    common = _run([
        "msgcomm", "--more-than=1", str(catalog), str(pot),
    ])
    return _parse_statistics(_run([
        "msgfmt", "--check", "--check-format", "--statistics",
        "-o", os.devnull, "-",
    ], input_text=common))


def validate_locale_set(root):
    """Return diagnostics for the fixed locale list and catalog filenames."""
    root = Path(root)
    po_dir = root / "po"
    declared = tuple(po_dir.joinpath("LINGUAS").read_text(encoding="utf-8").split())
    catalogs = tuple(sorted(path.stem for path in po_dir.glob("*.po")))
    errors = []
    if set(declared) != set(EXPECTED_LOCALES) or len(declared) != len(EXPECTED_LOCALES):
        errors.append("po/LINGUAS does not contain the fixed 56-locale set")
    if set(catalogs) != set(EXPECTED_LOCALES):
        errors.append("po/*.po does not match the fixed 56-locale set")
    if tuple(declared) != tuple(locale for locale in EXPECTED_LOCALES if locale in declared):
        errors.append("po/LINGUAS order contains duplicate or unexpected entries")
    return errors


def inventory(root):
    """Return one reconciled status record for every supported locale."""
    root = Path(root)
    errors = validate_locale_set(root)
    if errors:
        raise CatalogError("; ".join(errors))
    po_dir = root / "po"
    pot = po_dir / "xfce4-meowmenu-plugin.pot"
    pot_stats = _msgfmt_statistics(pot)
    pot_total = sum(pot_stats.values())
    records = []
    for locale in EXPECTED_LOCALES:
        stats = catalog_statistics(po_dir / f"{locale}.po", pot)
        counted = sum(stats.values())
        absent = pot_total - counted
        if absent < 0:
            raise CatalogError(f"{locale}: catalog counts exceed POT total")
        records.append({
            "locale": locale,
            "technical": "Pass",
            "translated": stats["translated"],
            "review_needed": stats["review_needed"],
            "untranslated": stats["untranslated"],
            "absent": absent,
            "coverage": round(100 * stats["translated"] / pot_total) if pot_total else 0,
            "fluent_review": FLUENT_REVIEW.get(locale, "Not reviewed"),
            "inventory_total": pot_total,
        })
    return records


def generated_section(records):
    """Render only the deterministic status block."""
    if not records:
        raise ValueError("translation status requires at least one locale")
    total = records[0]["inventory_total"]
    rows = [
        BEGIN_MARKER,
        "",
        f"The current generated inventory contains {total} message identities.",
        "Coverage is the only percentage shown: translated messages divided by "
        "that complete inventory. Review-needed, untranslated, and absent "
        "messages remain separate counts; retired catalog material earns no "
        "credit.",
        "",
        "| Locale | Technical | Translated | Review needed | Untranslated | Absent | Coverage | Fluent review |",
        "|---|---|---:|---:|---:|---:|---:|---|",
    ]
    rows.extend(
        f"| `{record['locale']}` | {record['technical']} | "
        f"{record['translated']} | {record['review_needed']} | "
        f"{record['untranslated']} | {record['absent']} | "
        f"{record['coverage']}% | {record['fluent_review']} |"
        for record in records
    )
    rows.extend(["", END_MARKER])
    return "\n".join(rows)


def update_document(existing, records):
    """Replace only the generated section, preserving surrounding guidance."""
    section = generated_section(records)
    if BEGIN_MARKER in existing and END_MARKER in existing:
        pattern = re.compile(
            re.escape(BEGIN_MARKER) + r".*?" + re.escape(END_MARKER),
            re.DOTALL,
        )
        return pattern.sub(section, existing, count=1).rstrip() + "\n"

    lines = existing.splitlines()
    table_start = next(
        (index for index, line in enumerate(lines) if line.startswith("| Locale |")),
        None,
    )
    if table_start is not None:
        table_end = table_start
        while table_end < len(lines) and lines[table_end].startswith("|"):
            table_end += 1
        prefix = "\n".join(lines[:table_start]).rstrip()
        suffix = "\n".join(lines[table_end:]).lstrip()
        combined = prefix + "\n\n" + section
        if suffix:
            combined += "\n\n" + suffix
        return combined.rstrip() + "\n"
    return existing.rstrip() + "\n\n" + section + "\n"


def expected_document(root):
    """Return the document content that matches the current catalog inputs."""
    root = Path(root)
    path = root / "docs/translations.md"
    existing = path.read_text(encoding="utf-8") if path.is_file() else BASE_DOCUMENT
    return update_document(existing, inventory(root))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true", help="write the generated section")
    mode.add_argument("--check", action="store_true", help="fail if the page is stale")
    args = parser.parse_args()

    output = args.output or args.root / "docs/translations.md"
    expected = expected_document(args.root)
    if args.check:
        if not output.is_file() or output.read_text(encoding="utf-8") != expected:
            raise SystemExit(f"{output} is out of date")
    else:
        output.write_text(expected, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
