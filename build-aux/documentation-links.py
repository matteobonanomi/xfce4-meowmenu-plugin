#!/usr/bin/env python3
"""Validate maintained repository-relative Markdown links and helper names."""

import argparse
import os
import re
import subprocess
from pathlib import Path
from urllib.parse import unquote


LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
OBSOLETE_RE = re.compile(
    r"\b(?:dev-install|dev-uninstall|dev-reload)\.sh\b|tools/news-version\.py"
)
FALLBACK_EXCLUDED_DIRS = {
    ".agents",
    ".claude",
    ".codex",
    ".git",
    ".idea",
    ".logs",
    ".specify",
    ".vscode",
    ".venv",
    "__pycache__",
    "_build",
    "bin",
    "build",
    "obj",
    "out",
    "venv",
}


def fallback_markdown_files(root: Path):
    """Walk Markdown sources while pruning private and generated trees."""
    for current, directories, filenames in os.walk(root):
        current_path = Path(current)
        kept = []
        for directory in directories:
            candidate = current_path / directory
            private_docs = current_path == root / "dev" and directory == "docs"
            meson_output = (candidate / "meson-private").is_dir()
            if (
                    directory not in FALLBACK_EXCLUDED_DIRS
                    and not private_docs
                    and not meson_output):
                kept.append(directory)
        directories[:] = sorted(kept)
        for filename in sorted(filenames):
            path = current_path / filename
            if path.suffix == ".md" and path.is_file():
                yield path.relative_to(root).as_posix(), path


def markdown_files(root: Path):
    """List Markdown from a checkout or a gitless release source tree."""
    try:
        result = subprocess.run(
            [
                "git", "ls-files", "--cached", "--others",
                "--exclude-standard", "*.md",
            ],
            cwd=root,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except OSError:
        result = None

    if result is not None and result.returncode == 0:
        for relative in result.stdout.splitlines():
            path = root / relative
            if path.is_file():
                yield relative, path
        return

    yield from fallback_markdown_files(root)


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
