#!/usr/bin/env bash
# aur-release.sh — refresh the AUR recipe checksum for the current release.
#
# Copies dist/arch/PKGBUILD into the sibling AUR clone, aligns its pkgver with
# NEWS (the version source of truth), recomputes sha256sums against the freshly
# downloaded release tarball, regenerates .SRCINFO, and verifies. It stops
# before any git action; committing and pushing to the AUR stay manual.
#
# makepkg/updpkgsums are Arch-only: the script runs them natively on Arch, or
# in a disposable archlinux container on any other host (needs docker).
#
# Usage: ./dev/aur-release.sh [AUR_REPO_DIR]
#   AUR_REPO_DIR defaults to ../xfce4-meowmenu-plugin-AUR (sibling of this repo).
#
# Prerequisite: the release tag vX.Y.Z (matching NEWS) must already be pushed,
# or the tarball download 404s.
set -euo pipefail

die() { printf 'aur-release: %s\n' "$*" >&2; exit 1; }

# Paths resolved from the script location so it runs from any directory.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DIST_ARCH="${REPO_ROOT}/dist/arch"
AUR_REPO="${1:-$(cd "${REPO_ROOT}/.." && pwd)/xfce4-meowmenu-plugin-AUR}"

[ -f "${DIST_ARCH}/PKGBUILD" ] || die "missing recipe: ${DIST_ARCH}/PKGBUILD"
[ -d "${AUR_REPO}/.git" ]      || die "not an AUR git clone: ${AUR_REPO}"

# NEWS is authoritative for the version (same source the release CI uses).
NEWS_VER="$(python3 "${REPO_ROOT}/build-aux/news-version.py" --version)" \
	|| die "could not read version from NEWS"
OLD_SUM="$(sed -n "s/^sha256sums=('\\([0-9a-f]*\\)').*/\\1/p" "${AUR_REPO}/PKGBUILD" 2>/dev/null || true)"

printf '==> AUR release for version %s (from NEWS)\n' "${NEWS_VER}"
printf '    recipe : %s/PKGBUILD\n' "${DIST_ARCH}"
printf '    AUR    : %s\n\n' "${AUR_REPO}"

# Step 1: copy the maintained recipe into the AUR clone (single source).
printf '==> Step 1/5: copy dist/arch/PKGBUILD into the AUR clone\n'
cp "${DIST_ARCH}/PKGBUILD" "${AUR_REPO}/PKGBUILD"

# Step 2: align pkgver with NEWS. Must precede the download — pkgver builds the
# tarball URL (v${pkgver}), so a stale value would fetch the wrong release.
printf '==> Step 2/5: check pkgver against NEWS\n'
PKG_VER="$(sed -n 's/^pkgver=//p' "${AUR_REPO}/PKGBUILD" | head -n1)"
if [ "${PKG_VER}" = "${NEWS_VER}" ]; then
	printf '    OK: pkgver=%s matches NEWS\n' "${PKG_VER}"
else
	printf '    MISMATCH: pkgver=%s, NEWS=%s -> correcting to NEWS\n' "${PKG_VER}" "${NEWS_VER}"
	sed -i "s/^pkgver=.*/pkgver=${NEWS_VER}/" "${AUR_REPO}/PKGBUILD"
	printf '    NOTE: dist/arch/PKGBUILD pkgver is also stale; fix it at the source too.\n'
fi

# Step 3: drop makepkg leftovers so the next step re-downloads the published
# tarball instead of hashing a stale local copy. Only build outputs are removed.
printf '==> Step 3/5: remove stale build artifacts (force fresh download)\n'
rm -rf "${AUR_REPO}/src" "${AUR_REPO}/pkg"
rm -f  "${AUR_REPO}"/*.tar.gz "${AUR_REPO}"/*.pkg.tar.* 2>/dev/null || true

# Step 4: refresh checksum, regenerate .SRCINFO, verify the download.
printf '==> Step 4/5: recompute checksum, regenerate .SRCINFO, verify\n'
REFRESH='updpkgsums && makepkg --printsrcinfo > .SRCINFO && makepkg --verifysource'
if command -v updpkgsums >/dev/null 2>&1 && command -v makepkg >/dev/null 2>&1; then
	printf '    using native Arch tooling\n'
	[ "$(id -u)" -ne 0 ] || die "run as a normal user (makepkg refuses root)"
	( cd "${AUR_REPO}" && eval "${REFRESH}" )
else
	printf '    no native makepkg; using archlinux container\n'
	command -v docker >/dev/null 2>&1 || die "need makepkg+updpkgsums (Arch) or docker"
	# Build user gets the host UID so files written back stay owned by the caller.
	# REFRESH is passed via env so the container shell expands it (no nested quoting).
	docker run --rm -e HOST_UID="$(id -u)" -e REFRESH="${REFRESH}" \
		-v "${AUR_REPO}:/work" archlinux:base-devel bash -c '
			set -e
			pacman -Sy --noconfirm --needed pacman-contrib >/dev/null
			if getent passwd "$HOST_UID" >/dev/null; then
				u="$(getent passwd "$HOST_UID" | cut -d: -f1)"
			else
				useradd -m -u "$HOST_UID" builder; u=builder
			fi
			chown -R "$HOST_UID" /work
			su "$u" -c "cd /work && $REFRESH"
		'
fi

# Step 5: clean artifacts and report; the operator reviews and pushes manually.
printf '==> Step 5/5: clean up and summarize\n'
rm -rf "${AUR_REPO}/src" "${AUR_REPO}/pkg"
rm -f  "${AUR_REPO}"/*.tar.gz "${AUR_REPO}"/*.pkg.tar.* 2>/dev/null || true
NEW_SUM="$(sed -n "s/^sha256sums=('\\([0-9a-f]*\\)').*/\\1/p" "${AUR_REPO}/PKGBUILD" | head -n1)"

printf '\n    version : %s\n' "${NEWS_VER}"
printf '    old sum : %s\n' "${OLD_SUM:-<none>}"
printf '    new sum : %s\n' "${NEW_SUM:-<unknown>}"
printf '\n==> Pending changes (review before committing):\n'
git -C "${AUR_REPO}" status --short
printf '\nNext (manual): cd %s && git add PKGBUILD .SRCINFO && git commit && git push\n' "${AUR_REPO}"
