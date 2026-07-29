#!/usr/bin/env python3
"""Fail-closed validation for a prepared MeowMenu release candidate."""

import argparse
import hashlib
import json
import re
import subprocess
import tarfile
from pathlib import Path


REQUIRED_NOTE_HEADINGS = (
    "Release candidate",
    "Changes since 0.8.0",
    "Known limitations",
    "Artifacts and integrity",
    "Verification",
    "Upgrade",
    "Feedback and security",
)


class ReleaseValidationError(ValueError):
    """A release input violates the publication contract."""


def run_git(repository: Path, *arguments):
    result = subprocess.run(
        ["git", *arguments],
        cwd=repository,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode:
        raise ReleaseValidationError(result.stderr.strip() or "Git command failed")
    return result.stdout.strip()


def validate_tag(repository: Path, tag: str, expected_tag: str, main_ref: str):
    """Require an exact annotated tag whose commit is reachable from main."""
    if tag != expected_tag:
        raise ReleaseValidationError(f"Tag {tag} does not match {expected_tag}")
    object_type = run_git(repository, "cat-file", "-t", f"refs/tags/{tag}")
    if object_type != "tag":
        raise ReleaseValidationError(f"{tag} is not an annotated tag")
    commit = run_git(repository, "rev-parse", f"{tag}^{{commit}}")
    result = subprocess.run(
        ["git", "merge-base", "--is-ancestor", commit, main_ref],
        cwd=repository,
        check=False,
    )
    if result.returncode:
        raise ReleaseValidationError(f"{tag} is not reachable from {main_ref}")
    return commit


def validate_archive(archive: Path, version: str):
    """Require one stable source root and a matching top NEWS version."""
    expected_root = f"xfce4-meowmenu-plugin-{version}"
    with tarfile.open(archive, "r:gz") as source:
        members = source.getmembers()
        roots = {member.name.split("/", 1)[0] for member in members}
        if roots != {expected_root}:
            raise ReleaseValidationError(
                f"Archive roots are {sorted(roots)}, expected {expected_root}"
            )
        news_member = next(
            (member for member in members if member.name == f"{expected_root}/NEWS"),
            None,
        )
        if news_member is None:
            raise ReleaseValidationError("Source archive has no top-level NEWS")
        news = source.extractfile(news_member).read().decode("utf-8")
    first = next((line.strip() for line in news.splitlines() if line.strip()), "")
    if not first.startswith(f"{version} ("):
        raise ReleaseValidationError("Source archive NEWS version does not match")


def expected_payload_names(version: str):
    return {
        f"xfce4-meowmenu-plugin_{version}_ubuntu26.04_amd64.deb",
        f"xfce4-meowmenu-plugin_{version}_debian13_amd64.deb",
        f"xfce4-meowmenu-plugin-{version}-1.fc44.x86_64.rpm",
        f"xfce4-meowmenu-plugin-{version}.tar.gz",
    }


def validate_assets(asset_dir: Path, version: str):
    """Require the exact four payloads plus their integrity manifest."""
    expected = expected_payload_names(version)
    actual = {item.name for item in asset_dir.iterdir() if item.is_file()}
    if actual != expected | {"SHA256SUMS"}:
        missing = sorted((expected | {"SHA256SUMS"}) - actual)
        extra = sorted(actual - (expected | {"SHA256SUMS"}))
        raise ReleaseValidationError(f"Asset mismatch; missing={missing}, extra={extra}")
    validate_checksums(asset_dir / "SHA256SUMS", asset_dir, expected)


def validate_checksums(manifest: Path, asset_dir: Path, expected):
    lines = [line for line in manifest.read_text(encoding="utf-8").splitlines() if line]
    names = []
    for line in lines:
        match = re.fullmatch(r"([0-9a-f]{64})  ([^/]+)", line)
        if not match:
            raise ReleaseValidationError(f"Invalid checksum line: {line}")
        digest, name = match.groups()
        names.append(name)
        payload = asset_dir / name
        if not payload.is_file():
            raise ReleaseValidationError(f"Checksum names missing asset: {name}")
        actual = hashlib.sha256(payload.read_bytes()).hexdigest()
        if actual != digest:
            raise ReleaseValidationError(f"Checksum mismatch: {name}")
    if set(names) != set(expected) or len(names) != len(set(names)):
        raise ReleaseValidationError("Checksum manifest does not cover payloads exactly")


def validate_release_notes(notes: Path):
    content = notes.read_text(encoding="utf-8")
    missing = [
        heading for heading in REQUIRED_NOTE_HEADINGS
        if f"## {heading}" not in content
    ]
    if missing:
        raise ReleaseValidationError(f"Release notes lack sections: {missing}")


def validate_release_state(state):
    if not state.get("draft", False):
        raise ReleaseValidationError("Release must remain a private draft")
    if not state.get("prerelease", False):
        raise ReleaseValidationError("Release must be marked as a prerelease")
    if state.get("latest", True):
        raise ReleaseValidationError("Release candidate must not be latest stable")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--notes", type=Path, required=True)
    parser.add_argument("--state-json", type=Path, required=True)
    args = parser.parse_args()
    validate_assets(args.assets, args.version)
    validate_release_notes(args.notes)
    validate_release_state(json.loads(args.state_json.read_text(encoding="utf-8")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
