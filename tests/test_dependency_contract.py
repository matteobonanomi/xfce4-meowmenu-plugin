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
        self.assertRegex(source, r"(?m)^ git,$")
        self.assertRegex(binary, r"(?m)^ exo-utils,$")

    def test_debian_jobs_resolve_the_manifest(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("mk-build-deps --install --remove", workflow)
        self.assertIn("dpkg-query -W libexo-2-dev", workflow)
        self.assertNotIn("debian-exo-negative", workflow)
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
        self.assertRegex(spec, r"(?m)^BuildRequires:\s+/usr/bin/git$")
        self.assertRegex(spec, r"(?m)^Requires:\s+/usr/bin/exo-open$")

    def test_arch_keeps_exo_in_runtime_depends(self):
        pkgbuild = (
            ROOT / "dist/arch/PKGBUILD"
        ).read_text(encoding="utf-8")
        depends = re.search(r"(?ms)^depends=\((.*?)\)\n", pkgbuild)
        self.assertIsNotNone(depends)
        self.assertRegex(depends.group(1), r"'exo'")
        makedepends = re.search(r"(?ms)^makedepends=\((.*?)\)\n", pkgbuild)
        self.assertIsNotNone(makedepends)
        self.assertRegex(makedepends.group(1), r"'git'")
        build_helper = (
            ROOT / "build-aux/arch/build-package.sh"
        ).read_text(encoding="utf-8")
        self.assertRegex(build_helper, r"makepkg\s+\\\n\s+--syncdeps")
        self.assertNotIn("meson test -C", build_helper)
        self.assertIn('dirname "${BASH_SOURCE[0]}"', build_helper)
        self.assertIn(
            '"${script_dir}/../compat/assert-dependency-regime.sh"',
            build_helper,
        )
        self.assertNotIn('dirname "$0"', build_helper)
        source_helper = (
            ROOT / "build-aux/arch/prepare-source.sh"
        ).read_text(encoding="utf-8")
        self.assertIn('package_version="${MEOWMENU_PACKAGE_VERSION:-}"', source_helper)
        self.assertIn(
            '"${package_version}" != "${news_version}"',
            source_helper,
        )

    def test_fedora_and_arch_keep_positive_package_checks(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("dnf -y builddep", workflow)
        self.assertNotIn("fedora-exo-negative", workflow)
        self.assertNotIn("arch-exo-negative", workflow)
        self.assertIn("build-aux/arch/build-package.sh", workflow)
        self.assertIn("build-aux/arch/smoke-install.sh", workflow)
        self.assertIn("installed-action-smoke.sh", workflow)

    def test_source_stack_matrix_covers_both_regimes(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertIn("xfce-source-stack:", workflow)
        self.assertIn("cell: ['4.16', '4.18', '4.20', successor]", workflow)
        self.assertIn("build-xfce-stack.sh", workflow)
        self.assertIn("--regime \"$regime\" --plugin \"$installed_plugin\"", workflow)
        self.assertIn("--staged-root \"$staged_root\"", workflow)
        bootstrap = workflow.split(
            "- name: Install source-stack bootstrap dependencies", maxsplit=1
        )[1].split("- name:", maxsplit=1)[0]
        self.assertRegex(bootstrap, r"\bgit\b")
        self.assertRegex(bootstrap, r"\bgobject-introspection\b")
        self.assertRegex(bootstrap, r"\blibgtop2-dev\b")
        stack_builder = (
            ROOT / "build-aux/compat/build-xfce-stack.sh"
        ).read_text(encoding="utf-8")
        self.assertIn("--libdir=lib", stack_builder)
        self.assertIn("export GI_GIR_PATH=", stack_builder)
        self.assertIn("export GI_TYPELIB_PATH=", stack_builder)
        self.assertIn('"xfce4-dev-tools:4.20.0"', stack_builder)
        self.assertIn('"xfce4-panel:4.21.0"', stack_builder)
        self.assertEqual(
            stack_builder.count('"libxfce4windowing:4.20.4"'),
            2,
        )

    def test_routine_ci_keeps_six_proportionate_checks(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        build = workflow.split("  build:", maxsplit=1)[1].split(
            "\n  sanitizers:",
            maxsplit=1,
        )[0]
        self.assertIn("distro: [ubuntu-26.04, debian-13, fedora-44]", build)
        self.assertIn("name: build (${{ matrix.distro }})", build)
        self.assertIn("--buildtype=debugoptimized", build)
        self.assertNotIn("matrix.buildtype", build)

        optional = workflow.split("  no-optional-deps:", maxsplit=1)[1].split(
            "\n  xfce-source-stack:",
            maxsplit=1,
        )[0]
        self.assertIn("-Daccountsservice=disabled", optional)
        self.assertIn("-Dgtk-layer-shell=disabled", optional)
        for provider in ("bc", "qalc", "gcalccmd"):
            self.assertIn(provider, optional)
        self.assertIn("meson test -C build --print-errorlogs", optional)

        source_stack = workflow.split(
            "  xfce-source-stack:",
            maxsplit=1,
        )[1]
        self.assertIn("if: github.event_name == 'workflow_dispatch'", source_stack)

    def test_release_package_ordering_is_derived_from_selected_version(self):
        workflow = (
            ROOT / ".github/workflows/packaging.yml"
        ).read_text(encoding="utf-8")
        self.assertIn('stable_debian="${DEBIAN_VERSION%%~rc*}-1"', workflow)
        self.assertIn('stable_rpm="${RPM_VERSION%%~rc*}"', workflow)
        self.assertIn('stable_arch="${EXPECTED_ARCH_VERSION%%rc*}"', workflow)
        self.assertIn(
            "if rpm.vercmp('${RPM_VERSION}', '${stable_rpm}') >= 0 then",
            workflow,
        )
        self.assertNotIn(
            "rpm.vercmp('${RPM_VERSION}', '${stable_rpm}') < 0 or",
            workflow,
        )
        for obsolete in ("0.9.0~rc2", "0.9.0rc2", "RC1 must precede RC2"):
            self.assertNotIn(obsolete, workflow)


if __name__ == "__main__":
    unittest.main()
