#!/usr/bin/env python3
"""Check release-facing repository text for stale internal presentation."""

import argparse
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


def repository_files(root: Path):
    """List cached and new non-ignored text files in the current worktree."""
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root,
        check=True,
        stdout=subprocess.PIPE,
    )
    for raw in result.stdout.split(b"\0"):
        if not raw:
            continue
        relative = raw.decode("utf-8")
        path = root / relative
        if path.is_file() and path.suffix in TEXT_SUFFIXES:
            yield relative, path


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
