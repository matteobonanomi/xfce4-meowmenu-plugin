#!/usr/bin/env python3
"""Create and verify canonical release assets."""

import argparse
import gzip
import hashlib
import json
import subprocess
from dataclasses import asdict, dataclass
from pathlib import Path

from validate_release import (
    InventoryState,
    ReleaseValidationError,
    expected_payload_names,
    validate_archive,
    validate_assets,
)


@dataclass(frozen=True)
class AssetMetadata:
    """Comparable local or remote asset identity."""

    name: str
    size: int
    sha256: str = None
    api_url: str = None


@dataclass(frozen=True)
class InventoryComparison:
    """Exact relationship between local and remote asset inventories."""

    state: InventoryState
    missing: tuple = ()
    conflicts: tuple = ()
    extra: tuple = ()
    reason: str = ""


def _normalize_digest(value):
    if not value:
        return None
    return value.removeprefix("sha256:")


def local_inventory(asset_dir: Path):
    """Hash every regular file in an inventory directory."""
    return {
        item.name: AssetMetadata(
            name=item.name,
            size=item.stat().st_size,
            sha256=hashlib.sha256(item.read_bytes()).hexdigest(),
        )
        for item in asset_dir.iterdir()
        if item.is_file()
    }


def remote_inventory(release):
    """Normalize GitHub asset metadata without assuming digest availability."""
    inventory = {}
    for asset in release.get("assets", ()):
        name = asset.get("name", "")
        if not name or "/" in name or name in inventory:
            raise ReleaseValidationError("Remote asset names are invalid or duplicated")
        inventory[name] = AssetMetadata(
            name=name,
            size=asset.get("size", -1),
            sha256=_normalize_digest(asset.get("digest")),
            api_url=asset.get("url"),
        )
    return inventory


def compare_inventories(local, remote):
    """Compare complete SHA-256 inventories before publication mutation."""
    local_names = set(local)
    remote_names = set(remote)
    extra = tuple(sorted(remote_names - local_names))
    if extra:
        return InventoryComparison(
            InventoryState.EXTRA,
            extra=extra,
            reason=f"Remote inventory has extra assets: {list(extra)}",
        )
    conflicts = tuple(
        sorted(
            name
            for name in local_names & remote_names
            if local[name].size != remote[name].size
            or local[name].sha256 != remote[name].sha256
        )
    )
    if conflicts:
        return InventoryComparison(
            InventoryState.CONFLICT,
            conflicts=conflicts,
            reason=f"Remote assets differ: {list(conflicts)}",
        )
    missing = tuple(sorted(local_names - remote_names))
    if not remote_names:
        return InventoryComparison(
            InventoryState.MISSING,
            missing=missing,
            reason="Remote release has no assets",
        )
    if missing:
        return InventoryComparison(
            InventoryState.INCOMPLETE,
            missing=missing,
            reason=f"Remote inventory is missing: {list(missing)}",
        )
    return InventoryComparison(InventoryState.IDENTICAL)


def compare_inventory_metadata(local, remote):
    """Reject names, sizes, or published digests that already conflict."""
    comparison = compare_inventories(
        local,
        {
            name: AssetMetadata(
                name=asset.name,
                size=asset.size,
                sha256=asset.sha256 or local.get(name, asset).sha256,
                api_url=asset.api_url,
            )
            for name, asset in remote.items()
        },
    )
    # Missing GitHub digests cannot prove equality. Exact downloaded bytes are
    # still required before an existing draft or public release is accepted.
    if (
        comparison.state == InventoryState.IDENTICAL
        and any(not asset.sha256 for asset in remote.values())
    ):
        return InventoryComparison(
            InventoryState.INCOMPLETE,
            reason="Remote metadata requires downloaded checksum verification",
        )
    return comparison


def write_inventory(asset_dir: Path, output: Path):
    """Persist stable inventory metadata for diagnostics and comparisons."""
    entries = [asdict(item) for item in local_inventory(asset_dir).values()]
    output.write_text(
        json.dumps(sorted(entries, key=lambda item: item["name"]), indent=2) + "\n",
        encoding="utf-8",
    )
    return output


def create_source_archive(
    repository: Path,
    version: str,
    output: Path,
    *,
    tag: str = None,
    ref: str = None,
):
    """Archive one immutable tag or commit with a stable, normalized layout."""
    if (tag is None) == (ref is None):
        raise ReleaseValidationError("Specify exactly one source tag or ref")
    if tag is not None:
        expected_tag = f"v{version}"
        if tag != expected_tag:
            raise ReleaseValidationError(f"Tag {tag} does not match {expected_tag}")
        ref = tag
    commit = subprocess.run(
        ["git", "rev-parse", f"{ref}^{{commit}}"],
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
    if len(expected) != 4:
        raise ReleaseValidationError("Release contract must define four payloads")
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
    source_ref = source.add_mutually_exclusive_group(required=True)
    source_ref.add_argument("--tag")
    source_ref.add_argument("--ref")
    source.add_argument("--version", required=True)
    source.add_argument("--output", type=Path, required=True)

    checksums = subparsers.add_parser("checksums")
    checksums.add_argument("--assets", type=Path, required=True)
    checksums.add_argument("--version", required=True)

    verify = subparsers.add_parser("verify")
    verify.add_argument("--assets", type=Path, required=True)
    verify.add_argument("--version", required=True)

    inventory = subparsers.add_parser("inventory")
    inventory.add_argument("--assets", type=Path, required=True)
    inventory.add_argument("--output", type=Path, required=True)

    args = parser.parse_args()
    if args.command == "create-source":
        create_source_archive(
            args.repository,
            args.version,
            args.output,
            tag=args.tag,
            ref=args.ref,
        )
    elif args.command == "checksums":
        generate_checksums(args.assets, args.version)
    elif args.command == "inventory":
        write_inventory(args.assets, args.output)
    else:
        validate_assets(args.assets, args.version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
