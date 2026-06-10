#!/usr/bin/env bash
#
# smoke-install.sh: install the produced package with pacman -U and assert it
# registers in the pacman database (queryable via -Qi and -Ql).
#
# Usage: smoke-install.sh <package-path>
#
set -euo pipefail

pkg="${1:?usage: smoke-install.sh <package-path>}"

pacman -U --noconfirm "${pkg}"
pacman -Qi xfce4-meowmenu-plugin
pacman -Ql xfce4-meowmenu-plugin >/dev/null

echo "smoke install OK: xfce4-meowmenu-plugin is registered"
