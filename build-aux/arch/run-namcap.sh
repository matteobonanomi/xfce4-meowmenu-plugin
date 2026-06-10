#!/usr/bin/env bash
#
# run-namcap.sh: lint the recipe and the produced package with namcap. Errors
# (lines containing ' E: ') fail the job; warnings are advisory but logged so
# future drift stays visible (each tolerated warning is justified in
# dist/arch/README.md).
#
# Usage: run-namcap.sh <pkgbuild-path> <package-path> [log-dir]
#
set -euo pipefail

pkgbuild="${1:?usage: run-namcap.sh <pkgbuild> <package> [log-dir]}"
pkg="${2:?usage: run-namcap.sh <pkgbuild> <package> [log-dir]}"
logdir="${3:-$(mktemp -d)}"

mkdir -p "${logdir}"
namcap "${pkgbuild}" | tee "${logdir}/namcap-pkgbuild.log"
namcap "${pkg}"      | tee "${logdir}/namcap-package.log"

# Explicit grep-then-exit (no shell short-circuit) so an error line reliably
# fails the job regardless of grep's place in a pipeline.
if grep -E ' E: ' "${logdir}"/namcap-*.log; then
  echo "::error::namcap reported errors (see lines above)"
  exit 1
fi

echo "namcap: no errors (warnings, if any, are advisory)"
printf '%s\n' "${logdir}"
