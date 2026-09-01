#!/usr/bin/env python3

import importlib.util
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from release_fixtures import (
    PUBLIC_VERSION,
    ReleaseFixture,
    TAG,
    candidate_identity,
    create_git_repository,
    create_payloads,
    create_tag,
    remote_release,
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
ARTIFACTS_SPEC = importlib.util.spec_from_file_location(
    "release_artifacts",
    RELEASE_HELPERS / "artifacts.py",
)
ARTIFACTS = importlib.util.module_from_spec(ARTIFACTS_SPEC)
ARTIFACTS_SPEC.loader.exec_module(ARTIFACTS)


class ReleaseContractTest(unittest.TestCase):
    def test_current_news_describes_current_composition(self):
        news = (ROOT / "NEWS").read_text(encoding="utf-8")
        current = news.split("\n\n", maxsplit=1)[0]
        for required in (
            "simplify Docked, Centered, and Full Screen composition",
            "use Modern by default",
        ):
            self.assertIn(required, current)
        self.assertNotRegex(current, r"(?i)\breset\b|retired layout|legacy key")

    def test_release_workflow_has_one_shared_tag_entry_path(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        interface = workflow.split("permissions:", maxsplit=1)[0]
        self.assertIn("- 'v*'", interface)
        self.assertIn("branches:\n      - development", interface)
        self.assertIn("workflow_dispatch:", interface)
        self.assertEqual(
            re.findall(r"(?m)^      ([a-z_]+):$", interface),
            ["mode", "tag"],
        )
        self.assertIn("default: artifact-only", interface)
        self.assertIn("- recover-release", interface)
        self.assertIn("required: false", interface)
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
            "- name: Validate candidate identity",
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
        self.assertIn("ref: ${{ env.CANDIDATE_REF }}", workflow)

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

    def test_development_source_archive_accepts_an_immutable_commit(self):
        with tempfile.TemporaryDirectory() as temporary:
            repository_path = Path(temporary) / "repository"
            repository_path.mkdir()
            repository = create_git_repository(
                repository_path,
                ReleaseFixture(),
            )
            output = Path(temporary) / f"xfce4-meowmenu-plugin-{PUBLIC_VERSION}.tar.gz"
            ARTIFACTS.create_source_archive(
                repository,
                PUBLIC_VERSION,
                output,
                ref="HEAD",
            )
            VALIDATE.validate_archive(output, PUBLIC_VERSION)

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
            {"prerelease": False},
        )
        self.assertEqual(
            VALIDATE.release_presentation("2.1.0"),
            {"prerelease": False},
        )
        VALIDATE.validate_release_state(
            {"draft": False, "prerelease": False, "latest": False},
            "2.1.0-rc7",
        )
        VALIDATE.validate_release_state(
            {"draft": False, "prerelease": False, "latest": True},
            "2.1.0",
        )

    def test_remote_lookup_distinguishes_absence_from_unavailability(self):
        self.assertEqual(
            VALIDATE.classify_lookup(200, 0),
            VALIDATE.LookupState.FOUND,
        )
        self.assertEqual(
            VALIDATE.classify_lookup(404, 1),
            VALIDATE.LookupState.ABSENT,
        )
        for status, command_exit in ((0, 1), (401, 1), (403, 1), (429, 1), (500, 1)):
            with self.subTest(status=status), self.assertRaisesRegex(
                VALIDATE.ReleaseValidationError,
                "lookup unavailable",
            ):
                VALIDATE.classify_lookup(status, command_exit)

    def test_included_api_response_preserves_status_and_json(self):
        with tempfile.TemporaryDirectory() as temporary:
            response = Path(temporary) / "response"
            response.write_bytes(
                b"HTTP/2 200 OK\r\ncontent-type: application/json\r\n\r\n"
                b'{"tag_name":"v1.0.0"}\n'
            )
            parsed = VALIDATE.parse_api_response(response, 0)
            self.assertEqual(parsed["lookup"], "found")
            self.assertEqual(parsed["release"]["tag_name"], "v1.0.0")
            response.write_bytes(b"HTTP/2 404 Not Found\r\n\r\n{}\n")
            self.assertEqual(
                VALIDATE.parse_api_response(response, 1),
                {"lookup": "absent"},
            )

    def test_asset_comparison_distinguishes_all_retry_states(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            local_path = root / "local"
            remote_path = root / "remote"
            local_path.mkdir()
            remote_path.mkdir()
            (local_path / "one.pkg").write_bytes(b"one")
            (local_path / "two.pkg").write_bytes(b"two")
            local = ARTIFACTS.local_inventory(local_path)

            comparison = ARTIFACTS.compare_inventories(
                local, ARTIFACTS.local_inventory(remote_path)
            )
            self.assertEqual(comparison.state, VALIDATE.InventoryState.MISSING)

            shutil.copy2(local_path / "one.pkg", remote_path / "one.pkg")
            comparison = ARTIFACTS.compare_inventories(
                local, ARTIFACTS.local_inventory(remote_path)
            )
            self.assertEqual(comparison.state, VALIDATE.InventoryState.INCOMPLETE)
            self.assertEqual(comparison.missing, ("two.pkg",))

            shutil.copy2(local_path / "two.pkg", remote_path / "two.pkg")
            comparison = ARTIFACTS.compare_inventories(
                local, ARTIFACTS.local_inventory(remote_path)
            )
            self.assertEqual(comparison.state, VALIDATE.InventoryState.IDENTICAL)

            (remote_path / "one.pkg").write_bytes(b"eno")
            comparison = ARTIFACTS.compare_inventories(
                local, ARTIFACTS.local_inventory(remote_path)
            )
            self.assertEqual(comparison.state, VALIDATE.InventoryState.CONFLICT)
            self.assertEqual(comparison.conflicts, ("one.pkg",))

            (remote_path / "extra.pkg").write_bytes(b"extra")
            comparison = ARTIFACTS.compare_inventories(
                local, ARTIFACTS.local_inventory(remote_path)
            )
            self.assertEqual(comparison.state, VALIDATE.InventoryState.EXTRA)

    def test_remote_metadata_requires_checksum_verification(self):
        with tempfile.TemporaryDirectory() as temporary:
            local_path = Path(temporary)
            (local_path / "one.pkg").write_bytes(b"one")
            local = ARTIFACTS.local_inventory(local_path)
            remote = ARTIFACTS.remote_inventory(
                {
                    "assets": [{
                        "name": "one.pkg",
                        "size": 3,
                        "url": "https://api.github.invalid/assets/1",
                    }]
                }
            )
            comparison = ARTIFACTS.compare_inventory_metadata(local, remote)
            self.assertEqual(comparison.state, VALIDATE.InventoryState.INCOMPLETE)

    def test_release_decision_matrix_is_fail_closed(self):
        candidate = VALIDATE.CandidateIdentity.from_mapping(candidate_identity())
        cases = (
            (
                VALIDATE.LookupState.ABSENT,
                None,
                VALIDATE.InventoryState.MISSING,
                VALIDATE.PublicationAction.CREATE_DRAFT,
            ),
            (
                VALIDATE.LookupState.FOUND,
                VALIDATE.RemoteRelease.from_mapping(remote_release(draft=True)),
                VALIDATE.InventoryState.INCOMPLETE,
                VALIDATE.PublicationAction.RESUME_DRAFT,
            ),
            (
                VALIDATE.LookupState.FOUND,
                VALIDATE.RemoteRelease.from_mapping(remote_release(draft=True)),
                VALIDATE.InventoryState.IDENTICAL,
                VALIDATE.PublicationAction.PUBLISH_DRAFT,
            ),
            (
                VALIDATE.LookupState.FOUND,
                VALIDATE.RemoteRelease.from_mapping(remote_release(draft=False)),
                VALIDATE.InventoryState.IDENTICAL,
                VALIDATE.PublicationAction.SKIP_PUBLIC,
            ),
            (
                VALIDATE.LookupState.FOUND,
                VALIDATE.RemoteRelease.from_mapping(remote_release(draft=False)),
                VALIDATE.InventoryState.INCOMPLETE,
                VALIDATE.PublicationAction.FAIL,
            ),
            (
                VALIDATE.LookupState.FOUND,
                VALIDATE.RemoteRelease.from_mapping(remote_release(draft=True)),
                VALIDATE.InventoryState.CONFLICT,
                VALIDATE.PublicationAction.FAIL,
            ),
        )
        for lookup, remote, inventory, expected in cases:
            with self.subTest(expected=expected.value):
                decision = VALIDATE.decide_publication(
                    candidate,
                    lookup,
                    inventory,
                    ("missing.pkg",),
                    remote,
                )
                self.assertEqual(decision.action, expected.value)

        conflict = remote_release(draft=True)
        conflict["name"] = "Other title"
        decision = VALIDATE.decide_publication(
            candidate,
            VALIDATE.LookupState.FOUND,
            VALIDATE.InventoryState.IDENTICAL,
            remote=VALIDATE.RemoteRelease.from_mapping(conflict),
        )
        self.assertEqual(decision.action, VALIDATE.PublicationAction.FAIL.value)
        self.assertIn("title", decision.reason)

    def test_release_publication_stages_verifies_and_then_publishes(self):
        workflow_path = ROOT / ".github/workflows/packaging.yml"
        workflow = workflow_path.read_text(encoding="utf-8")
        release_job = workflow.split("  publish-release:", maxsplit=1)[1]
        self.assertIn("GH_REPO: ${{ github.repository }}", release_job)
        self.assertIn("- name: Checkout release tools", release_job)
        self.assertIn("ref: ${{ github.workflow_sha }}", release_job)
        self.assertIn("path: release-tools", release_job)
        self.assertIn("Validate candidate and local inventory", release_job)
        self.assertIn("Compare remote release before mutation", release_job)
        self.assertIn("Stage only missing assets", release_job)
        self.assertIn("Verify complete staged inventory", release_job)
        self.assertIn("Publish verified draft", release_job)
        self.assertIn("Verify public release", release_job)
        self.assertIn("Summarize release run", release_job)
        self.assertIn('gh release create "$RELEASE_TAG" --draft', release_job)
        self.assertIn('gh release upload "$RELEASE_TAG"', release_job)
        self.assertIn('gh release edit "$RELEASE_TAG" --draft=false', release_job)
        self.assertIn("artifacts.py verify", release_job)
        self.assertIn("validate_release.py decide", release_job)
        self.assertIn("if: always()", release_job)
        self.assertEqual(workflow.count("gh release create"), 1)
        self.assertNotIn("--clobber", workflow)
        self.assertNotIn("gh release delete", workflow)

        owners = []
        for candidate in (ROOT / ".github/workflows").glob("*.yml"):
            if "gh release create" in candidate.read_text(encoding="utf-8"):
                owners.append(candidate.name)
        self.assertEqual(owners, ["packaging.yml"])

    def test_release_lookup_includes_unpublished_drafts(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        publication = workflow.split("  publish-release:", maxsplit=1)[1]
        self.assertEqual(
            publication.count(
                '"repos/${GH_REPO}/releases?per_page=100"'
            ),
            2,
        )
        self.assertEqual(
            publication.count("map(select(.tag_name == $tag))"),
            2,
        )
        self.assertNotIn(
            'releases/tags/${RELEASE_TAG}',
            publication,
        )
        self.assertIn(
            "multiple releases found for the selected tag",
            publication,
        )
        self.assertIn(
            "exactly one staged release is required",
            publication,
        )

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
            assembly.index("Upload complete package set"),
        )
        publication = workflow.split("  publish-release:", maxsplit=1)[1]
        self.assertIn("needs: [validate-tag, assemble-release]", publication)
        self.assertIn("if: needs.validate-tag.outputs.publish == 'true'", publication)

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
            "rpm -qf --qf '%{NAME}\\n' /usr/bin/exo-open",
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
        self.assertIn(
            "MEOWMENU_PACKAGE_VERSION: ${{ needs.validate-tag.outputs.version }}",
            arch,
        )
        self.assertNotIn("GITHUB_EVENT_NAME:", arch)
        self.assertNotIn("GITHUB_REF_NAME:", arch)

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
        self.assertIn("Upload complete package set", assembly)
        self.assertIn("artifacts/*", assembly)

    def test_development_packaging_is_artifact_only_and_non_publishing(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        validation = workflow.split("  validate-tag:", maxsplit=1)[1].split(
            "\n  source:",
            maxsplit=1,
        )[0]
        assembly = workflow.split("  assemble-release:", maxsplit=1)[1].split(
            "\n  publish-release:",
            maxsplit=1,
        )[0]
        publication = workflow.split("  publish-release:", maxsplit=1)[1]
        self.assertIn("permissions:\n  contents: read", workflow)
        self.assertIn(
            "test \"$GITHUB_REF\" = refs/heads/development",
            validation,
        )
        self.assertIn("test -z \"$RECOVERY_TAG\"", validation)
        self.assertIn("publish=false", validation)
        self.assertIn("--ref \"$CANDIDATE_REF\"", workflow)
        self.assertIn(
            "github.event_name == 'push' "
            "&& github.ref == 'refs/heads/development'",
            workflow,
        )
        self.assertIn("Upload complete package set", assembly)
        self.assertNotIn("gh release", assembly)
        self.assertIn("permissions:\n      contents: write", publication)
        self.assertIn(
            "if: needs.validate-tag.outputs.publish == 'true'",
            publication,
        )

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
        self.assertIn(
            "if: github.event_name == 'workflow_dispatch' "
            "&& inputs.mode == 'recover-release'",
            authority,
        )
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
            workflow.index("Compare remote release before mutation"),
        )

    def test_recovery_never_deletes_or_replaces_remote_assets(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        publication = workflow.split("  publish-release:", maxsplit=1)[1]
        self.assertIn("remote-release-stream.json", publication)
        self.assertIn("peeledCommit", publication)
        self.assertIn("preflight", publication)
        self.assertIn("missing_assets", publication)
        self.assertIn("skip-public", publication)
        self.assertNotIn("release delete", publication)
        self.assertNotIn("delete-asset", publication)
        self.assertNotIn("--clobber", publication)
        releasing = (ROOT / "RELEASING.md").read_text(encoding="utf-8")
        self.assertIn("A matching private draft", releasing)
        self.assertIn("A conflicting draft fails closed", releasing)
        self.assertNotIn("delete a stale draft", releasing)
        self.assertNotIn("may be deleted", releasing)

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
