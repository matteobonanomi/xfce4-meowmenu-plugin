#!/usr/bin/env bash
#
# check-version.sh: fail unless the committed dist/arch/PKGBUILD pkgver equals
# the project version derived from NEWS (build-aux/news-version.py). This keeps
# the Arch recipe's version tracking the single source of truth, exactly as the
# .deb/.rpm jobs do.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

expected="$(python3 "${REPO_ROOT}/build-aux/news-version.py" --version)"

# Source the committed recipe in a subshell to read its pkgver without running
# any build logic. The recipe path is not a constant, so silence shellcheck's
# follow warning.
# shellcheck disable=SC1090,SC1091,SC2154
actual="$(source "${REPO_ROOT}/dist/arch/PKGBUILD"; printf '%s' "${pkgver}")"

if [ "${expected}" != "${actual}" ]; then
  echo "::error::PKGBUILD pkgver '${actual}' does not match NEWS version '${expected}'"
  exit 1
fi

echo "pkgver '${actual}' matches NEWS version"
