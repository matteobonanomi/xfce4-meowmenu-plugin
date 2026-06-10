#!/usr/bin/env bash
#
# check-whisker-overlap.sh: prove the produced package coexists with Arch's
# official xfce4-whiskermenu-plugin and never substitutes it. Mirrors the
# standing .deb/.rpm coexistence guarantee for Arch.
#
# Assertions:
#   1. Whisker + MeowMenu both install and stay installed (no refused conflict).
#   2. No shared non-directory file path between the two packages.
#   3. The produced package's .PKGINFO declares no provides/conflict/replaces
#      against xfce4-whiskermenu-plugin.
#   4. The committed recipe declares none either.
#
# Usage: check-whisker-overlap.sh <package-path>
#
set -euo pipefail

pkg="${1:?usage: check-whisker-overlap.sh <package-path>}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"

# (1) Coexisting install: Whisker first, then MeowMenu. Both must remain.
pacman -S --noconfirm --needed xfce4-whiskermenu-plugin
pacman -U --noconfirm "${pkg}"
pacman -Qi xfce4-whiskermenu-plugin >/dev/null
pacman -Qi xfce4-meowmenu-plugin    >/dev/null

# (2) Non-directory file overlap. Only shared directory entries are allowed.
pacman -Qlq xfce4-meowmenu-plugin    | sort -u > "${work}/meow.txt"
pacman -Qlq xfce4-whiskermenu-plugin | sort -u > "${work}/whisker.txt"
comm -12 "${work}/meow.txt" "${work}/whisker.txt" > "${work}/overlap.txt"

: > "${work}/violations.txt"
while IFS= read -r path; do
  [ -z "${path}" ] && continue
  # NOTE: pacman -Ql marks directory entries with a trailing slash. That
  # package-metadata marker is the authoritative directory signal here: a
  # filesystem `[ -d ]` test is unreliable because shared directories listed in
  # the package DB may not be physically present at check time. Shared
  # directories (common Xfce/locale/man trees) are expected; only a shared
  # non-directory entry (a real file or symlink) is a coexistence violation.
  case "${path}" in
    */) continue ;;
  esac
  echo "${path}" >> "${work}/violations.txt"
done < "${work}/overlap.txt"

if [ -s "${work}/violations.txt" ]; then
  echo "::error::non-directory file overlap with xfce4-whiskermenu-plugin:"
  cat "${work}/violations.txt"
  exit 1
fi

# (3) No substitution metadata in the produced package. Explicit
# match-then-exit (no shell short-circuit precedence).
bsdtar -xOf "${pkg}" .PKGINFO > "${work}/pkginfo.txt"
if grep -Eq '^(provides|conflict|replaces) = xfce4-whiskermenu-plugin$' "${work}/pkginfo.txt"; then
  echo "::error::produced package declares substitution metadata against xfce4-whiskermenu-plugin:"
  grep -E '^(provides|conflict|replaces) = xfce4-whiskermenu-plugin$' "${work}/pkginfo.txt"
  exit 1
fi

# (4) No substitution metadata in the committed recipe.
if grep -Eq '^(conflicts|provides|replaces)=.*xfce4-whiskermenu-plugin' \
     "${REPO_ROOT}/dist/arch/PKGBUILD"; then
  echo "::error::committed PKGBUILD declares substitution metadata against xfce4-whiskermenu-plugin:"
  grep -E '^(conflicts|provides|replaces)=.*xfce4-whiskermenu-plugin' "${REPO_ROOT}/dist/arch/PKGBUILD"
  exit 1
fi

echo "Whisker coexistence OK: both installed, no non-directory overlap, no substitution metadata"
