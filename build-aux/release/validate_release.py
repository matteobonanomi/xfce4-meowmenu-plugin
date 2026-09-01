#!/usr/bin/env python3
"""Fail-closed validation and retry decisions for MeowMenu releases."""

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tarfile
from dataclasses import asdict, dataclass
from enum import Enum
from pathlib import Path


VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(?:-rc\d+)?$")


class ReleaseValidationError(ValueError):
    """A release input violates the publication contract."""


class LookupState(str, Enum):
    """Result of an authoritative remote release lookup."""

    ABSENT = "absent"
    FOUND = "found"


class InventoryState(str, Enum):
    """Relationship between prepared and already-published assets."""

    MISSING = "missing"
    IDENTICAL = "identical"
    INCOMPLETE = "incomplete"
    CONFLICT = "conflict"
    EXTRA = "extra"


class RemoteState(str, Enum):
    """Validated release state exposed to publication orchestration."""

    ABSENT = "absent"
    MATCHING_DRAFT = "matching-draft"
    CONFLICTING_DRAFT = "conflicting-draft"
    MATCHING_PUBLIC = "matching-public"
    CONFLICTING_PUBLIC = "conflicting-public"
    UNAVAILABLE = "unavailable"


class PublicationAction(str, Enum):
    """Only mutations permitted after complete remote comparison."""

    CREATE_DRAFT = "create-draft"
    RESUME_DRAFT = "resume-draft"
    PUBLISH_DRAFT = "publish-draft"
    SKIP_PUBLIC = "skip-public"
    FAIL = "fail"


@dataclass(frozen=True)
class CandidateIdentity:
    """Immutable local identity that every remote field must match."""

    version: str
    tag: str
    peeled_commit: str
    title: str
    body: str
    prerelease: bool = False

    @classmethod
    def from_mapping(cls, value):
        return cls(
            version=value["version"],
            tag=value["tag"],
            peeled_commit=value["peeled_commit"],
            title=value["title"],
            body=value["body"],
            prerelease=value.get("prerelease", False),
        )


@dataclass(frozen=True)
class RemoteRelease:
    """Normalized GitHub release metadata, including the peeled tag commit."""

    tag: str
    peeled_commit: str
    title: str
    body: str
    draft: bool
    prerelease: bool

    @classmethod
    def from_mapping(cls, value):
        return cls(
            tag=value.get("tag_name", value.get("tagName", value.get("tag", ""))),
            peeled_commit=value.get(
                "peeledCommit", value.get("peeled_commit", "")
            ),
            title=value.get("name", value.get("title", "")),
            body=value.get("body") or "",
            draft=value.get("draft", value.get("isDraft", True)),
            prerelease=value.get(
                "prerelease", value.get("isPrerelease", False)
            ),
        )


@dataclass(frozen=True)
class ReleaseDecision:
    """Pure result consumed by the workflow before any remote mutation."""

    remote_state: str
    action: str
    missing_assets: tuple = ()
    reason: str = ""

    def to_mapping(self):
        value = asdict(self)
        value["missing_assets"] = list(self.missing_assets)
        return value


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


def classify_lookup(http_status: int, command_exit: int):
    """Treat only an authoritative 404 as absence; all uncertainty fails."""
    if http_status == 404:
        return LookupState.ABSENT
    if command_exit == 0 and 200 <= http_status < 300:
        return LookupState.FOUND
    raise ReleaseValidationError(
        "Remote release lookup unavailable "
        f"(HTTP {http_status or 'unknown'}, command exit {command_exit})"
    )


def parse_api_response(response: Path, command_exit: int):
    """Parse `gh api --include` output without hiding transport diagnostics."""
    content = response.read_bytes()
    matches = list(re.finditer(rb"(?m)^HTTP/[^ ]+ ([0-9]{3})[^\r\n]*\r?$", content))
    if not matches:
        classify_lookup(0, command_exit)
    status_match = matches[-1]
    status = int(status_match.group(1))
    header_end = re.search(rb"\r?\n\r?\n", content[status_match.end():])
    if header_end is None:
        body = b""
    else:
        body = content[status_match.end() + header_end.end():]
    state = classify_lookup(status, command_exit)
    if state == LookupState.ABSENT:
        return {"lookup": state.value}
    try:
        release = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ReleaseValidationError("Remote release response is not valid JSON") from error
    return {"lookup": state.value, "release": release}


