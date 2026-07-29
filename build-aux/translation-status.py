#!/usr/bin/env python3
"""Generate the public translation inventory from gettext catalogs."""

import argparse
import os
import re
import subprocess
from pathlib import Path


STAT_RE = {
    "translated": re.compile(r"(\d+) translated message"),
    "fuzzy": re.compile(r"(\d+) fuzzy translation"),
    "untranslated": re.compile(r"(\d+) untranslated message"),
}


def catalog_statistics(catalog: Path):
    """Validate one catalog and return msgfmt's deterministic counts."""
    environment = dict(os.environ)
    environment["LC_ALL"] = "C"
    result = subprocess.run(
        ["msgfmt", "--check", "--statistics", "-o", os.devnull, str(catalog)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
    )
    if result.returncode:
        raise RuntimeError(f"{catalog}: {result.stderr.strip()}")
    output = result.stdout + result.stderr
    return {
        key: int(match.group(1)) if (match := expression.search(output)) else 0
        for key, expression in STAT_RE.items()
    }


def inventory(root: Path):
    """Return every locale from LINGUAS with validity and review metadata."""
    po_dir = root / "po"
    locales = (po_dir / "LINGUAS").read_text(encoding="utf-8").split()
    records = []
    for locale in locales:
        catalog = po_dir / f"{locale}.po"
        content = catalog.read_text(encoding="utf-8")
        stats = catalog_statistics(catalog)
        total = sum(stats.values())
        percent = round(100 * stats["translated"] / total) if total else 0
        records.append(
            {
                "locale": locale,
                **stats,
                "percent": percent,
                "provenance": (
                    "machine-assisted/inherited"
                    if ("LL" + "M-assisted translation") in content
                    else "inherited/contributor"
                ),
                "review": "Italian maintainer review" if locale == "it" else "Review invited",
            }
        )
    return records


def render(records):
    """Render the public page without implying fluent review from syntax."""
    rows = "\n".join(
        f"| `{record['locale']}` | Pass | {record['translated']} | "
        f"{record['fuzzy']} | {record['untranslated']} | {record['percent']}% | "
        f"{record['provenance']} | {record['review']} |"
        for record in records
    )
    return f"""---
layout: default
title: Translations
nav_order: 8
---

# Translation status

MeowMenu includes {len(records)} gettext catalogs. Every row below has passed
`msgfmt --check`; that confirms catalog syntax, not linguistic accuracy.
Completeness is generated from the current files. Italian has maintainer review;
all other languages still welcome review by fluent speakers, regardless of
their percentage or provenance.

To improve a language, edit `po/<locale>.po`, run
`msgfmt --check po/<locale>.po`, and
[open a pull request](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/pulls).

| Locale | Technical | Translated | Fuzzy | Untranslated | Complete | Provenance | Fluent review |
|---|---|---:|---:|---:|---:|---|---|
{rows}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    content = render(inventory(args.root))
    if args.check:
        if not args.output.is_file() or args.output.read_text(encoding="utf-8") != content:
            raise SystemExit(f"{args.output} is out of date")
    else:
        args.output.write_text(content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
