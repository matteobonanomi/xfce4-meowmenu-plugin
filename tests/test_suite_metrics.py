#!/usr/bin/env python3
"""Report the maintained test scope and the result set of a configured tree."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path
SOURCE_SUFFIXES = {".cpp", ".h", ".py", ".sh"}
TEST_REGISTRATION = re.compile(r"^\s*test\s*\(", re.MULTILINE)
NATIVE_CASE = re.compile(r"g_test_add(?:_data)?_func\s*\(")
PYTHON_CASE = re.compile(r"^\s*def\s+test_[A-Za-z0-9_]+\s*\(", re.MULTILINE)
RESULT_BLOCK = re.compile(
    r"^test:\s+[^:]+:(?P<name>[^\s]+)\n"
    r"(?:^start time:\s+(?P<start>[^\n]+)\n)?"
    r"^duration:\s+(?P<seconds>[0-9.]+)s\n"
    r"^result:\s+exit status (?P<status>\d+)",
    re.MULTILINE | re.DOTALL,
)
def run(command: list[str], cwd: Path) -> str:
    """Run a read-only command and return its UTF-8 output."""
    return subprocess.check_output(command, cwd=cwd, text=True).strip()
def source_files(root: Path) -> list[Path]:
    """Return maintained test and build files, excluding generated trees."""
    files = []
    for path in (root / "tests").rglob("*"):
        if not path.is_file() or "__pycache__" in path.parts:
            continue
        if path.name == "meson.build" or path.suffix in SOURCE_SUFFIXES:
            files.append(path)
    return sorted(files)
def git_source_files(root: Path, revision: str) -> list[tuple[str, str]]:
    """Load maintained files from a Git revision without changing the tree."""
    names = run(
        ["git", "ls-tree", "-r", "--name-only", revision, "tests"], root
    ).splitlines()
    selected = []
    for name in names:
        path = Path(name)
        if "__pycache__" in path.parts:
            continue
        if path.name != "meson.build" and path.suffix not in SOURCE_SUFFIXES:
            continue
        selected.append((name, run(["git", "show", f"{revision}:{name}"], root)))
    return selected
def count_lines(root: Path, revision: str | None) -> int:
    """Count source lines in the maintained test/build scope."""
    if revision:
        return sum(len(content.splitlines()) for _, content in git_source_files(root, revision))
    return sum(len(path.read_text(encoding="utf-8").splitlines()) for path in source_files(root))
def current_texts(root: Path, revision: str | None) -> list[tuple[str, str]]:
    """Return the test/build texts used by source-only measurements."""
    if revision:
        return git_source_files(root, revision)
    return [
        (str(path.relative_to(root)), path.read_text(encoding="utf-8"))
        for path in source_files(root)
    ]
def source_counts(root: Path, revision: str | None) -> dict[str, int]:
    """Count textual registrations and named behavior cases."""
    texts = current_texts(root, revision)
    meson = "\n".join(content for name, content in texts if name.endswith("meson.build"))
    native = sum(len(NATIVE_CASE.findall(content)) for name, content in texts if name.endswith(".cpp"))
    python = sum(len(PYTHON_CASE.findall(content)) for name, content in texts if name.endswith(".py"))
    return {
        "textual_registrations": len(TEST_REGISTRATION.findall(meson)),
        "native_named_cases": native,
        "python_named_cases": python,
        "meaningful_scenarios": native + python,
    }
def introspect(root: Path, build_dir: Path) -> dict[str, object]:
    """Read configured test topology when Meson has produced introspection data."""
    result: dict[str, object] = {}
    if not (build_dir / "meson-private" / "coredata.dat").exists():
        return result
    try:
        tests = json.loads(run(["meson", "introspect", str(build_dir), "--tests"], root))
        targets = json.loads(run(["meson", "introspect", str(build_dir), "--targets"], root))
    except (OSError, subprocess.CalledProcessError, json.JSONDecodeError):
        return result
    result["registrations"] = len(tests)
    test_targets = [
        target
        for target in targets
        if target.get("type") == "executable"
        and (target.get("name", "").startswith("test_")
             or target.get("name") == "calculator-engine-stub")
    ]
    result["linked_executables"] = sum(
        target.get("name") != "calculator-engine-stub" for target in test_targets
    )
    result["helper_stubs"] = sum(
        target.get("name") == "calculator-engine-stub" for target in test_targets
    )
    result["registered_names"] = [test.get("name", "") for test in tests]
    return result
def parse_testlog(build_dir: Path) -> dict[str, object]:
    """Extract timing, diagnostics, skips, and named results from Meson's log."""
    log_path = build_dir / "meson-logs" / "testlog.txt"
    if not log_path.exists():
        return {"wall_samples": [], "cumulative_seconds": 0.0, "results": []}
    content = log_path.read_text(encoding="utf-8", errors="replace")
    results = []
    starts = []
    ends = []
    for match in RESULT_BLOCK.finditer(content):
        item = match.groupdict()
        status = int(item.pop("status"))
        start = item.pop("start")
        if start:
            start_seconds = datetime.strptime(
                start.strip(), "%H:%M:%S"
            ).hour * 3600 + datetime.strptime(
                start.strip(), "%H:%M:%S"
            ).minute * 60 + datetime.strptime(
                start.strip(), "%H:%M:%S"
            ).second
            starts.append(start_seconds)
            ends.append(start_seconds + float(item["seconds"]))
        item["result"] = "OK" if status == 0 else "SKIP" if status == 77 else "FAIL"
        results.append(item)
    criticals = len(re.findall(r"(?im)^.*\bCRITICAL \*\*.*$", content))
    warnings = len(re.findall(r"(?im)^.*\bWARNING \*\*.*$", content))
    skips = [item["name"] for item in results if item["result"] == "SKIP"]
    cumulative = sum(float(item["seconds"]) for item in results)
    wall = max(ends) - min(starts) if starts else cumulative
    return {
        "wall_samples": [round(wall, 3)] if results else [],
        "cumulative_seconds": round(cumulative, 3),
        "unexpected_criticals": criticals,
        "unexpected_warnings": warnings,
        "declared_skips": skips,
        "results": [f"{item['name']}={item['result']}" for item in results],
    }
