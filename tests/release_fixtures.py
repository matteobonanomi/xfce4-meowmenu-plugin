"""Reusable fixtures for release-helper tests."""

import hashlib
import subprocess
import tarfile
from pathlib import Path


PUBLIC_VERSION = "0.9.0-rc1"
TAG = f"v{PUBLIC_VERSION}"
PAYLOAD_NAMES = (
    f"xfce4-meowmenu-plugin_{PUBLIC_VERSION}_ubuntu26.04_amd64.deb",
    f"xfce4-meowmenu-plugin_{PUBLIC_VERSION}_debian13_amd64.deb",
    f"xfce4-meowmenu-plugin-{PUBLIC_VERSION}-1.fc44.x86_64.rpm",
    f"xfce4-meowmenu-plugin-{PUBLIC_VERSION}.tar.gz",
)


def write_news(path: Path, version=PUBLIC_VERSION, date="2026-07-23"):
    path.write_text(f"{version} ({date})\n=====\n- Candidate.\n", encoding="utf-8")
    return path


def create_git_repository(path: Path):
    subprocess.run(["git", "init", "-q", "-b", "main"], cwd=path, check=True)
    subprocess.run(["git", "config", "user.email", "tests@example.invalid"],
                   cwd=path, check=True)
    subprocess.run(["git", "config", "user.name", "Release Tests"],
                   cwd=path, check=True)
    write_news(path / "NEWS")
    subprocess.run(["git", "add", "NEWS"], cwd=path, check=True)
    subprocess.run(["git", "commit", "-q", "-m", "Initial"], cwd=path, check=True)
    return path


def create_source_archive(path: Path, version=PUBLIC_VERSION):
    source = path / f"xfce4-meowmenu-plugin-{version}"
    source.mkdir()
    write_news(source / "NEWS", version=version)
    archive = path / f"xfce4-meowmenu-plugin-{version}.tar.gz"
    with tarfile.open(archive, "w:gz") as output:
        output.add(source, arcname=source.name)
    return archive


def create_payloads(path: Path):
    for name in PAYLOAD_NAMES[:-1]:
        (path / name).write_bytes(name.encode("utf-8"))
    create_source_archive(path)
    return tuple(path / name for name in PAYLOAD_NAMES)


def write_checksums(path: Path, payloads):
    lines = []
    for payload in sorted(payloads, key=lambda item: item.name):
        digest = hashlib.sha256(payload.read_bytes()).hexdigest()
        lines.append(f"{digest}  {payload.name}\n")
    manifest = path / "SHA256SUMS"
    manifest.write_text("".join(lines), encoding="utf-8")
    return manifest