def compare_release_metadata(candidate: CandidateIdentity, remote: RemoteRelease):
    """Return every identity conflict before assets are downloaded or changed."""
    conflicts = []
    fields = (
        ("tag", candidate.tag, remote.tag),
        ("peeled commit", candidate.peeled_commit, remote.peeled_commit),
        ("title", candidate.title, remote.title),
        ("notes", candidate.body, remote.body),
        ("prerelease", candidate.prerelease, remote.prerelease),
    )
    for label, expected, actual in fields:
        if expected != actual:
            conflicts.append(label)
    return conflicts


def decide_publication(candidate, lookup, inventory_state, missing_assets=(), remote=None):
    """Choose one fail-closed action from validated metadata and inventory."""
    if lookup == LookupState.ABSENT:
        return ReleaseDecision(
            RemoteState.ABSENT.value,
            PublicationAction.CREATE_DRAFT.value,
            tuple(sorted(missing_assets)),
        )
    if remote is None:
        raise ReleaseValidationError("Found release has no metadata")
    conflicts = compare_release_metadata(candidate, remote)
    if conflicts:
        state = (
            RemoteState.CONFLICTING_DRAFT
            if remote.draft
            else RemoteState.CONFLICTING_PUBLIC
        )
        return ReleaseDecision(
            state.value,
            PublicationAction.FAIL.value,
            reason="Release metadata differs: " + ", ".join(conflicts),
        )
    if inventory_state in (InventoryState.CONFLICT, InventoryState.EXTRA):
        state = (
            RemoteState.CONFLICTING_DRAFT
            if remote.draft
            else RemoteState.CONFLICTING_PUBLIC
        )
        return ReleaseDecision(
            state.value,
            PublicationAction.FAIL.value,
            reason=f"Remote asset inventory is {inventory_state.value}",
        )
    if remote.draft:
        action = (
            PublicationAction.PUBLISH_DRAFT
            if inventory_state == InventoryState.IDENTICAL
            else PublicationAction.RESUME_DRAFT
        )
        return ReleaseDecision(
            RemoteState.MATCHING_DRAFT.value,
            action.value,
            tuple(sorted(missing_assets)),
        )
    if inventory_state == InventoryState.IDENTICAL:
        return ReleaseDecision(
            RemoteState.MATCHING_PUBLIC.value,
            PublicationAction.SKIP_PUBLIC.value,
        )
    return ReleaseDecision(
        RemoteState.CONFLICTING_PUBLIC.value,
        PublicationAction.FAIL.value,
        reason="Public release has an incomplete asset inventory",
    )


def _load_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def _write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _candidate_command(args):
    validate_release_notes(args.notes)
    candidate = CandidateIdentity(
        version=args.version,
        tag=args.tag,
        peeled_commit=args.peeled_commit,
        title=args.title,
        body=args.notes.read_text(encoding="utf-8"),
        prerelease=False,
    )
    release_presentation(candidate.version)
    if candidate.tag != f"v{candidate.version}":
        raise ReleaseValidationError("Candidate tag does not match version")
    _write_json(args.output, asdict(candidate))


