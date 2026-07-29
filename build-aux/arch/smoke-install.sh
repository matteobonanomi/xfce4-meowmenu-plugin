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
pacman -Q exo
test "$(pacman -Qo /usr/bin/exo-open | awk '{print $5}')" = exo
test "$(pacman -Qo /usr/bin/exo-desktop-item-edit | awk '{print $5}')" = exo

plugin="$(find /usr/lib -path '*/xfce4/panel/plugins/libmeowmenu.so' -print -quit)"
script_dir="$(cd "$(dirname "$0")" && pwd)"
"${script_dir}/../compat/assert-dependency-regime.sh" \
  --regime legacy --plugin "$plugin"
"${script_dir}/../compat/installed-action-smoke.sh" \
  --regime legacy --plugin "$plugin"

echo "smoke install OK: xfce4-meowmenu-plugin is registered"
