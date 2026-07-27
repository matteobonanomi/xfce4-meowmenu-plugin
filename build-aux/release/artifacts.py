#!/usr/bin/env python3
"""Create and verify canonical release assets."""

import argparse
import gzip
import hashlib
import subprocess
from pathlib import Path

from validate_release import (
    ReleaseValidationError,
    expected_payload_names,
    validate_archive,
    validate_assets,
)


def create_source_archive(repository: Path, tag: str, version: str, output: Path):
    """Archive the peeled tag with one stable root and normalized gzip time."""
    expected_tag = f"v{version}"
    if tag != expected_tag:
        raise ReleaseValidationError(f"Tag {tag} does not match {expected_tag}")
    commit = subprocess.run(
        ["git", "rev-parse", f"{tag}^{{commit}}"],
        cwd=repository,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout.strip()
    prefix = f"xfce4-meowmenu-plugin-{version}/"
    archive = subprocess.run(
        ["git", "archive", "--format=tar", f"--prefix={prefix}", commit],
        cwd=repository,
        check=True,
        stdout=subprocess.PIPE,
    ).stdout
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(gzip.compress(archive, compresslevel=9, mtime=0))
    validate_archive(output, version)
    return output


def generate_checksums(asset_dir: Path, version: str):
    """Write a sorted manifest for exactly the four required payloads."""
    expected = expected_payload_names(version)
    present = {item.name for item in asset_dir.iterdir() if item.is_file()}
    missing = expected - present
    if missing:
        raise ReleaseValidationError(f"Cannot checksum missing payloads: {sorted(missing)}")
    unexpected = present - expected - {"SHA256SUMS"}
    if unexpected:
        raise ReleaseValidationError(f"Unexpected release payloads: {sorted(unexpected)}")
    lines = []
    for name in sorted(expected):
        digest = hashlib.sha256((asset_dir / name).read_bytes()).hexdigest()
        lines.append(f"{digest}  {name}\n")
    manifest = asset_dir / "SHA256SUMS"
    manifest.write_text("".join(lines), encoding="utf-8")
    validate_assets(asset_dir, version)
    return manifest


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    source = subparsers.add_parser("create-source")
    source.add_argument("--repository", type=Path, default=Path.cwd())
    source.add_argument("--tag", required=True)
    source.add_argument("--version", required=True)
    source.add_argument("--output", type=Path, required=True)

    checksums = subparsers.add_parser("checksums")
    checksums.add_argument("--assets", type=Path, required=True)
    checksums.add_argument("--version", required=True)

    verify = subparsers.add_parser("verify")
    verify.add_argument("--assets", type=Path, required=True)
    verify.add_argument("--version", required=True)

    args = parser.parse_args()
    if args.command == "create-source":
        create_source_archive(args.repository, args.tag, args.version, args.output)
    elif args.command == "checksums":
        generate_checksums(args.assets, args.version)
    else:
        validate_assets(args.assets, args.version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
