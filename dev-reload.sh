#!/usr/bin/env bash
# dev-reload.sh — build, install, and hot-reload MeowMenu in a running Xfce session.
#
# BACKGROUND
#   MeowMenu runs inside xfce4-panel's `wrapper-2.0` process, not inside the panel
#   itself.  Simply restarting the panel with `xfce4-panel -r` does NOT recycle the
#   wrapper, so the old libmeowmenu.so stays loaded.  This script kills the wrapper
#   explicitly so the panel re-spawns it against the freshly installed library.
#
#   There must also never be two copies of the .so on disk at the same time: Xfce
#   searches XDG_DATA_DIRS user-first, so a stale ~/.local copy silently shadows the
#   one you just installed under /usr/local.  The script removes that stale copy.
#
# USAGE
#   ./dev-reload.sh [--icons] [BUILD_DIR]
#
#   --icons     Re-render icons/hi*-app-meowmenu.png from assets/meowmenu.svg
#               before building.  Requires rsvg-convert (preferred) or inkscape.
#               Without this flag icons are left as-is (the committed PNGs are used).
#   BUILD_DIR   Meson build directory (default: ./build).
#               Must already exist; run `meson setup build` once first.
#
# LOGS
#   Verbose output from meson and the compiler is redirected to .logs/dev-reload.log
#   (rotated on each run).  Check that file if a step fails.

set -euo pipefail

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="${REPO}/.logs"
LOG_FILE="${LOG_DIR}/dev-reload.log"

mkdir -p "${LOG_DIR}"
# Rotate: start a fresh log for every run.
> "${LOG_FILE}"

# step <label>  — print a short progress line to the terminal.
step() { echo "  » $*"; }

# run <label> <cmd...>  — run a command silently; on failure print the last
#                         20 lines of the log and exit.
run() {
    local label="$1"; shift
    step "${label}"
    if ! "$@" >> "${LOG_FILE}" 2>&1; then
        echo ""
        echo "FAILED: ${label}"
        echo "Last output (see ${LOG_FILE} for the full log):"
        tail -20 "${LOG_FILE}"
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

REGEN_ICONS=false
BUILD_DIR=""

for arg in "$@"; do
    case "${arg}" in
        --icons) REGEN_ICONS=true ;;
        *)       BUILD_DIR="${arg}" ;;
    esac
done

BUILD_DIR="${BUILD_DIR:-${REPO}/build}"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory '${BUILD_DIR}' not found."
    echo "Run first:  meson setup ${BUILD_DIR} --prefix=/usr/local"
    exit 1
fi

echo ""
echo "MeowMenu dev-reload  (log → ${LOG_FILE})"
echo "─────────────────────────────────────────"

# ---------------------------------------------------------------------------
# Optional: re-render icons from master SVG
# ---------------------------------------------------------------------------
# Without --icons the committed PNGs are used as-is.  Pass --icons when you
# have replaced assets/meowmenu.svg and want the hi*-app-meowmenu.png files
# to be regenerated before the build.  Requires rsvg-convert or inkscape.

if [[ "${REGEN_ICONS}" == true ]]; then
    run "Regen icons from assets/meowmenu.svg" \
        python3 "${REPO}/tools/regen-icons.py" \
            --input  "${REPO}/assets/meowmenu.svg" \
            --output-dir "${REPO}/icons"
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
# `meson setup --reconfigure` re-reads meson.build and NEWS (via
# tools/news-version.py) without a full clean rebuild.  This picks up version
# bumps and any meson.build changes automatically.

run "Reconfigure (pick up NEWS version + meson changes)" \
    meson setup --reconfigure "${BUILD_DIR}"

run "Compile" \
    meson compile -C "${BUILD_DIR}"

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
# Use sudo only when the install prefix is not user-writable.

PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
    | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"

if [[ -w "${PREFIX}" ]]; then
    run "Install to ${PREFIX}" \
        meson install -C "${BUILD_DIR}"
else
    step "Install to ${PREFIX} (sudo)"
    if ! sudo meson install -C "${BUILD_DIR}" >> "${LOG_FILE}" 2>&1; then
        echo "FAILED: Install"; tail -20 "${LOG_FILE}"; exit 1
    fi
fi

# ---------------------------------------------------------------------------
# Remove stale user-prefix shadow copy
# ---------------------------------------------------------------------------
# If a previous install left a copy in ~/.local, it would silently win over
# the system prefix because XDG_DATA_DIRS is searched user-first.

USER_SO="${HOME}/.local/lib/x86_64-linux-gnu/xfce4/panel/plugins/libmeowmenu.so"
if [[ "${PREFIX}" != "${HOME}/.local" && -e "${USER_SO}" ]]; then
    step "Remove stale ${USER_SO}"
    rm -f "${USER_SO}"
fi

# ---------------------------------------------------------------------------
# Reset Xfconf state
# ---------------------------------------------------------------------------
# Delete the persisted channel XML so the plugin starts from the Modern preset
# defaults (Settings::migrate_schema sees an empty channel and applies
# PRESET_MODERN).  Kill xfconfd with SIGKILL *after* the delete so it cannot
# flush its in-memory state back to disk before dying; D-Bus auto-restarts it.

XFCONF_FILE="${HOME}/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml"
if [[ -f "${XFCONF_FILE}" ]]; then
    step "Reset Xfconf channel (meowmenu.xml)"
    rm -f "${XFCONF_FILE}"
fi
pkill -9 xfconfd 2>/dev/null || true

# ---------------------------------------------------------------------------
# Reload panel
# ---------------------------------------------------------------------------
# Kill the wrapper process that holds the old .so in memory, then ask the
# panel to relaunch any missing plugins.  `xfce4-panel -r` alone is not
# enough because it does not recycle live wrappers.

pkill -f 'wrapper-2.0.*libmeowmenu\.so' 2>/dev/null || true
xfce4-panel -r >> "${LOG_FILE}" 2>&1 &
disown || true

echo "─────────────────────────────────────────"
echo "  Done. MeowMenu reloaded from ${PREFIX}."
echo ""
