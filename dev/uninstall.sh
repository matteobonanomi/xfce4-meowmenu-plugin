#!/usr/bin/env bash
# uninstall.sh — remove a MeowMenu dev install and wipe every trace of
#                    user configuration, leaving the system as if MeowMenu had
#                    never been installed.
#
# BACKGROUND
#   Use this before creating a release branch or PR to verify you are starting
#   from a clean slate — no installed files, no leftover Xfconf state, no user
#   presets, no /initialized marker.  It is the counterpart of install.sh;
#   both are dev/debug helpers and are not part of the public install
#   documentation.
#
#   The script reads the install prefix from the build directory so it removes
#   exactly what install.sh put there. It also cleans exact MeowMenu modules
#   from the conventional user-local and /usr/local development prefixes.
#   Files under /usr are left to the package manager.
#
#   xfconfd is NOT touched: killing it would race with the panel and corrupt
#   the panel layout.  Only the meowmenu wrapper process is killed; the panel
#   itself keeps running with all other plugins intact.
#
# USAGE
#   ./dev/uninstall.sh [BUILD_DIR]
#
#   BUILD_DIR  Meson build directory (default: ./build at the repo root).

set -euo pipefail

# This script lives under dev/; the repo root (and its default build dir) is its
# parent directory.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${REPO}/build}"

step() { echo "  » $*"; }

# /usr may contain a package-managed MeowMenu payload. /usr/local is the
# supported system-wide development prefix and is intentionally excluded.
# is_protected_system_path:
# @1: path to classify.
#
# Returns success when the path belongs to the package-managed /usr tree.
is_protected_system_path() {
    local path
    path="$(realpath -m -- "$1")"
    case "${path}" in
        /usr|/usr/*)
            [[ "${path}" != "/usr/local" && "${path}" != /usr/local/* ]]
            ;;
        *)
            return 1
            ;;
    esac
}

# remove_exact_file:
# @1: exact file path to remove.
#
# Deletes a development file, using privilege escalation only when needed;
# package-managed paths remain untouched.
remove_exact_file() {
    local target="$1"

    # Classify the parent so a development symlink itself can be removed
    # without ever following it into the package-managed /usr tree.
    if is_protected_system_path "$(dirname -- "${target}")"; then
        step "Preserve package-managed ${target}"
        return 0
    fi
    if rm -f -- "${target}" 2>/dev/null; then
        return 0
    fi
    sudo rm -f -- "${target}" 2>/dev/null || true
}

# remove_exact_tree:
# @1: exact directory path to remove.
#
# Deletes a development directory, using privilege escalation only when
# needed; package-managed paths remain untouched.
remove_exact_tree() {
    local target="$1"

    if is_protected_system_path "${target}"; then
        step "Preserve package-managed ${target}"
        return 0
    fi
    if rm -rf -- "${target}" 2>/dev/null; then
        return 0
    fi
    sudo rm -rf -- "${target}" 2>/dev/null || true
}

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
    echo "  Other custom prefixes are not searched; inspect exact module paths manually."
    echo "  find /path/to/prefix -path '*/xfce4/panel/plugins/libmeowmenu.so' -print"
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

# Find only the known module filename in development prefixes. Do not search
# /usr: package removal is the package manager's responsibility.
KNOWN_ROOTS=("${HOME}/.local" "/usr/local")
if ! is_protected_system_path "${PREFIX}"; then
    KNOWN_ROOTS+=("${PREFIX}")
fi
declare -A seen_roots=()
for root in "${KNOWN_ROOTS[@]}"; do
    root="$(realpath -m -- "${root}")"
    [[ -d "${root}" ]] || continue
    if [[ -n "${seen_roots[${root}]+x}" ]]; then
        continue
    fi
    seen_roots["${root}"]=1
    while IFS= read -r -d '' candidate; do
        step "Remove development module ${candidate}"
        remove_exact_file "${candidate}"
        if [[ -e "${candidate}" || -L "${candidate}" ]]; then
            echo "FAILED: development module remains: ${candidate}"
            exit 1
        fi
    done < <(find "${root}" \
        -path '*/xfce4/panel/plugins/libmeowmenu.so' \
        \( -type f -o -type l \) -print0 2>/dev/null)
done

# ---------------------------------------------------------------------------
# Remove installed files
# ---------------------------------------------------------------------------

# do_rm:
# @*: exact file paths belonging to the selected development installation.
#
# Applies the package-path guard to each requested file.
do_rm() {
    local target
    for target in "$@"; do
        remove_exact_file "${target}"
    done
}

# do_rmr:
# @*: exact directory paths belonging to the selected development install.
#
# Applies the package-path guard to each requested directory.
do_rmr() {
    local target
    for target in "$@"; do
        remove_exact_tree "${target}"
    done
}

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
do_rm "${DATADIR}/metainfo/io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml"

step "Remove icons"
for size in 16 22 24 32 48 64 128 256; do
    do_rm "${DATADIR}/icons/hicolor/${size}x${size}/apps/org.xfce.panel.meowmenu.png"
done
do_rm "${DATADIR}/icons/hicolor/scalable/apps/org.xfce.panel.meowmenu.svg"

step "Remove translations"
if is_protected_system_path "${LOCALEDIR}"; then
    step "Preserve package-managed translations under ${LOCALEDIR}"
else
    while IFS= read -r -d '' translation; do
        do_rm "${translation}"
    done < <(find "${LOCALEDIR}" -name 'xfce4-meowmenu-plugin.mo' \
        -type f -print0 2>/dev/null)
fi

# ---------------------------------------------------------------------------
# Wipe user configuration
# ---------------------------------------------------------------------------

USER_DATA_ROOT="${XDG_DATA_HOME:-${HOME}/.local/share}"
USER_MEOWMENU_DATA="${USER_DATA_ROOT%/}/meowmenu"
if [[ "${USER_DATA_ROOT}" != /* || "${USER_DATA_ROOT}" == "/" ]] \
        || is_protected_system_path "${USER_MEOWMENU_DATA}"; then
    echo "Refusing unsafe XDG_DATA_HOME '${USER_DATA_ROOT}'."
    exit 1
fi
step "Remove user presets"
rm -rf -- "${USER_MEOWMENU_DATA}" 2>/dev/null || true

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

# Usage statistics live outside Xfconf and the preset directory. Remove the
# same dedicated cache directory as install.sh so ranking history is cleared.
USER_CACHE_ROOT="${XDG_CACHE_HOME:-${HOME}/.cache}"
USER_CACHE_DIR="${USER_CACHE_ROOT%/}/xfce4/meowmenu"
if [[ "${USER_CACHE_ROOT}" != /* || "${USER_CACHE_ROOT}" == "/" ]] \
        || is_protected_system_path "${USER_CACHE_DIR}"; then
    echo "Refusing unsafe XDG_CACHE_HOME '${USER_CACHE_ROOT}'."
    exit 1
fi
if [[ -e "${USER_CACHE_DIR}" ]]; then
    step "Remove usage cache ${USER_CACHE_DIR}"
    rm -rf -- "${USER_CACHE_DIR}" 2>/dev/null || true
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
