#!/usr/bin/env python3

import importlib.util
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from release_fixtures import (
    PUBLIC_VERSION,
    ReleaseFixture,
    TAG,
    create_git_repository,
    create_payloads,
    create_tag,
    write_checksums,
)


ROOT = Path(__file__).resolve().parents[1]
RELEASE_HELPERS = ROOT / "build-aux" / "release"
sys.path.insert(0, str(RELEASE_HELPERS))
HELPER = RELEASE_HELPERS / "validate_release.py"
SPEC = importlib.util.spec_from_file_location("validate_release", HELPER)
VALIDATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATE)
NOTES_SPEC = importlib.util.spec_from_file_location(
    "release_notes",
    RELEASE_HELPERS / "release_notes.py",
)
NOTES = importlib.util.module_from_spec(NOTES_SPEC)
NOTES_SPEC.loader.exec_module(NOTES)


class ReleaseContractTest(unittest.TestCase):
    def test_release_workflow_has_one_shared_tag_entry_path(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        interface = workflow.split("permissions:", maxsplit=1)[0]
        self.assertIn("- 'v*'", interface)
        self.assertIn("workflow_dispatch:", interface)
        self.assertEqual(
            re.findall(r"(?m)^      ([a-z_]+):$", interface),
            ["tag"],
        )
        for retired in ("publish:", "authorization:", "manual_evidence:"):
            self.assertNotIn(retired, interface)
        self.assertIn(
            "group: packaging-${{ github.event_name == 'workflow_dispatch' "
            "&& inputs.tag || github.ref_name }}",
            interface,
        )
        self.assertIn("cancel-in-progress: false", interface)

    def test_release_workflow_separates_tools_from_candidate_source(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        identity = workflow.split(
            "- name: Validate tag, main ancestry, and NEWS identity",
            maxsplit=1,
        )[1].split("\n  source:", maxsplit=1)[0]
        self.assertGreaterEqual(
            workflow.count("ref: ${{ github.workflow_sha }}"),
            4,
        )
        self.assertIn("path: release-tools", workflow)
        self.assertIn("path: candidate", workflow)
        self.assertIn(
            'git -C candidate rev-parse "${RELEASE_TAG}^{commit}"',
            identity,
        )
        self.assertIn(
            'git -C candidate merge-base --is-ancestor "$commit" origin/main',
            identity,
        )
        self.assertIn("--news candidate/NEWS", identity)
        self.assertNotIn("cat-file -t", identity)
        self.assertIn("--repository candidate", workflow)

    def test_annotated_and_lightweight_tags_resolve_identically(self):
        for kind in ("annotated", "lightweight"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temporary:
                release = ReleaseFixture(tag_kind=kind)
                repository = create_git_repository(Path(temporary), release)
                create_tag(repository, release)
                commit = VALIDATE.validate_tag(
                    repository,
                    release.tag,
                    release.tag,
                    "main",
                )
                head = subprocess.run(
                    ["git", "rev-parse", "HEAD"],
                    cwd=repository,
                    check=True,
                    text=True,
                    stdout=subprocess.PIPE,
                ).stdout.strip()
                self.assertEqual(commit, head)

    def test_exact_asset_inventory_archive_and_checksums(self):
        for version in ("2.1.0-rc7", "2.1.0"):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temporary:
                release = ReleaseFixture(version=version)
                assets = Path(temporary)
                payloads = create_payloads(assets, release)
                manifest = write_checksums(assets, payloads)
                VALIDATE.validate_assets(assets, version)
                VALIDATE.validate_archive(payloads[-1], version)
                entries = manifest.read_text(encoding="utf-8").splitlines()
                self.assertEqual(len(entries), 4)
                self.assertNotIn("SHA256SUMS", manifest.read_text(encoding="utf-8"))

    def test_checksum_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            assets = Path(temporary)
            payloads = create_payloads(assets)
            write_checksums(assets, payloads)
            payloads[0].write_text("changed", encoding="utf-8")
            with self.assertRaisesRegex(VALIDATE.ReleaseValidationError, "Checksum"):
                VALIDATE.validate_assets(assets, PUBLIC_VERSION)

    def test_missing_and_extra_assets_are_rejected(self):
        for mutation in ("missing", "extra"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                assets = Path(temporary)
                payloads = create_payloads(assets)
                write_checksums(assets, payloads)
                if mutation == "missing":
                    payloads[0].unlink()
                else:
                    (assets / "unexpected.pkg").write_bytes(b"extra")
                with self.assertRaisesRegex(
                    VALIDATE.ReleaseValidationError,
                    "Asset mismatch",
                ):
                    VALIDATE.validate_assets(assets, PUBLIC_VERSION)

    def test_duplicate_and_self_referential_checksums_are_rejected(self):
        for mutation in ("duplicate", "self"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                assets = Path(temporary)
                payloads = create_payloads(assets)
                manifest = write_checksums(assets, payloads)
                lines = manifest.read_text(encoding="utf-8").splitlines()
                if mutation == "duplicate":
                    lines[-1] = lines[0]
                else:
                    digest = "0" * 64
                    lines[-1] = f"{digest}  SHA256SUMS"
                manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
                with self.assertRaisesRegex(
                    VALIDATE.ReleaseValidationError,
                    "manifest|Checksum",
                ):
                    VALIDATE.validate_assets(assets, PUBLIC_VERSION)

    def test_release_notes_preserve_multiline_and_shell_characters(self):
        body = "- Keep $HOME and `commands` literal.\n\n- Preserve \"quotes\" && pipes |."
        for version in ("2.1.0-rc7", "2.1.0"):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temporary:
                release = ReleaseFixture(version=version, news_body=body)
                repository = create_git_repository(Path(temporary), release)
                rendered = NOTES.render(repository / "NEWS", version)
                self.assertEqual(rendered, body)
                notes = repository / "notes.md"
                notes.write_text(rendered, encoding="utf-8")
                VALIDATE.validate_release_notes(notes, body)

    def test_release_presentation_is_derived_from_version(self):
        self.assertEqual(
            VALIDATE.release_presentation("2.1.0-rc7"),
            {"prerelease": True, "latest": False},
        )
        self.assertEqual(
            VALIDATE.release_presentation("2.1.0"),
            {"prerelease": False, "latest": True},
        )
        VALIDATE.validate_release_state(
            {"draft": False, "prerelease": True, "latest": False},
            "2.1.0-rc7",
        )
        VALIDATE.validate_release_state(
            {"draft": False, "prerelease": False, "latest": True},
            "2.1.0",
        )

    def test_release_publication_is_one_exact_inventory_transaction(self):
        workflow_path = ROOT / ".github/workflows/packaging.yml"
        workflow = workflow_path.read_text(encoding="utf-8")
        publication = workflow.split(
            "- name: Publish one complete release",
            maxsplit=1,
        )[1].split(
            "- name: Download and verify the published assets",
            maxsplit=1,
        )[0]
        self.assertIn('gh release create "$RELEASE_TAG" artifacts/*', publication)
        self.assertIn("--verify-tag", publication)
        self.assertIn("--notes-file release-notes.md", publication)
        self.assertIn("--prerelease --latest=false", publication)
        self.assertEqual(workflow.count("gh release create"), 1)
        self.assertNotIn("gh release upload", workflow)
        self.assertNotIn("--clobber", workflow)
        self.assertNotIn("gh release edit", workflow)
        self.assertNotIn("--draft", publication)

        owners = []
        for candidate in (ROOT / ".github/workflows").glob("*.yml"):
            if "gh release create" in candidate.read_text(encoding="utf-8"):
                owners.append(candidate.name)
        self.assertEqual(owners, ["packaging.yml"])

    def test_publication_depends_on_every_mandatory_package_gate(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        assembly = workflow.split("  assemble-release:", maxsplit=1)[1]
        needs = assembly.split("needs:", maxsplit=1)[1].splitlines()[0]
        for job in (
            "validate-tag",
            "source",
            "build-deb",
            "build-rpm",
            "arch-pkgbuild-test",
        ):
            self.assertIn(job, needs)
        self.assertLess(
            assembly.index("Run release, presentation, documentation, and translation gates"),
            assembly.index("Publish one complete release"),
        )

    def test_native_package_jobs_keep_complete_candidate_gates(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        deb = workflow.split("  build-deb:", maxsplit=1)[1].split(
            "\n  build-rpm:",
            maxsplit=1,
        )[0]
        for required in (
            "Download canonical source",
            "dpkg-buildpackage -us -uc -b",
            "dpkg-deb -f",
            "appstreamcli validate --no-net",
            'lintian "$package"',
            'apt-get install -y "$package"',
            "dpkg-query -W -f=",
            "dpkg-query -S /usr/bin/exo-open",
            "assert-dependency-regime.sh",
            "installed-action-smoke.sh",
        ):
            self.assertIn(required, deb)

        rpm = workflow.split("  build-rpm:", maxsplit=1)[1].split(
            "\n  arch-pkgbuild-test:",
            maxsplit=1,
        )[0]
        for required in (
            "Download canonical source",
            "dnf -y builddep",
            "rpmbuild -ba",
            "rpm -qp --qf",
            "appstreamcli validate --no-net",
            'rpmlint "$spec" "$package"',
            'dnf -y install "$package"',
            "rpm -qf /usr/bin/exo-open",
            "assert-dependency-regime.sh",
            "installed-action-smoke.sh",
        ):
            self.assertIn(required, rpm)

        arch = workflow.split("  arch-pkgbuild-test:", maxsplit=1)[1].split(
            "\n  assemble-release:",
            maxsplit=1,
        )[0]
        for required in (
            "Checkout immutable candidate",
            "build-aux/arch/prepare-source.sh",
            "build-aux/arch/build-package.sh",
            "build-aux/arch/run-namcap.sh",
            "build-aux/arch/smoke-install.sh",
            "pacman -Q xfce4-meowmenu-plugin",
            "makepkg --printsrcinfo",
        ):
            self.assertIn(required, arch)

    def test_release_assembly_downloads_only_publishable_artifacts(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        assembly = workflow.split("  assemble-release:", maxsplit=1)[1]
        for artifact in (
            "deb-package-ubuntu",
            "deb-package-debian",
            "rpm-package",
            "canonical-source",
        ):
            self.assertIn(f"name: {artifact}", assembly)
        self.assertNotIn("name: arch-pkgbuild-logs\n          path: artifacts", assembly)
        self.assertIn("artifacts.py verify", assembly)

    def test_routine_ci_has_stable_required_contexts(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        triggers = workflow.split("permissions:", maxsplit=1)[0]
        self.assertIn("pull_request:\n    branches: [main]", triggers)
        self.assertIn("push:\n    branches: [main]", triggers)

        required = {
            "build (${{ matrix.distro }})",
            "sanitizers",
            "static-checks",
            "no-optional-deps",
        }
        names = set(re.findall(r"(?m)^    name: (.+)$", workflow))
        self.assertTrue(required.issubset(names))
        for context in (
            "build (ubuntu-26.04)",
            "build (debian-13)",
            "build (fedora-44)",
            "sanitizers",
            "static-checks",
            "no-optional-deps",
        ):
            self.assertIn(f"#   {context}", workflow)
        self.assertNotIn("name: translations", workflow)
        self.assertNotIn("name: docs-dependency-drift", workflow)

    def test_codeql_is_weekly_manual_and_non_publishing(self):
        workflow = (
            ROOT / ".github/workflows/codeql.yml"
        ).read_text(encoding="utf-8")
        triggers = workflow.split("permissions:", maxsplit=1)[0]
        self.assertIn("workflow_dispatch:", triggers)
        self.assertIn("schedule:", triggers)
        self.assertIn("cron: '0 6 * * 1'", triggers)
        self.assertNotIn("pull_request:", triggers)
        self.assertNotIn("push:", triggers)
        self.assertNotIn("gh release", workflow)
        self.assertNotIn("contents: write", workflow)

    def test_existing_tag_recovery_is_authorized_before_candidate_or_mutation(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        authority = workflow.split(
            "- name: Authorize existing-tag recovery",
            maxsplit=1,
        )[1].split(
            "- name: Checkout immutable candidate",
            maxsplit=1,
        )[0]
        self.assertIn("if: github.event_name == 'workflow_dispatch'", authority)
        self.assertIn(
            "*/.github/workflows/packaging.yml@refs/heads/main",
            authority,
        )
        self.assertIn("WORKFLOW_SHA: ${{ github.workflow_sha }}", authority)
        self.assertIn(
            '"$WORKFLOW_SHA" origin/main',
            authority,
        )
        self.assertNotIn("gh release", authority)
        self.assertLess(
            workflow.index("Authorize existing-tag recovery"),
            workflow.index("Checkout immutable candidate"),
        )
        self.assertLess(
            workflow.index("Authorize existing-tag recovery"),
            workflow.index('gh release delete "$RELEASE_TAG" --yes'),
        )

    def test_recovery_refuses_public_replacement_and_cleans_only_stale_drafts(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        publication = workflow.split(
            "- name: Publish one complete release",
            maxsplit=1,
        )[1].split(
            "- name: Download and verify the published assets",
            maxsplit=1,
        )[0]
        self.assertIn("--json isDraft", publication)
        self.assertIn('test "$(jq -r .isDraft /tmp/release.json)" = true', publication)
        self.assertIn('gh release delete "$RELEASE_TAG" --yes', publication)
        self.assertNotIn("--cleanup-tag", publication)
        self.assertIn("A public release already exists", publication)
        self.assertNotIn("delete-asset", publication)

    def test_release_surfaces_exclude_retired_paths_and_internal_wording(self):
        surfaces = (
            ROOT / ".github/workflows/packaging.yml",
            ROOT / ".github/workflows/ci.yml",
            ROOT / "RELEASING.md",
            ROOT / "README.md",
            ROOT / "docs/installation.md",
            ROOT / "docs/testing.md",
            ROOT / "docs/support.md",
            ROOT / "docs/known-limitations.md",
        )
        combined = "\n".join(
            surface.read_text(encoding="utf-8") for surface in surfaces
        )
        for retired in (
            "whisker-" + "overlap",
            "coexistence gate",
            "coexistence report",
            "manual_evidence",
            "inputs.publish",
            "authorization:",
            "gh release upload",
            "--clobber",
            "draft-first",
            "0.9.0~rc2",
            "0.9.0rc2",
            "RC1 must",
            ".spec" + "ify/",
        ):
            self.assertNotIn(retired, combined)

    def test_cross_artifact_release_invariants_are_reconstructable(self):
        packaging = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        ci = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        releasing = (ROOT / "RELEASING.md").read_text(encoding="utf-8")
        for tag_kind in ("annotated", "lightweight"):
            self.assertIn(tag_kind, packaging)
            self.assertIn(tag_kind, releasing)
        for context in (
            "build (ubuntu-26.04)",
            "build (debian-13)",
            "build (fedora-44)",
            "sanitizers",
            "static-checks",
            "no-optional-deps",
        ):
            self.assertIn(context, ci)
            self.assertIn(context, releasing)
        self.assertIn("SHA256SUMS", packaging)
        self.assertIn("artifacts/*", packaging)
        self.assertIn("bc qalc gcalccmd", ci)
        self.assertIn("refs/heads/main", packaging)

    def test_retired_coexistence_machinery_is_absent(self):
        retired_script = "check-whisker-" + "overlap.sh"
        self.assertFalse((ROOT / "build-aux/arch" / retired_script).exists())
        workflow = (ROOT / ".github/workflows/packaging.yml").read_text(encoding="utf-8")
        self.assertNotIn("whisker-" + "overlap-check", workflow)
        self.assertNotIn("whisker-" + "overlap.md", workflow)


if __name__ == "__main__":
    unittest.main()
