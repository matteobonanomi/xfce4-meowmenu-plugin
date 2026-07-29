#!/usr/bin/env python3

import importlib.util
import re
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build-aux" / "documentation-links.py"
SPEC = importlib.util.spec_from_file_location("documentation_links", HELPER)
LINKS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LINKS)


class DocumentationLinksTest(unittest.TestCase):
    def test_repository_links_and_commands(self):
        self.assertEqual(LINKS.violations(ROOT), [])

    def test_extensionless_jekyll_route(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            document = docs / "index.md"
            target = docs / "support.md"
            document.write_text("[Support](support)\n", encoding="utf-8")
            target.write_text("# Support\n", encoding="utf-8")
            self.assertTrue(LINKS.resolve_link(root, document, "support"))

    def test_missing_route_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = root / "README.md"
            document.write_text("[Missing](docs/nope)\n", encoding="utf-8")
            self.assertFalse(LINKS.resolve_link(root, document, "docs/nope"))

    def test_gitless_source_tree_ignores_build_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            (docs / "support.md").write_text("# Support\n", encoding="utf-8")
            generated = root / "build"
            generated.mkdir()
            (generated / "generated.md").write_text(
                "[Missing](nowhere)\n",
                encoding="utf-8",
            )
            self.assertEqual(
                [relative for relative, _path in LINKS.markdown_files(root)],
                ["docs/support.md"],
            )

    def test_public_navigation_has_unique_order_and_required_pages(self):
        navigation = {}
        for document in (ROOT / "docs").glob("*.md"):
            content = document.read_text(encoding="utf-8")
            if not content.startswith("---\n"):
                continue
            front_matter = content.split("---\n", 2)[1]
            match = re.search(r"^nav_order:\s*(\d+)\s*$", front_matter, re.MULTILINE)
            self.assertIsNotNone(match, document)
            order = int(match.group(1))
            self.assertNotIn(order, navigation, f"duplicate nav_order {order}")
            navigation[order] = document.name
        self.assertTrue(
            {
                "support.md",
                "known-limitations.md",
                "testing.md",
                "translations.md",
            }.issubset(set(navigation.values()))
        )

    def test_evergreen_identity_and_entry_points(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        home = (ROOT / "docs/index.md").read_text(encoding="utf-8")
        for label, target in (
            ("Support and compatibility", "docs/support.md"),
            ("Known limitations", "docs/known-limitations.md"),
            ("Testing", "docs/testing.md"),
            ("Translation status", "docs/translations.md"),
        ):
            self.assertIn(f"[{label}]({target})", readme)
        self.assertIn("## Support and compatibility", readme)
        self.assertIn("supports Xfce 4.16 through 4.21", readme)
        self.assertIn("Xfce 4.20 as the primary", readme)
        self.assertIn("MeowMenu is a native Xfce panel launcher", readme)
        self.assertNotIn("## Release-candidate support", readme)
        self.assertIn("[Check support and compatibility](support)", home)
        self.assertIn("[Run the testing checklist](testing)", home)
        self.assertIn(
            "Distribution testing, package availability, and Xfce compatibility",
            home,
        )
        self.assertNotRegex(home, r"(?i)current release candidate|check RC1")

    def test_public_support_pages_avoid_obsolete_framing(self):
        pages = (
            "installation.md",
            "support.md",
            "known-limitations.md",
            "testing.md",
            "keyboard-navigation.md",
        )
        candidate_scope = re.compile(
            r"(?i)release[- ]candidate|current candidate|for RC1|"
            r"upgrade to RC1|check RC1"
        )
        current_whisker_identity = re.compile(
            r"(?im)^#\s+Whisker Menu\b|\bWhisker Menu is\b|"
            r"\bcurrent (?:product|launcher) is Whisker Menu\b"
        )
        coexistence_contract = re.compile(
            r"(?i)whisker-overlap|coexistence (?:check|test|gate|guarantee)|"
            r"(?:verified|certified) coexistence"
        )
        for name in pages:
            content = (ROOT / "docs" / name).read_text(encoding="utf-8")
            self.assertIsNone(candidate_scope.search(content), name)
            self.assertIsNone(current_whisker_identity.search(content), name)
            self.assertIsNone(coexistence_contract.search(content), name)

    def test_distro_matrix_records_exact_provenance(self):
        support = (ROOT / "docs/support.md").read_text(encoding="utf-8")
        matrix = support.split("## Distro testing", maxsplit=1)[1]
        matrix = matrix.split("## Package availability", maxsplit=1)[0]
        rows = {}
        for line in matrix.splitlines():
            cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
            if len(cells) != 4 or cells[0] in {
                "Distribution/context",
                "----------------------",
            }:
                continue
            rows[cells[0]] = tuple(cells[1:])
        self.assertEqual(
            rows,
            {
                "Debian 13": ("✓", "✓", "✓"),
                "Xubuntu 26.04": ("✓", "✓", "✓"),
                "Arch Linux": ("✓", "—", "✓"),
                "MX Linux": ("—", "✓", "—"),
                "Fedora 44": ("—", "—", "✓"),
            },
        )
        for definition in (
            "**Maintainer** means a manual result",
            "**Community** means a manual result",
            "**CI** means automated",
            "no result is currently documented",
            "not a known\n  failure or incompatibility",
        ):
            self.assertIn(definition, matrix)
        self.assertIn(
            "CI marks describe automation for the named row",
            matrix,
        )
        self.assertIn(
            "never transferred to the Maintainer or Community columns",
            matrix,
        )
        self.assertNotRegex(
            matrix,
            r"(?i)\.deb|\.rpm|AUR|Xfce 4\.(?:16|18|20|21)",
        )
        for obsolete in (
            "Source-build compatible",
            "Staged-install compatible",
            "Published package target",
            "Maintainer-published source recipe",
            "Live validated",
        ):
            self.assertNotIn(obsolete, support)
        package = support.split("## Package availability", maxsplit=1)[1]
        self.assertIn("[installation guide](installation)", package)
        self.assertIn(
            "do not create maintainer or community testing marks",
            " ".join(package.split()),
        )

    def test_xfce_compatibility_has_separate_evidence_boundaries(self):
        support = (ROOT / "docs/support.md").read_text(encoding="utf-8")
        section = support.split("## Xfce compatibility", maxsplit=1)[1]
        section = section.split("## Sessions and architectures", maxsplit=1)[0]
        self.assertIn("supports Xfce 4.16 through 4.21", section)
        self.assertIn("Xfce 4.20 as the primary\nquality target", section)
        for row in (
            "| Xfce 4.16 libraries | Source configure, build, and tests with Exo | Supported source stack |",
            "| Xfce 4.18 libraries | Source configure, build, and tests with Exo | Supported source stack |",
            "| Xfce 4.20 libraries | Source configure, build, and tests with Exo | Primary quality target |",
            "| libxfce4ui 4.21 or newer | Successor source cell and staged install without Exo | Dependency-transition boundary only |",
        ):
            self.assertIn(row, section)
        self.assertIn(
            "explicit on-demand source-stack evidence, not routine distro or "
            "live desktop results",
            " ".join(section.split()),
        )
        self.assertIn(
            "not a separately live-validated Xfce 4.21 desktop",
            " ".join(section.split()),
        )
        self.assertIn(
            "does not claim compatibility with every future",
            " ".join(section.split()),
        )

        boundaries = support.split(
            "## Sessions and architectures", maxsplit=1
        )[1]
        self.assertIn("X11 on `x86_64`/`amd64` is the primary", boundaries)
        self.assertIn("Wayland is supported\nwith a graceful", boundaries)
        self.assertIn(
            "Source compilation does not establish a session,\n"
            "architecture, or live desktop result",
            boundaries,
        )

    def test_testing_guide_is_reusable_and_scoped(self):
        testing = (ROOT / "docs/testing.md").read_text(encoding="utf-8")
        context = testing.split("## Test context", maxsplit=1)[1]
        context = context.split("## Five-minute core check", maxsplit=1)[0]
        for field in (
            "**MeowMenu version/revision:**",
            "**Distribution/version:**",
            "**Xfce version:**",
            "**Architecture:**",
            "**Session type:**",
            "**Installation method/artifact:**",
        ):
            self.assertIn(field, context)
        self.assertIn("Every result applies only to this recorded", context)

        core = testing.split("## Five-minute core check", maxsplit=1)[1]
        core = core.split("## Automated dependency evidence", maxsplit=1)[0]
        for check in (
            "version under test",
            "version shown in **About**",
            "**Add New Items**",
            "fresh profile",
            "open/search/launch cycle",
            "Log out and in",
            "another startup",
        ):
            self.assertIn(check, core)
        self.assertIn("scoped to the six recorded context fields", core)

        upgrade = testing.split("## Upgrade check", maxsplit=1)[1]
        upgrade = upgrade.split("## Removal and full cleanup", maxsplit=1)[0]
        self.assertIn("`<source-version>`", upgrade)
        self.assertIn("`<target-version>`", upgrade)
        for retained in (
            "panel item",
            "favourites and order",
            "layout/preferences",
            "Calculator choices",
            "Xfconf output",
            "preset files",
            "Log out and in",
            "another startup",
        ):
            self.assertIn(retained, upgrade)
        self.assertNotRegex(upgrade, r"\b0\.8\.0\b|\bRC1\b")

    def test_release_specific_dependency_classes_are_documented(self):
        installation = (
            ROOT / "docs/installation.md"
        ).read_text(encoding="utf-8")
        required = {
            "Ubuntu 26.04": "libexo-2-dev",
            "Debian 13": "libexo-2-dev",
            "Fedora 44": "exo-devel",
            "Arch / Manjaro / EndeavourOS": "exo",
        }
        for heading, dependency in required.items():
            section = installation.split(f"### {heading}", maxsplit=1)[1]
            section = section.split("\n### ", maxsplit=1)[0]
            self.assertIn(dependency, section)
        self.assertIn("Optional integrations on Ubuntu 26.04", installation)
        self.assertIn("Optional integrations on Debian 13", installation)
        self.assertIn("Optional integrations on Fedora 44", installation)
        self.assertIn("Optional integrations on Arch", installation)
        self.assertIn("libxfce4ui 4.21", installation)

    def test_release_artifact_and_arch_boundaries_are_documented(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        installation = (
            ROOT / "docs/installation.md"
        ).read_text(encoding="utf-8")
        for document in (readme, installation):
            normalized = " ".join(document.split())
            self.assertIn("Ubuntu 26.04", normalized)
            self.assertIn("Debian 13", normalized)
            self.assertIn("Fedora 44", normalized)
            self.assertIn("source archive", normalized)
            self.assertIn("SHA256SUMS", normalized)
        self.assertIn("sha256sum -c SHA256SUMS", installation)
        self.assertIn("four payloads", installation)
        self.assertIn("no Arch binary is attached", installation)
        self.assertIn("published manually by the maintainer", installation)

    def test_ci_package_and_live_evidence_remain_distinct(self):
        testing = (ROOT / "docs/testing.md").read_text(encoding="utf-8")
        support = (ROOT / "docs/support.md").read_text(encoding="utf-8")
        limitations = (
            ROOT / "docs/known-limitations.md"
        ).read_text(encoding="utf-8")
        for context in (
            "Ubuntu 26.04",
            "Debian 13",
            "Fedora 44",
            "sanitizers",
            "catalogs",
            "Calculator",
        ):
            self.assertIn(context, testing)
        self.assertIn("first ten consecutive", testing)
        self.assertIn("at least nine should\nfinish within 15 minutes", testing)
        self.assertIn("explicit compatibility matrix is dispatched", testing)
        self.assertIn("not live desktop results", testing)
        self.assertIn("explicit on-demand source-stack evidence", support)
        self.assertIn("do not create\nmaintainer or community testing marks", support)
        self.assertIn("explicit compatibility run", limitations)
        self.assertNotIn("continuously checked", limitations)

    def test_release_guide_matches_automatic_recovery_contract(self):
        releasing = (ROOT / "RELEASING.md").read_text(encoding="utf-8")
        normalized = " ".join(releasing.split())
        for context in (
            "build (ubuntu-26.04)",
            "build (debian-13)",
            "build (fedora-44)",
            "sanitizers",
            "static-checks",
            "no-optional-deps",
        ):
            self.assertIn(context, releasing)
        self.assertIn("first ten consecutive", normalized)
        self.assertIn("At least nine must complete within 15 minutes", normalized)
        self.assertIn("annotated or lightweight", normalized)
        self.assertIn("other than `main`", normalized)
        self.assertIn("exactly:", normalized)
        self.assertIn("published AUR metadata", normalized)
        self.assertIn("Commit and publish them manually", normalized)

    def test_readme_calls_source_stack_checks_on_demand(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("On-demand source-stack builds", readme)
        self.assertNotIn("Continuous source builds", readme)


if __name__ == "__main__":
    unittest.main()
