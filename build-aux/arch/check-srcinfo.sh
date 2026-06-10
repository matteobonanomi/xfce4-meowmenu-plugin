#!/usr/bin/env bash
#
# check-srcinfo.sh: regenerate .SRCINFO from the committed, unpatched
# dist/arch/PKGBUILD and fail on any difference against the committed
# dist/arch/.SRCINFO. AUR tooling reads .SRCINFO, so staleness is a defect;
# this runs BEFORE any CI source patch so the committed metadata is what gets
# validated.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# makepkg refuses to run as root. When invoked as root (the CI container), run
# it as the unprivileged 'builder' user; otherwise run it directly.
run_makepkg() {
  if [ "$(id -u)" -eq 0 ]; then
    sudo -u builder makepkg "$@"
  else
    makepkg "$@"
  fi
}

# Operate on a builder-owned scratch copy so makepkg can read it as 'builder'
# and the committed file is never touched.
work="$(mktemp -d)"
cp "${REPO_ROOT}/dist/arch/PKGBUILD" "${work}/PKGBUILD"
chown -R builder "${work}" 2>/dev/null || true

( cd "${work}" && run_makepkg --printsrcinfo ) > "${work}/.SRCINFO.fresh"

if ! diff -u "${REPO_ROOT}/dist/arch/.SRCINFO" "${work}/.SRCINFO.fresh"; then
  echo "::error::dist/arch/.SRCINFO is stale; regenerate it with 'makepkg --printsrcinfo > .SRCINFO'"
  exit 1
fi

echo ".SRCINFO is fresh against the committed PKGBUILD"
