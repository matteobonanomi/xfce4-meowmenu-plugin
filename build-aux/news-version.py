#!/usr/bin/env python3
# news-version.py:
# Parse the top entry of the project NEWS file and emit either the version
# string, the release date, or a consistency check result. Used by Meson at
# configure time (FR-001..FR-003, FR-012).
#
# Top entry format (first non-empty, non-header line that matches):
#     X.Y.Z[suffix] (YYYY-MM-DD)
# e.g. "0.3.1 (2026-05-17)" or "0.4.0-rc1 (2026-06-01)".

import argparse
import re
import sys
from pathlib import Path

ENTRY_RE = re.compile(
    r"^\s*(?P<version>\d+\.\d+\.\d+[A-Za-z0-9.\-+]*)\s+\((?P<date>\d{4}-\d{2}-\d{2})\)\s*$"
)


def find_news(start: Path) -> Path:
    # Search current dir and ancestors for a NEWS file. Lets the script work
    # whether invoked from repo root or from a meson build subdirectory.
    cur = start.resolve()
    for candidate in [cur, *cur.parents]:
        news = candidate / "NEWS"
        if news.is_file():
            return news
    raise FileNotFoundError("NEWS file not found in current dir or ancestors")


def parse_top_entry(news: Path):
    with news.open(encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\n").strip()
            if not line:
                continue
            m = ENTRY_RE.match(line)
            if m:
                return m.group("version"), m.group("date")
    raise ValueError(
        f"No valid version entry found in {news}. "
        "Expected a line matching 'X.Y.Z (YYYY-MM-DD)'."
    )


def main() -> int:
    ap = argparse.ArgumentParser(description="Emit version/date from NEWS top entry.")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--version", action="store_true", help="print version only")
    g.add_argument("--date", action="store_true", help="print release date only")
    g.add_argument("--check", metavar="EXPECTED",
                   help="exit non-zero if NEWS version differs from EXPECTED")
    ap.add_argument("--news", default=None,
                    help="path to NEWS file (default: auto-discover)")
    args = ap.parse_args()

    try:
        news_path = Path(args.news) if args.news else find_news(Path.cwd())
        version, date = parse_top_entry(news_path)
    except (FileNotFoundError, ValueError) as e:
        print(f"news-version.py: {e}", file=sys.stderr)
        return 2

    if args.version:
        print(version)
    elif args.date:
        print(date)
    elif args.check is not None:
        if args.check != version:
            print(
                f"news-version.py: NEWS says {version} but expected {args.check}",
                file=sys.stderr,
            )
            return 1
        print(f"OK: {version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
