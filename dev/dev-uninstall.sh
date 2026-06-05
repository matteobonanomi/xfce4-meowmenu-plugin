#!/usr/bin/env bash
# dev-uninstall.sh — remove a MeowMenu dev install and wipe all user config.
#
# BACKGROUND
#   Use this before creating a release branch or PR to verify you are starting
#   from a clean slate — no installed files, no leftover Xfconf state.
#   It is the counterpart of dev-install.sh; both are dev/debug helpers and are
#   not part of the public install documentation.
#
#   The script reads the install prefix from the build directory so it removes
#   exactly what dev-install.sh put there.  If the build directory is missing
#   it falls back to /usr/local.
#
#   xfconfd is NOT touched: killing it would race with the panel and corrupt
#   the panel layout.  Only the meowmenu wrapper process is killed; the panel
#   itself keeps running with all other plugins intact.
#
# USAGE
#   ./dev/dev-uninstall.sh [BUILD_DIR]
#
#   BUILD_DIR  Meson build directory (default: ./build at the repo root).

set -euo pipefail

# This script lives under dev/; the repo root (and its default build dir) is its
# parent directory.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${REPO}/build}"

step() { echo "  » $*"; }

# ---------------------------------------------------------------------------
# Resolve install prefix
# ---------------------------------------------------------------------------

if [[ -d "${BUILD_DIR}" ]]; then
    PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
        | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"
    LIBDIR="$(meson introspect --buildoptions "${BUILD_DIR}" \
        | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="libdir"][0])')"
else
    echo "  Build directory '${BUILD_DIR}' not found, falling back to prefix=/usr/local"
    PREFIX="/usr/local"
    LIBDIR="lib/x86_64-linux-gnu"
fi

# libdir may be absolute (/usr/local/lib/…) or relative (lib/…); normalise.
if [[ "${LIBDIR}" == /* ]]; then
    FULL_LIBDIR="${LIBDIR}"
else
    FULL_LIBDIR="${PREFIX}/${LIBDIR}"
fi

DATADIR="${PREFIX}/share"
LOCALEDIR="${PREFIX}/share/locale"

echo ""
echo "MeowMenu dev-uninstall  (prefix: ${PREFIX})"
echo "─────────────────────────────────────────"

# ---------------------------------------------------------------------------
# Remove installed files
# ---------------------------------------------------------------------------

NEEDS_SUDO=""
[[ ! -w "${PREFIX}" ]] && NEEDS_SUDO="sudo"

do_rm()  { ${NEEDS_SUDO} rm -f  "$@" 2>/dev/null || true; }
do_rmr() { ${NEEDS_SUDO} rm -rf "$@" 2>/dev/null || true; }

step "Remove plugin library"
do_rm "${FULL_LIBDIR}/xfce4/panel/plugins/libmeowmenu.so"

step "Remove popup helper binary"
do_rm "${PREFIX}/bin/xfce4-popup-meowmenu"

step "Remove panel plugin descriptor"
do_rm "${DATADIR}/xfce4/panel/plugins/meowmenu.desktop"

step "Remove man page"
do_rm "${DATADIR}/man/man1/xfce4-popup-meowmenu.1"

step "Remove built-in presets"
do_rmr "${DATADIR}/meowmenu"

step "Remove package data"
do_rmr "${DATADIR}/xfce4-meowmenu-plugin"

step "Remove appstream metainfo"
do_rm "${DATADIR}/metainfo/xfce4-meowmenu-plugin.appdata.xml"

step "Remove icons"
for size in 16 22 24 32 48 64 128 256; do
    do_rm "${DATADIR}/icons/hicolor/${size}x${size}/apps/org.xfce.panel.meowmenu.png"
done
do_rm "${DATADIR}/icons/hicolor/scalable/apps/org.xfce.panel.meowmenu.svg"

step "Remove translations"
find "${LOCALEDIR}" -name 'xfce4-meowmenu-plugin.mo' -exec ${NEEDS_SUDO} rm -f {} + 2>/dev/null || true

# ---------------------------------------------------------------------------
# Wipe user configuration
# ---------------------------------------------------------------------------

step "Remove user presets"
rm -rf "${HOME}/.local/share/meowmenu/" 2>/dev/null || true

# Legacy on-disk channel file (pre-rename installs); harmless to remove if present.
XFCONF_FILE="${HOME}/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml"
if [[ -f "${XFCONF_FILE}" ]]; then
    step "Remove legacy Xfconf channel file (meowmenu.xml)"
    rm -f "${XFCONF_FILE}"
fi

# The plugin's live settings are NOT in meowmenu.xml: they sit in the
# xfce4-panel channel under each instance's base (/plugins/plugin-N), including
# the /initialized first-run marker. Removing only the file would leave xfconfd
# serving the stale in-memory state, so the next install would be misdetected as
# an upgrade and land on the wrong preset. Reset the marker and the whole plugin
# subtree directly through xfconfd so a reinstall is a genuine clean install.
# NOTE: a targeted --reset --recursive is used deliberately INSTEAD of killing
# xfconfd, which would race with the panel and corrupt the panel layout.
if command -v xfconf-query >/dev/null 2>&1; then
    step "Reset MeowMenu Xfconf state (xfce4-panel channel + /initialized marker)"
    while IFS= read -r prop; do
        # Match exactly /plugins/plugin-N (a plugin slot), not its children.
        if [[ "${prop}" =~ ^/plugins/plugin-[0-9]+$ ]]; then
            value="$(xfconf-query --channel xfce4-panel --property "${prop}" 2>/dev/null || true)"
            if [[ "${value}" == "meowmenu" ]]; then
                step "  Reset ${prop} (recursive)"
                # Clears every key under the base, marker included, in xfconfd's
                # in-memory store and on disk.
                xfconf-query --channel xfce4-panel --property "${prop}" \
                    --reset --recursive 2>/dev/null || true
            fi
        fi
    done < <(xfconf-query --channel xfce4-panel --list 2>/dev/null || true)
else
    step "xfconf-query not found — skipping channel reset (reinstall may not be detected as fresh)"
fi

# ---------------------------------------------------------------------------
# Kill only the MeowMenu wrapper — leave the panel and xfconfd running
# ---------------------------------------------------------------------------
# xfce4-panel wrapper-2.0 loads libmeowmenu.so in a dedicated process.
# Killing just that process makes the panel drop the plugin slot; all other
# panels and plugins are unaffected.  xfconfd is deliberately NOT killed here
# to avoid a race that would corrupt the panel layout on restart.

step "Kill MeowMenu wrapper"
pkill -f 'wrapper-2.0.*libmeowmenu\.so' 2>/dev/null || true

echo "─────────────────────────────────────────"
echo "  Done. MeowMenu removed from ${PREFIX}."
echo "  The panel slot may show a placeholder — remove it via right-click → Remove."
echo ""
