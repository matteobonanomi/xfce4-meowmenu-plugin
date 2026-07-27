#!/usr/bin/env python3
"""Render complete release-candidate notes from the top NEWS entry."""

import argparse
import re
from pathlib import Path

from validate_release import validate_release_notes


HEADER_RE = re.compile(r"^\d+\.\d+\.\d+(?:-rc\d+)? \(\d{4}-\d{2}-\d{2}\)$")


def top_news_body(news: Path, version: str):
    """Return the body belonging to the exact top NEWS entry."""
    lines = news.read_text(encoding="utf-8").splitlines()
    header_index = next((i for i, line in enumerate(lines) if HEADER_RE.match(line)), None)
    if header_index is None or not lines[header_index].startswith(f"{version} ("):
        raise ValueError(f"Top NEWS entry does not match {version}")
    body = []
    for line in lines[header_index + 1:]:
        if HEADER_RE.match(line):
            break
        if set(line.strip()) == {"="}:
            continue
        if line.strip():
            body.append(line)
    if not body:
        raise ValueError("Top NEWS entry is empty")
    return "\n".join(body)


def render(news: Path, version: str):
    """Render the stable public sections required for a candidate."""
    changes = top_news_body(news, version)
    return f"""# MeowMenu {version}

## Release candidate

This is MeowMenu's first release candidate on the path to 1.0.0. It is a
prerelease for testing and may still change before the first stable release.

## Changes since 0.8.0

{changes}

## Known limitations

Wayland remains experimental and unverified for this candidate. Maintainer
binaries and live testing currently target x86_64/amd64. See
[Known limitations](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/known-limitations/).

## Artifacts and integrity

The release provides Ubuntu 26.04 and Debian 13 DEBs, a Fedora 44 RPM, the
canonical source archive, and `SHA256SUMS`. Download all required files and run
`sha256sum -c SHA256SUMS` before installation.

## Verification

Package builds, installs, metadata, and automated tests are separate from live
desktop checks. See the release-specific
[support matrix](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/support/)
for the evidence available for each environment.

## Upgrade

Upgrades from 0.8.0 are intended to retain the panel item, preferences,
favourites, Calculator choices, and saved custom presets. Back up important
desktop configuration before testing a prerelease.

## Feedback and security

Report ordinary problems through the
[issue tracker](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues).
Report suspected vulnerabilities privately using GitHub private vulnerability
reporting, following the repository security policy.
"""


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
