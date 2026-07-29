#!/usr/bin/env python3
"""Extract complete release notes from the top NEWS entry."""

import argparse
import re
from pathlib import Path

from validate_release import validate_release_notes


HEADER_RE = re.compile(r"^\d+\.\d+\.\d+(?:-rc\d+)? \(\d{4}-\d{2}-\d{2}\)$")


def top_news_body(news: Path, version: str):
    """Return the exact body belonging to the top NEWS entry."""
    lines = news.read_text(encoding="utf-8").splitlines()
    header_index = next((i for i, line in enumerate(lines) if HEADER_RE.match(line)), None)
    if header_index is None or not lines[header_index].startswith(f"{version} ("):
        raise ValueError(f"Top NEWS entry does not match {version}")
    body_start = header_index + 1
    if body_start < len(lines) and set(lines[body_start].strip()) == {"="}:
        body_start += 1
    body = []
    for line in lines[body_start:]:
        if HEADER_RE.match(line):
            break
        body.append(line)
    while body and not body[0]:
        body.pop(0)
    while body and not body[-1]:
        body.pop()
    if not any(line.strip() for line in body):
        raise ValueError("Top NEWS entry is empty")
    return "\n".join(body)


def render(news: Path, version: str):
    """Return the authoritative release body without adding presentation."""
    return top_news_body(news, version)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--news", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    content = render(args.news, args.version)
    args.output.write_text(content, encoding="utf-8")
    validate_release_notes(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
