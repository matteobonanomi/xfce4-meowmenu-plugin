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
VERSION_RE = re.compile(r"\d+\.\d+\.\d+(?:-rc\d+)?")
TOP_NEWS_RE = re.compile(
    r"^\s*(?P<version>\d+\.\d+\.\d+(?:-rc\d+)?)\s+"
    r"\(\d{4}-\d{2}-\d{2}\)\s*$"
)
CURRENT_VERSION_FILES = (
    "debian/changelog",
    "dist/rpm/xfce4-meowmenu-plugin.spec",
    "dist/arch/PKGBUILD",
    ".github/SECURITY.md",
    ".github/ISSUE_TEMPLATE/bug-report.yml",
    ".github/ISSUE_TEMPLATE/compatibility-report.yml",
    "data/metainfo/io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml",
)
CURRENT_GUIDANCE_FILES = (
    "README.md",
    "RELEASING.md",
    ".github/SECURITY.md",
    ".github/ISSUE_TEMPLATE/bug-report.yml",
    ".github/ISSUE_TEMPLATE/compatibility-report.yml",
    "docs",
    "dist/arch/README.md",
    "build-aux/arch/README.md",
    "dev/docs/ci.md",
)
RETIRED_GUIDANCE_RE = re.compile(
    r"(?i)one-time reset|resets exactly once|reset each pre-1\.0|"
    r"legacy[- ]key|sidebar-position\s*=\s*(?:top|bottom)|"
    r"retired (?:layout|sidebar|grid)"
)
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
MAINTAINER_MARKDOWN = {"dev/docs/ci.md"}


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
        tracked = {
            raw.decode("utf-8") for raw in result.stdout.split(b"\0") if raw
        }
        tracked.update(
            relative for relative in MAINTAINER_MARKDOWN
            if (root / relative).is_file()
        )
        for relative in sorted(tracked):
            path = root / relative
            if path.is_file() and path.suffix in TEXT_SUFFIXES:
                yield relative, path
        return

    discovered = dict(fallback_repository_files(root))
    for relative in MAINTAINER_MARKDOWN:
        path = root / relative
        if path.is_file():
            discovered[relative] = path
    yield from ((relative, discovered[relative])
                for relative in sorted(discovered))


def current_version(root: Path):
    """Read the canonical version from the first NEWS entry."""
    news = root / "NEWS"
    for line in news.read_text(encoding="utf-8").splitlines():
        match = TOP_NEWS_RE.match(line)
        if match:
            return match.group("version")
        if line.strip():
            break
    return None


def native_versions(version):
    """Return the package-native spellings for a NEWS version."""
    if "-rc" not in version:
        return {
            "debian": f"{version}-1",
            "rpm": version,
            "arch": version,
        }
    base, number = version.rsplit("-rc", maxsplit=1)
    return {
        "debian": f"{base}~rc{number}-1",
        "rpm": f"{base}~rc{number}",
        "arch": f"{base}rc{number}",
    }


def current_state_violations(root: Path):
    """Check current package metadata and public guidance without scanning history."""
    errors = []
    version = current_version(root)
    if version is None:
        return ["NEWS: malformed top release entry"]
    native = native_versions(version)

    patterns = {
        "debian/changelog": (r"^xfce4-meowmenu-plugin \(([^)]+)\)", "debian"),
        "dist/rpm/xfce4-meowmenu-plugin.spec": (
            r"^Version:\s+(\S+)",
            "rpm",
        ),
        "dist/arch/PKGBUILD": (r"^pkgver=(\S+)", "arch"),
    }
    for relative, (pattern, key) in patterns.items():
        path = root / relative
        if not path.is_file():
            continue
        match = re.search(pattern, path.read_text(encoding="utf-8"), re.MULTILINE)
        if match is None or match.group(1) != native[key]:
            found = match.group(1) if match else "missing"
            errors.append(
                f"{relative}: current version {found} does not match NEWS {native[key]}"
            )

    rpm = root / "dist/rpm/xfce4-meowmenu-plugin.spec"
    if rpm.is_file():
        rpm_match = re.search(
            r"^%global upstream_version\s+(\S+)",
            rpm.read_text(encoding="utf-8"),
            re.MULTILINE,
        )
        if rpm_match is None or rpm_match.group(1) != version:
            errors.append(f"{rpm}: upstream version does not match NEWS {version}")

    arch = root / "dist/arch/PKGBUILD"
    if arch.is_file():
        arch_match = re.search(
            r"^_upstream_version=(\S+)",
            arch.read_text(encoding="utf-8"),
            re.MULTILINE,
        )
        if arch_match is None or arch_match.group(1) != version:
            errors.append(f"{arch}: upstream version does not match NEWS {version}")

    for relative in CURRENT_VERSION_FILES:
        path = root / relative
        if not path.is_file():
            continue
        content = path.read_text(encoding="utf-8")
        if relative.endswith(".xml"):
            values = re.findall(r'<release\s+version="([^"]+)"', content)
        else:
            values = VERSION_RE.findall(content)
        for value in values:
            if value not in {version, "1.0.0"}:
                errors.append(f"{relative}: stale current version {value}")

    for relative in CURRENT_GUIDANCE_FILES:
        paths = [root / relative] if "." in Path(relative).name else sorted(
            (root / relative).glob("*.md")
        )
        for path in paths:
            if not path.is_file():
                continue
            match = RETIRED_GUIDANCE_RE.search(path.read_text(encoding="utf-8"))
            if match:
                errors.append(f"{path.relative_to(root)}: retired current guidance: {match.group(0)}")
    return errors


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

    errors.extend(current_state_violations(root))

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
