#!/usr/bin/env python3
"""Fail-closed validation for a prepared MeowMenu release."""

import argparse
import hashlib
import json
import re
import subprocess
import tarfile
from pathlib import Path


VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(?:-rc\d+)?$")


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


def resolve_tag_commit(repository: Path, tag: str):
    """Resolve either an annotated or lightweight tag to its commit."""
    run_git(repository, "show-ref", "--verify", f"refs/tags/{tag}")
    return run_git(repository, "rev-parse", f"{tag}^{{commit}}")


def validate_tag(repository: Path, tag: str, expected_tag: str, main_ref: str):
    """Require an exact release tag whose commit is reachable from main."""
    if tag != expected_tag:
        raise ReleaseValidationError(f"Tag {tag} does not match {expected_tag}")
    commit = resolve_tag_commit(repository, tag)
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
    if len(expected) != 4:
        raise ReleaseValidationError("Release contract must define four payloads")
    actual = {item.name for item in asset_dir.iterdir() if item.is_file()}
    if actual != expected | {"SHA256SUMS"}:
        missing = sorted((expected | {"SHA256SUMS"}) - actual)
        extra = sorted(actual - (expected | {"SHA256SUMS"}))
        raise ReleaseValidationError(f"Asset mismatch; missing={missing}, extra={extra}")
    validate_checksums(asset_dir / "SHA256SUMS", asset_dir, expected)


def validate_checksums(manifest: Path, asset_dir: Path, expected):
    lines = [line for line in manifest.read_text(encoding="utf-8").splitlines() if line]
    if len(lines) != 4:
        raise ReleaseValidationError("Checksum manifest must contain four payloads")
    names = []
    for line in lines:
        match = re.fullmatch(r"([0-9a-f]{64})  ([^/]+)", line)
        if not match:
            raise ReleaseValidationError(f"Invalid checksum line: {line}")
        digest, name = match.groups()
        if name == manifest.name:
            raise ReleaseValidationError("Checksum manifest must not name itself")
        names.append(name)
        payload = asset_dir / name
        if not payload.is_file():
            raise ReleaseValidationError(f"Checksum names missing asset: {name}")
        actual = hashlib.sha256(payload.read_bytes()).hexdigest()
        if actual != digest:
            raise ReleaseValidationError(f"Checksum mismatch: {name}")
    if set(names) != set(expected) or len(names) != len(set(names)):
        raise ReleaseValidationError("Checksum manifest does not cover payloads exactly")


def validate_release_notes(notes: Path, expected_body=None):
    """Require non-empty notes and optionally an exact literal NEWS body."""
    content = notes.read_text(encoding="utf-8")
    if not content.strip():
        raise ReleaseValidationError("Release notes are empty")
    if expected_body is not None and content != expected_body:
        raise ReleaseValidationError("Release notes do not match NEWS literally")


def release_presentation(version: str):
    """Describe the standard GitHub presentation for a valid release."""
    if not VERSION_RE.fullmatch(version):
        raise ReleaseValidationError(f"Unsupported release version: {version}")
    # The RC suffix communicates the project's stability channel, but it does
    # not make the GitHub release a technical prerelease. GitHub owns the
    # chronological latest marker because a version alone cannot establish
    # publication order.
    return {"prerelease": False}


def validate_release_state(state, version):
    """Require a public standard release with an explicit latest marker."""
    if state.get("draft", True):
        raise ReleaseValidationError("Release must be public")
    expected = release_presentation(version)
    if state.get("prerelease") != expected["prerelease"]:
        raise ReleaseValidationError("Release prerelease state does not match version")
    if not isinstance(state.get("latest"), bool):
        raise ReleaseValidationError(
            "Release latest state must be an explicit chronological marker"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--notes", type=Path, required=True)
    parser.add_argument("--state-json", type=Path, required=True)
    args = parser.parse_args()
    validate_assets(args.assets, args.version)
    validate_release_notes(args.notes)
    validate_release_state(
        json.loads(args.state_json.read_text(encoding="utf-8")),
        args.version,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
