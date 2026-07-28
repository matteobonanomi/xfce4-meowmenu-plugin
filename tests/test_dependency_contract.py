#!/usr/bin/env python3
"""Static dependency-boundary and package/workflow contract checks."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DependencyContractTests(unittest.TestCase):
    def test_libxfce4ui_transition_boundary(self):
        meson = (ROOT / "meson.build").read_text(encoding="utf-8")
        boundaries = re.findall(
            r"libxfce4ui\.version\(\)\.version_compare\('(< 4\.21\.0)'\)",
            meson,
        )
        self.assertEqual(boundaries, ["< 4.21.0"])

    def test_debian_declares_direct_exo_contract(self):
        control = (ROOT / "debian/control").read_text(encoding="utf-8")
        source, binary = control.split("\nPackage:", maxsplit=1)
        self.assertRegex(source, r"(?m)^ libexo-2-dev,$")
        self.assertRegex(binary, r"(?m)^ exo-utils,$")

    def test_debian_jobs_resolve_the_manifest(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("mk-build-deps --install --remove", workflow)
        self.assertIn("dpkg-query -W libexo-2-dev", workflow)
        self.assertIn("debian-exo-negative", workflow)
        bootstrap = workflow.split(
            "- name: Install Debian package bootstrap tools", maxsplit=1
        )[1].split("- name:", maxsplit=1)[0]
        self.assertNotIn("libexo-2-dev", bootstrap)
        self.assertNotIn("libxfce4ui-2-dev", bootstrap)

    def test_fedora_declares_build_and_helper_contract(self):
        spec = (
            ROOT / "dist/rpm/xfce4-meowmenu-plugin.spec"
        ).read_text(encoding="utf-8")
        self.assertRegex(spec, r"(?m)^BuildRequires:\s+pkgconfig\(exo-2\)$")
        self.assertRegex(spec, r"(?m)^Requires:\s+/usr/bin/exo-open$")

    def test_arch_keeps_exo_in_runtime_depends(self):
        pkgbuild = (
            ROOT / "dist/arch/PKGBUILD"
        ).read_text(encoding="utf-8")
        depends = re.search(r"(?ms)^depends=\((.*?)\)\n", pkgbuild)
        self.assertIsNotNone(depends)
        self.assertRegex(depends.group(1), r"'exo'")
        build_helper = (
            ROOT / "build-aux/arch/build-package.sh"
        ).read_text(encoding="utf-8")
        self.assertRegex(build_helper, r"makepkg\s+\\\n\s+--syncdeps")

    def test_fedora_and_arch_workflow_mutations(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("dnf -y builddep", workflow)
        self.assertIn("fedora-exo-negative", workflow)
        self.assertIn("arch-exo-negative", workflow)
        self.assertIn("installed-action-smoke.sh", workflow)

    def test_source_stack_matrix_covers_both_regimes(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertIn("xfce-source-stack:", workflow)
        self.assertIn("cell: ['4.16', '4.18', '4.20', successor]", workflow)
        self.assertIn("build-xfce-stack.sh", workflow)
        self.assertIn("--regime \"$regime\" --plugin \"$installed_plugin\"", workflow)
        self.assertIn("--staged-root \"$staged_root\"", workflow)


if __name__ == "__main__":
    unittest.main()
