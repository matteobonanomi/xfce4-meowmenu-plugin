#!/usr/bin/env bash
#
# prepare-source.sh: source path for every trigger. Builds a local tarball from
# the current checkout (the tagged commit on a release run, HEAD otherwise) and
# patches ONLY a scratch copy of the PKGBUILD to point at it with a freshly
# computed checksum and the resolved version. The committed dist/arch/PKGBUILD
# is never written (the documented behavior).
#
# The package version is supplied explicitly by packaging CI or derived from a
# release-tag push. The committed pkgver is only a standalone development
# fallback. This keeps validation tied to NEWS without requiring a
# hand-maintained pkgver + checksum before tagging.
#
# Emits the scratch build directory on stdout for the downstream build step.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Read the package name and fallback versions without running build logic.
# shellcheck disable=SC1090,SC1091,SC2154
eval "$(source "${REPO_ROOT}/dist/arch/PKGBUILD"; printf 'pkgname=%q\npkgver=%q\nupstream_version=%q\n' "${pkgname}" "${pkgver}" "${_upstream_version}")"

# Preserve the public version for the source directory while deriving Arch's
# hyphen-free pkgver with the shared NEWS mapper.
package_version="${MEOWMENU_PACKAGE_VERSION:-}"
ref_name="${GITHUB_REF_NAME:-}"
if [[ -n "${package_version}" ]]; then
  news_version="$(python3 "${REPO_ROOT}/build-aux/news-version.py" \
    --version --news "${REPO_ROOT}/NEWS")"
  if [[ "${package_version}" != "${news_version}" ]]; then
    echo "::error::Requested package version ${package_version} does not match NEWS ${news_version}" >&2
    exit 1
  fi
  upstream_version="${package_version}"
  pkgver="$(python3 "${REPO_ROOT}/build-aux/news-version.py" \
    --arch-version --news "${REPO_ROOT}/NEWS")"
elif [ "${GITHUB_EVENT_NAME:-}" = "push" ] && [[ "${ref_name}" == v[0-9]* ]]; then
  upstream_version="${ref_name#v}"
  pkgver="$(python3 "${REPO_ROOT}/build-aux/news-version.py" \
    --arch-version --news "${REPO_ROOT}/NEWS")"
fi

work="$(mktemp -d)"
tarball="${work}/${pkgname}-${upstream_version}.tar.gz"

# Mirror the published tag archive's top-level directory name so the recipe's
# build()/check()/package() resolve the same source path on both code paths.
git -C "${REPO_ROOT}" archive --format=tar.gz \
  --prefix="${pkgname}-${upstream_version}/" HEAD > "${tarball}"

cp "${REPO_ROOT}/dist/arch/PKGBUILD" "${work}/PKGBUILD"
sum="$(sha256sum "${tarball}" | cut -d' ' -f1)"

# Patch only the scratch copy with public/native versions and a real checksum.
sed -i "s#^_upstream_version=.*#_upstream_version=${upstream_version}#" "${work}/PKGBUILD"
sed -i "s#^pkgver=.*#pkgver=${pkgver}#" "${work}/PKGBUILD"
sed -i "s#^source=.*#source=(\"${pkgname}-${upstream_version}.tar.gz\")#" "${work}/PKGBUILD"
sed -i "s#^sha256sums=.*#sha256sums=('${sum}')#" "${work}/PKGBUILD"

chown -R builder "${work}" 2>/dev/null || true

echo "Prepared local-tarball build dir at ${work} (sha256=${sum})" >&2
printf '%s\n' "${work}"
