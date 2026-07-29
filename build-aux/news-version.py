#!/usr/bin/env python3
"""Read the canonical release identity from the top NEWS entry."""

import argparse
import datetime
import re
import sys
from pathlib import Path


VERSION = r"\d+\.\d+\.\d+(?:-rc\d+)?"
ENTRY_RE = re.compile(
    rf"^\s*(?P<version>{VERSION})\s+\((?P<date>\d{{4}}-\d{{2}}-\d{{2}})\)\s*$"
)
HEADER_RE = re.compile(r"^\s*\d+\.\d+\.\d+")
RC_RE = re.compile(r"^(?P<base>\d+\.\d+\.\d+)-rc(?P<number>\d+)$")


def find_news(start: Path) -> Path:
    """Find NEWS in the current directory or one of its parents."""
    current = start.resolve()
    for candidate in [current, *current.parents]:
        news = candidate / "NEWS"
        if news.is_file():
            return news
    raise FileNotFoundError("NEWS file not found in current dir or ancestors")


def parse_top_entry(news: Path):
    """Return the first NEWS version and date, rejecting a malformed header."""
    with news.open(encoding="utf-8") as source:
        for raw in source:
            line = raw.rstrip("\n").strip()
            if not line:
                continue
            match = ENTRY_RE.match(line)
            if match:
                date = match.group("date")
                try:
                    datetime.date.fromisoformat(date)
                except ValueError as error:
                    raise ValueError(
                        f"Invalid release date in top NEWS entry: {date}"
                    ) from error
                return match.group("version"), date
            if HEADER_RE.match(line):
                raise ValueError(
                    f"Malformed top NEWS entry: {line!r}. "
                    "Expected 'X.Y.Z (YYYY-MM-DD)' or "
                    "'X.Y.Z-rcN (YYYY-MM-DD)'."
                )
    raise ValueError(
        f"No valid version entry found in {news}. "
        "Expected a line matching 'X.Y.Z (YYYY-MM-DD)'."
    )


def native_versions(version: str):
    """Map a public version to package-native upstream versions."""
    match = RC_RE.match(version)
    if not match:
        return {
            "tag": f"v{version}",
            "debian": f"{version}-1",
            "rpm": version,
            "rpm_release": "1",
            "arch": version,
            "arch_release": "1",
        }

    base = match.group("base")
    number = match.group("number")
    return {
        "tag": f"v{version}",
        "debian": f"{base}~rc{number}-1",
        "rpm": f"{base}~rc{number}",
        "rpm_release": "1",
        "arch": f"{base}rc{number}",
        "arch_release": "1",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Emit release identity from NEWS.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--version", action="store_true", help="print version only")
    group.add_argument("--date", action="store_true", help="print release date only")
    group.add_argument("--tag", action="store_true", help="print annotated tag name")
    group.add_argument("--debian-version", action="store_true",
                       help="print Debian package version")
    group.add_argument("--rpm-version", action="store_true",
                       help="print RPM Version value")
    group.add_argument("--rpm-release", action="store_true",
                       help="print RPM Release value")
    group.add_argument("--arch-version", action="store_true",
                       help="print Arch pkgver value")
    group.add_argument("--check", metavar="EXPECTED",
                       help="exit non-zero if NEWS differs from EXPECTED")
    parser.add_argument("--news", default=None,
                        help="path to NEWS file (default: auto-discover)")
    args = parser.parse_args()

    try:
        news_path = Path(args.news) if args.news else find_news(Path.cwd())
        version, date = parse_top_entry(news_path)
    except (FileNotFoundError, ValueError) as error:
        print(f"news-version.py: {error}", file=sys.stderr)
        return 2

    mapped = native_versions(version)
    if args.version:
        print(version)
    elif args.date:
        print(date)
    elif args.tag:
        print(mapped["tag"])
    elif args.debian_version:
        print(mapped["debian"])
    elif args.rpm_version:
        print(mapped["rpm"])
    elif args.rpm_release:
        print(mapped["rpm_release"])
    elif args.arch_version:
        print(mapped["arch"])
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
    raise SystemExit(main())
