#!/usr/bin/env bash
#
# prepare-source.sh: manual / pre-tag source path. Builds a local tarball from
# the current checkout and patches ONLY a scratch copy of the PKGBUILD to point
# at it with a freshly computed checksum. The committed dist/arch/PKGBUILD is
# never written (FR-013). This exists so the current tree can be built and
# tested before a v${pkgver} tag (and its published tarball) exists.
#
# Emits the scratch build directory on stdout for the downstream build step.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Read pkgname/pkgver from the committed recipe without running build logic.
# shellcheck disable=SC1090,SC1091,SC2154
eval "$(source "${REPO_ROOT}/dist/arch/PKGBUILD"; printf 'pkgname=%q\npkgver=%q\n' "${pkgname}" "${pkgver}")"

work="$(mktemp -d)"
tarball="${work}/${pkgname}-${pkgver}.tar.gz"

# Mirror the published tag archive's top-level directory name so the recipe's
# build()/check()/package() resolve the same source path on both code paths.
git -C "${REPO_ROOT}" archive --format=tar.gz \
  --prefix="${pkgname}-${pkgver}/" HEAD > "${tarball}"

cp "${REPO_ROOT}/dist/arch/PKGBUILD" "${work}/PKGBUILD"
sum="$(sha256sum "${tarball}" | cut -d' ' -f1)"

# Patch ONLY the scratch copy: local tarball + its real checksum.
sed -i "s#^source=.*#source=(\"${pkgname}-${pkgver}.tar.gz\")#" "${work}/PKGBUILD"
sed -i "s#^sha256sums=.*#sha256sums=('${sum}')#" "${work}/PKGBUILD"

chown -R builder "${work}" 2>/dev/null || true

echo "Prepared local-tarball build dir at ${work} (sha256=${sum})" >&2
printf '%s\n' "${work}"