def revision_state(root: Path) -> dict[str, object]:
    """Describe the source revision and whether the worktree is dirty."""
    try:
        revision = run(["git", "rev-parse", "HEAD"], root)
        dirty = bool(run(["git", "status", "--porcelain"], root))
    except (OSError, subprocess.CalledProcessError):
        revision, dirty = "unversioned", None
    return {"revision": revision, "dirty": dirty}
def build_measurement(root: Path, build_dir: Path, revision: str | None) -> dict[str, object]:
    """Build a serializable measurement snapshot."""
    result = {
        **revision_state(root),
        "reference_environment": {
            "platform": os.uname().sysname + " " + os.uname().release,
            "machine": os.uname().machine,
            "python": sys.version.split()[0],
            "meson": shutil.which("meson") or "missing",
            "build_dir": str(build_dir),
        },
        "maintained_lines": count_lines(root, revision),
        **source_counts(root, revision),
        "test_build_footprint_bytes": build_footprint(build_dir),
    }
    result.update(introspect(root, build_dir))
    result.update(parse_testlog(build_dir))
    return result
def build_footprint(build_dir: Path) -> int:
    """Return the byte footprint of the build tree, not its filesystem."""
    if not build_dir.exists():
        return 0
    total = 0
    for path in build_dir.rglob("*"):
        if path.is_file():
            total += path.stat().st_size
    return total
def self_check(root: Path) -> None:
    """Exercise the parser against a small synthetic test result set."""
    with tempfile.TemporaryDirectory() as directory:
        build_dir = Path(directory)
        log_dir = build_dir / "meson-logs"
        log_dir.mkdir()
        (log_dir / "testlog.txt").write_text(
            "test: sample:alpha\n"
            "duration: 0.10s\n"
            "result: exit status 0\n"
            "test: sample:beta\n"
            "duration: 0.20s\n"
            "result: exit status 77\n",
            encoding="utf-8",
        )
        parsed = parse_testlog(build_dir)
        assert parsed["cumulative_seconds"] == 0.3
        assert parsed["declared_skips"] == ["beta"]
        assert parse_testlog(build_dir / "missing")["wall_samples"] == []
    assert all("__pycache__" not in str(path) for path in source_files(root))
    print("suite metrics self-check: PASS")


def main() -> int:
    """Parse command-line options and print a measurement or self-check result."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path, default=None)
    parser.add_argument("--revision", default=None)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    build_dir = (args.build_dir or root / "build").resolve()
    if args.self_check:
        self_check(root)
        return 0
    measurement = build_measurement(root, build_dir, args.revision)
    if args.json:
        print(json.dumps(measurement, indent=2, sort_keys=True))
    else:
        for key, value in measurement.items():
            print(f"{key}: {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
