#!/usr/bin/env bash
#
# prepare-source.sh: source path for every trigger. Builds a local tarball from
# the current checkout (the tagged commit on a release run, HEAD otherwise) and
# patches ONLY a scratch copy of the PKGBUILD to point at it with a freshly
# computed checksum and the resolved version. The committed dist/arch/PKGBUILD
# is never written (FR-013).
#
# The package version is tag-driven, exactly like the .deb/.rpm jobs: on a
# release-tag run (push of vX.Y.Z) the tag is authoritative; the committed
# pkgver is only a development fallback for untagged runs. This keeps the Arch
# validation building the actual tagged source instead of requiring a
# hand-maintained pkgver + checksum pinned into the recipe before tagging.
#
# Emits the scratch build directory on stdout for the downstream build step.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Read pkgname and the fallback pkgver from the committed recipe without running
# build logic.
# shellcheck disable=SC1090,SC1091,SC2154
eval "$(source "${REPO_ROOT}/dist/arch/PKGBUILD"; printf 'pkgname=%q\npkgver=%q\n' "${pkgname}" "${pkgver}")"

# Tag-driven version: on a release-tag push (vX.Y.Z) the tag wins, mirroring how
# the .deb/.rpm jobs derive their version. GITHUB_* are provided by the workflow
# step; absent them (local runs) the committed pkgver fallback stands.
ref_name="${GITHUB_REF_NAME:-}"
if [ "${GITHUB_EVENT_NAME:-}" = "push" ] && [[ "${ref_name}" == v[0-9]* ]]; then
  pkgver="${ref_name#v}"
fi

work="$(mktemp -d)"
tarball="${work}/${pkgname}-${pkgver}.tar.gz"

# Mirror the published tag archive's top-level directory name so the recipe's
# build()/check()/package() resolve the same source path on both code paths.
git -C "${REPO_ROOT}" archive --format=tar.gz \
  --prefix="${pkgname}-${pkgver}/" HEAD > "${tarball}"

cp "${REPO_ROOT}/dist/arch/PKGBUILD" "${work}/PKGBUILD"
sum="$(sha256sum "${tarball}" | cut -d' ' -f1)"

# Patch ONLY the scratch copy: resolved version + local tarball + its real
# checksum. pkgver is patched so build()/check()/package() resolve the same
# ${pkgname}-${pkgver} source directory the tarball prefix above created.
sed -i "s#^pkgver=.*#pkgver=${pkgver}#" "${work}/PKGBUILD"
sed -i "s#^source=.*#source=(\"${pkgname}-${pkgver}.tar.gz\")#" "${work}/PKGBUILD"
sed -i "s#^sha256sums=.*#sha256sums=('${sum}')#" "${work}/PKGBUILD"

chown -R builder "${work}" 2>/dev/null || true

echo "Prepared local-tarball build dir at ${work} (sha256=${sum})" >&2
printf '%s\n' "${work}"