def _preflight_command(args):
    from artifacts import compare_inventory_metadata, local_inventory, remote_inventory

    candidate = CandidateIdentity.from_mapping(_load_json(args.candidate))
    lookup_document = _load_json(args.lookup)
    lookup = LookupState(lookup_document["lookup"])
    local = local_inventory(args.local_assets)
    if lookup == LookupState.ABSENT:
        result = decide_publication(
            candidate, lookup, InventoryState.MISSING, local.keys()
        )
        _write_json(args.output, result.to_mapping())
        return
    remote_mapping = lookup_document["release"]
    remote = RemoteRelease.from_mapping(remote_mapping)
    conflicts = compare_release_metadata(candidate, remote)
    if conflicts:
        raise ReleaseValidationError(
            "Remote release metadata differs: " + ", ".join(conflicts)
        )
    comparison = compare_inventory_metadata(local, remote_inventory(remote_mapping))
    if comparison.state in (InventoryState.CONFLICT, InventoryState.EXTRA):
        raise ReleaseValidationError(comparison.reason)
    if not remote.draft and comparison.missing:
        raise ReleaseValidationError("Public release inventory is incomplete")
    _write_json(
        args.output,
        {
            "remote_state": (
                RemoteState.MATCHING_DRAFT.value
                if remote.draft
                else RemoteState.MATCHING_PUBLIC.value
            ),
            "inventory_state": comparison.state.value,
            "missing_assets": list(comparison.missing),
        },
    )


def _decision_command(args):
    from artifacts import compare_inventories, local_inventory

    candidate = CandidateIdentity.from_mapping(_load_json(args.candidate))
    lookup_document = _load_json(args.lookup)
    lookup = LookupState(lookup_document["lookup"])
    local = local_inventory(args.local_assets)
    if lookup == LookupState.ABSENT:
        decision = decide_publication(
            candidate, lookup, InventoryState.MISSING, local.keys()
        )
    else:
        remote = RemoteRelease.from_mapping(lookup_document["release"])
        comparison = compare_inventories(local, local_inventory(args.remote_assets))
        decision = decide_publication(
            candidate,
            lookup,
            comparison.state,
            comparison.missing,
            remote,
        )
    _write_json(args.output, decision.to_mapping())
    if decision.action == PublicationAction.FAIL.value:
        raise ReleaseValidationError(decision.reason)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command")

    candidate = subparsers.add_parser("candidate")
    candidate.add_argument("--version", required=True)
    candidate.add_argument("--tag", required=True)
    candidate.add_argument("--peeled-commit", required=True)
    candidate.add_argument("--title", required=True)
    candidate.add_argument("--notes", type=Path, required=True)
    candidate.add_argument("--output", type=Path, required=True)

    response = subparsers.add_parser("classify-response")
    response.add_argument("--response", type=Path, required=True)
    response.add_argument("--exit-code", type=int, required=True)
    response.add_argument("--output", type=Path, required=True)

    preflight = subparsers.add_parser("preflight")
    preflight.add_argument("--candidate", type=Path, required=True)
    preflight.add_argument("--lookup", type=Path, required=True)
    preflight.add_argument("--local-assets", type=Path, required=True)
    preflight.add_argument("--output", type=Path, required=True)

    decision = subparsers.add_parser("decide")
    decision.add_argument("--candidate", type=Path, required=True)
    decision.add_argument("--lookup", type=Path, required=True)
    decision.add_argument("--local-assets", type=Path, required=True)
    decision.add_argument("--remote-assets", type=Path)
    decision.add_argument("--output", type=Path, required=True)

    # Preserve the original validation interface for local callers.
    parser.add_argument("--version")
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--notes", type=Path)
    parser.add_argument("--state-json", type=Path)

    args = parser.parse_args()
    try:
        if args.command == "candidate":
            _candidate_command(args)
        elif args.command == "classify-response":
            _write_json(args.output, parse_api_response(args.response, args.exit_code))
        elif args.command == "preflight":
            _preflight_command(args)
        elif args.command == "decide":
            if (
                args.remote_assets is None
                and _load_json(args.lookup)["lookup"] == LookupState.FOUND.value
            ):
                raise ReleaseValidationError("Found release requires downloaded assets")
            _decision_command(args)
        else:
            if not all((args.version, args.assets, args.notes, args.state_json)):
                parser.error("local validation requires version, assets, notes, and state")
            validate_assets(args.assets, args.version)
            validate_release_notes(args.notes)
            validate_release_state(
                json.loads(args.state_json.read_text(encoding="utf-8")),
                args.version,
            )
    except ReleaseValidationError as error:
        print(f"release validation: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
