#!/usr/bin/env python3
"""Check release-facing repository text for stale internal presentation."""

import argparse
import os
import re
import subprocess
from pathlib import Path


TEXT_SUFFIXES = {
    "", ".c", ".cc", ".cpp", ".h", ".in", ".md", ".py", ".sh",
    ".xml", ".yml", ".yaml", ".po", ".pot",
}
SELF_EXCLUDES = {
    "build-aux/repository-presentation.py",
    "tests/test_repository_presentation.py",
}
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
FALLBACK_EXCLUDED_FILES = {".codex", "AGENTS.md", "CLAUDE.md"}


def fallback_repository_files(root: Path):
    """Walk source text while pruning private and generated trees."""
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
            relative = path.relative_to(root).as_posix()
            if (
                    relative not in FALLBACK_EXCLUDED_FILES
                    and path.suffix in TEXT_SUFFIXES
                    and path.is_file()):
                yield relative, path


def repository_files(root: Path):
    """List cached and new non-ignored text files in the current worktree."""
    try:
        result = subprocess.run(
            [
                "git", "ls-files", "--cached", "--others",
                "--exclude-standard", "-z",
            ],
            cwd=root,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except OSError:
        result = None

    if result is not None and result.returncode == 0:
        for raw in result.stdout.split(b"\0"):
            if not raw:
                continue
            relative = raw.decode("utf-8")
            path = root / relative
            if path.is_file() and path.suffix in TEXT_SUFFIXES:
                yield relative, path
        return

    yield from fallback_repository_files(root)


def violations(root: Path):
    """Return presentation-policy violations with bounded public allowances."""
    errors = []
    forbidden = (
        ("private workflow path", re.compile(r"\." + r"specify/")),
        ("requirement identifier", re.compile(r"\b(?:FR|SC)-\d{3}\b")),
        ("task identifier", re.compile(r"\bT\d{3}\b")),
        ("obsolete helper", re.compile(
            r"\b(?:dev-install|dev-uninstall|dev-reload)\.sh\b|"
            r"tools/news-version\.py"
        )),
        ("stale contributor URL", re.compile(
            r"github\.com/matteob/xfce4-meowmenu-plugin"
        )),
        ("stale current tracker", re.compile(
            r"gitlab\.xfce\.org/[^\s)]*xfce4-meowmenu-plugin"
        )),
        ("retired coexistence gate", re.compile(
            r"whisker-overlap-check|whisker-overlap\.md"
        )),
    )
    method_terms = re.compile(
        r"Spec-" + r"Kit|spec-" + r"driven|coding " + r"agent|"
        r"\b(?:Codex|Claude)\b"
    )
    assistance_terms = re.compile(r"\bA" + r"I\b|LLM")

    for relative, path in repository_files(root):
        if relative in SELF_EXCLUDES:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        if relative == ".gitignore":
            content = "\n".join(
                line for line in content.splitlines()
                if not line.startswith((".specify/", ".agents/"))
            )
        for label, expression in forbidden:
            for match in expression.finditer(content):
                line = content.count("\n", 0, match.start()) + 1
                errors.append(f"{relative}:{line}: {label}: {match.group(0)}")
        for match in method_terms.finditer(content):
            line = content.count("\n", 0, match.start()) + 1
            errors.append(f"{relative}:{line}: internal methodology: {match.group(0)}")
        if assistance_terms.search(content):
            allowed = relative == "README.md" or (
                relative.startswith("po/") and relative.endswith(".po")
            )
            if not allowed:
                errors.append(f"{relative}: assistance disclosure is not allowed here")

    readme = (root / "README.md").read_text(encoding="utf-8")
    disclosure = (
        "MeowMenu was developed with AI assistance; every change is "
        "maintainer-reviewed."
    )
    if readme.count(disclosure) != 1:
        errors.append("README.md: approved assistance disclosure must occur exactly once")
    for package_file in (
        "debian/control",
        "dist/rpm/xfce4-meowmenu-plugin.spec",
        "dist/arch/PKGBUILD",
        "data/metainfo/io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml",
    ):
        content = (root / package_file).read_text(encoding="utf-8")
        if "Built for Xubuntu" in content:
            errors.append(f"{package_file}: permanent description is distro-specific")
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
