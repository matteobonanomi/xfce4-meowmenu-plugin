#!/usr/bin/env python3
"""Validate maintained repository-relative Markdown links and helper names."""

import argparse
import re
import subprocess
from pathlib import Path
from urllib.parse import unquote


LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
OBSOLETE_RE = re.compile(
    r"\b(?:dev-install|dev-uninstall|dev-reload)\.sh\b|tools/news-version\.py"
)


def markdown_files(root: Path):
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "*.md"],
        cwd=root,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    for relative in result.stdout.splitlines():
        path = root / relative
        if path.is_file():
            yield relative, path


def resolve_link(root: Path, document: Path, target: str):
    """Resolve repository files and extensionless Jekyll page routes."""
    target = target.strip().split(maxsplit=1)[0].strip("<>")
    if not target or target.startswith(("#", "http://", "https://", "mailto:")):
        return True
    clean = unquote(target.split("#", 1)[0].split("?", 1)[0])
    if not clean:
        return True
    candidate = (root / clean.lstrip("/")) if clean.startswith("/") else (
        document.parent / clean
    )
    candidates = [candidate]
    if not candidate.suffix:
        candidates.extend((candidate.with_suffix(".md"), candidate / "index.md"))
    return any(item.exists() for item in candidates)


def violations(root: Path):
    errors = []
    for relative, document in markdown_files(root):
        content = document.read_text(encoding="utf-8")
        for match in LINK_RE.finditer(content):
            if not resolve_link(root, document, match.group(1)):
                line = content.count("\n", 0, match.start()) + 1
                errors.append(f"{relative}:{line}: missing link target {match.group(1)}")
        obsolete = OBSOLETE_RE.search(content)
        if obsolete:
            errors.append(f"{relative}: obsolete helper {obsolete.group(0)}")
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    errors = violations(args.root)
    if errors:
        raise SystemExit("\n".join(errors))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
