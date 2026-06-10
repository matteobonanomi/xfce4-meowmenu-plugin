#!/usr/bin/env bash
#
# check-source-checksum.sh: tag-run source path. Using the committed
# dist/arch/PKGBUILD unchanged, re-download the published v${pkgver} tarball and
# validate it against the committed sha256sums (no SKIP, no recomputation). A
# checksum mismatch fails the job — it signals the maintainer must re-pin, not
# that CI should bypass.
#
# Emits the resolved build directory (the committed dist/arch) on stdout for the
# downstream build step.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${REPO_ROOT}/dist/arch"

run_makepkg() {
  if [ "$(id -u)" -eq 0 ]; then
    sudo -u builder makepkg "$@"
  else
    makepkg "$@"
  fi
}

# --verifysource downloads the source and checks it against sha256sums only.
echo "Verifying committed checksum against the published tarball..." >&2
( cd "${build_dir}" && run_makepkg --verifysource --noconfirm ) >&2

# Hand the build directory to the next step.
printf '%s\n' "${build_dir}"
