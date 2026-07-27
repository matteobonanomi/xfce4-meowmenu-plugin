#!/usr/bin/env python3

import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from release_fixtures import (
    PUBLIC_VERSION,
    TAG,
    create_git_repository,
    create_payloads,
    write_checksums,
)


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build-aux" / "release" / "validate_release.py"
SPEC = importlib.util.spec_from_file_location("validate_release", HELPER)
VALIDATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATE)


class ReleaseContractTest(unittest.TestCase):
    def test_annotated_tag_and_ancestry(self):
        with tempfile.TemporaryDirectory() as temporary:
            repository = create_git_repository(Path(temporary))
            subprocess.run(
                ["git", "tag", "-a", TAG, "-m", "Candidate"],
                cwd=repository,
                check=True,
            )
            commit = VALIDATE.validate_tag(repository, TAG, TAG, "main")
            self.assertEqual(len(commit), 40)

    def test_lightweight_tag_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            repository = create_git_repository(Path(temporary))
            subprocess.run(["git", "tag", TAG], cwd=repository, check=True)
            with self.assertRaisesRegex(VALIDATE.ReleaseValidationError, "annotated"):
                VALIDATE.validate_tag(repository, TAG, TAG, "main")

    def test_exact_asset_inventory_archive_and_checksums(self):
        with tempfile.TemporaryDirectory() as temporary:
            assets = Path(temporary)
            payloads = create_payloads(assets)
            write_checksums(assets, payloads)
            VALIDATE.validate_assets(assets, PUBLIC_VERSION)
            VALIDATE.validate_archive(payloads[-1], PUBLIC_VERSION)

    def test_checksum_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            assets = Path(temporary)
            payloads = create_payloads(assets)
            write_checksums(assets, payloads)
            payloads[0].write_text("changed", encoding="utf-8")
            with self.assertRaisesRegex(VALIDATE.ReleaseValidationError, "Checksum"):
                VALIDATE.validate_assets(assets, PUBLIC_VERSION)

    def test_release_notes_and_private_prerelease_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            notes = Path(temporary) / "notes.md"
            notes.write_text(
                "\n".join(f"## {heading}\nContent" for heading in VALIDATE.REQUIRED_NOTE_HEADINGS),
                encoding="utf-8",
            )
            VALIDATE.validate_release_notes(notes)
            VALIDATE.validate_release_state(
                {"draft": True, "prerelease": True, "latest": False}
            )
            with self.assertRaises(VALIDATE.ReleaseValidationError):
                VALIDATE.validate_release_state(
                    {"draft": False, "prerelease": True, "latest": False}
                )

    def test_retired_coexistence_machinery_is_absent(self):
        retired_script = "check-whisker-" + "overlap.sh"
        self.assertFalse((ROOT / "build-aux/arch" / retired_script).exists())
        workflow = (ROOT / ".github/workflows/packaging.yml").read_text(encoding="utf-8")
        self.assertNotIn("whisker-" + "overlap-check", workflow)
        self.assertNotIn("whisker-" + "overlap.md", workflow)


if __name__ == "__main__":
    unittest.main()
