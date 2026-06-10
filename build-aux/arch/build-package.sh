#!/usr/bin/env bash
#
# build-package.sh: build the package with makepkg as the non-root 'builder'
# user. --syncdeps installs declared dependencies, --check runs the package
# check() phase (the project's meson test suite), --cleanbuild/--force keep
# repeated CI runs deterministic. A build or test failure exits non-zero.
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
  --syncdeps --noconfirm --cleanbuild --check --force >&2

shopt -s nullglob
pkgs=("${build_dir}"/*.pkg.tar.*)
shopt -u nullglob
if [ "${#pkgs[@]}" -eq 0 ]; then
  echo "::error::makepkg produced no package artifact in ${build_dir}"
  exit 1
fi

printf '%s\n' "${pkgs[0]}"
