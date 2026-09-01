"""Reusable fixtures for release-helper tests."""

import hashlib
import subprocess
import tarfile
from dataclasses import dataclass
from pathlib import Path


PUBLIC_VERSION = "0.9.0-rc1"
TAG = f"v{PUBLIC_VERSION}"
NEWS_BODY = "- Candidate."


@dataclass(frozen=True)
class ReleaseFixture:
    """Describe one release independently of version and Git tag kind."""

    version: str = PUBLIC_VERSION
    date: str = "2026-07-23"
    news_body: str = NEWS_BODY
    tag_kind: str = "annotated"

    @property
    def tag(self):
        return f"v{self.version}"

    @property
    def archive_root(self):
        return f"xfce4-meowmenu-plugin-{self.version}"

    @property
    def payload_names(self):
        return payload_names(self.version)


def payload_names(version=PUBLIC_VERSION):
    """Return the exact four versioned release payload names."""
    return (
        f"xfce4-meowmenu-plugin_{version}_ubuntu26.04_amd64.deb",
        f"xfce4-meowmenu-plugin_{version}_debian13_amd64.deb",
        f"xfce4-meowmenu-plugin-{version}-1.fc44.x86_64.rpm",
        f"xfce4-meowmenu-plugin-{version}.tar.gz",
    )


PAYLOAD_NAMES = payload_names()


def write_news(path: Path, version=PUBLIC_VERSION, date="2026-07-23",
               body=NEWS_BODY):
    """Write a top NEWS entry while preserving its supplied body literally."""
    normalized_body = body.rstrip("\n")
    path.write_text(
        f"{version} ({date})\n=====\n{normalized_body}\n",
        encoding="utf-8",
    )
    return path


def create_git_repository(path: Path, release=ReleaseFixture()):
    """Create a main-branch repository containing the requested release."""
    subprocess.run(["git", "init", "-q", "-b", "main"], cwd=path, check=True)
    subprocess.run(["git", "config", "user.email", "tests@example.invalid"],
                   cwd=path, check=True)
    subprocess.run(["git", "config", "user.name", "Release Tests"],
                   cwd=path, check=True)
    write_news(
        path / "NEWS",
        version=release.version,
        date=release.date,
        body=release.news_body,
    )
    subprocess.run(["git", "add", "NEWS"], cwd=path, check=True)
    subprocess.run(["git", "commit", "-q", "-m", "Initial"], cwd=path, check=True)
    return path


def create_tag(repository: Path, release=ReleaseFixture()):
    """Create the requested annotated or lightweight release tag."""
    if release.tag_kind == "annotated":
        command = ["git", "tag", "-a", release.tag, "-m", release.version]
    elif release.tag_kind == "lightweight":
        command = ["git", "tag", release.tag]
    else:
        raise ValueError(f"Unsupported tag kind: {release.tag_kind}")
    subprocess.run(command, cwd=repository, check=True)
    return release.tag


def create_source_archive(path: Path, version=PUBLIC_VERSION,
                          date="2026-07-23", body=NEWS_BODY,
                          archive_root=None):
    """Create a release archive with a configurable stable top-level root."""
    root = archive_root or f"xfce4-meowmenu-plugin-{version}"
    source = path / root
    source.mkdir()
    write_news(source / "NEWS", version=version, date=date, body=body)
    archive = path / f"xfce4-meowmenu-plugin-{version}.tar.gz"
    with tarfile.open(archive, "w:gz") as output:
        output.add(source, arcname=source.name)
    return archive


def create_payloads(path: Path, release=ReleaseFixture(), names=None,
                    archive_root=None):
    """Create exactly four payloads for an arbitrary RC or stable release."""
    names = tuple(names or release.payload_names)
    if len(names) != 4:
        raise ValueError("A release fixture requires exactly four payload names")
    for name in names[:-1]:
        (path / name).write_bytes(name.encode("utf-8"))
    archive = create_source_archive(
        path,
        version=release.version,
        date=release.date,
        body=release.news_body,
        archive_root=archive_root or release.archive_root,
    )
    expected_archive = path / names[-1]
    if archive != expected_archive:
        archive.rename(expected_archive)
    return tuple(path / name for name in names)


def write_checksums(path: Path, payloads):
    lines = []
    for payload in sorted(payloads, key=lambda item: item.name):
        digest = hashlib.sha256(payload.read_bytes()).hexdigest()
        lines.append(f"{digest}  {payload.name}\n")
    manifest = path / "SHA256SUMS"
    manifest.write_text("".join(lines), encoding="utf-8")
    return manifest


def candidate_identity(release=ReleaseFixture(), commit="a" * 40):
    """Return the normalized identity prepared by release automation."""
    return {
        "version": release.version,
        "tag": release.tag,
        "peeled_commit": commit,
        "title": f"MeowMenu {release.version}",
        "body": release.news_body,
        "prerelease": False,
    }


def remote_release(release=ReleaseFixture(), commit="a" * 40, *, draft=True,
                   assets=()):
    """Return GitHub-shaped metadata for a matching release fixture."""
    return {
        "tag_name": release.tag,
        "peeledCommit": commit,
        "name": f"MeowMenu {release.version}",
        "body": release.news_body,
        "draft": draft,
        "prerelease": False,
        "assets": list(assets),
    }
