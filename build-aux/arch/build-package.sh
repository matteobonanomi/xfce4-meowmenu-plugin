#!/usr/bin/env bash
#
# build-package.sh: build the package with makepkg as the non-root 'builder'
# user. --syncdeps installs declared dependencies; --cleanbuild/--force keep
# repeated CI runs deterministic. After makepkg succeeds, this script runs the
# project's meson test suite explicitly against the makepkg-produced build tree.
# A build or test failure exits non-zero.
#
# Usage: build-package.sh <build-dir>
# Emits the produced *.pkg.tar.* path on stdout.
#
set -euo pipefail

build_dir="${1:?usage: build-package.sh <build-dir>}"

run_makepkg() {
  if [ "$(id -u)" -eq 0 ]; then
    sudo -u builder "$@"
  else
    "$@"
  fi
}

cd "${build_dir}"

# LC_ALL=C keeps build/test output locale-stable for log diffing.
run_makepkg env LC_ALL=C makepkg \
  --syncdeps --noconfirm --cleanbuild --force >&2

# prepare-source.sh emits build_dir, and PKGBUILD creates the Meson build dir
# at ${build_dir}/src/build via arch-meson "${pkgname}-${pkgver}" build.
# makepkg --cleanbuild cleans srcdir before the build, but leaves it afterward,
# so this exercises the exact tree that produced the package without rebuilding.
run_makepkg env LC_ALL=C meson test -C "${build_dir}/src/build" \
  --print-errorlogs >&2

shopt -s nullglob
pkgs=("${build_dir}"/*.pkg.tar.*)
shopt -u nullglob
if [ "${#pkgs[@]}" -eq 0 ]; then
  echo "::error::makepkg produced no package artifact in ${build_dir}"
  exit 1
fi

printf '%s\n' "${pkgs[0]}"
