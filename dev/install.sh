#!/usr/bin/env bash
# install.sh — build, install, and hot-reload MeowMenu in a running Xfce session.
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
#   ./dev/install.sh [--icons] [--reconfigure] [BUILD_DIR]
#
#   --icons        Re-render icons/hi*-app-meowmenu.png from build-aux/art/meowmenu.svg
#                  before building.  Requires rsvg-convert (preferred) or inkscape.
#                  Without this flag icons are left as-is (the committed PNGs are used).
#   --reconfigure  Force meson setup --reconfigure even if meson.build and NEWS are
#                  unchanged.  Use after adding a new dependency or option.
#                  Normally the script detects whether reconfigure is needed automatically.
#   BUILD_DIR      Meson build directory (default: ./build).
#                  Must already exist; run `meson setup build` once first.
#
# LOGS
#   Verbose output from meson and the compiler is redirected to
#   dev/.logs/dev-install.log (rotated on each run). Check that file if a step
#   fails.

set -euo pipefail

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# This script lives under dev/; the repo root is its parent. REPO must point at
# the root so the meson build, build-aux helpers, and icons resolve correctly;
# logs stay alongside the script under dev/.logs/.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
LOG_DIR="${SCRIPT_DIR}/.logs"
LOG_FILE="${LOG_DIR}/dev-install.log"

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
FORCE_RECONFIGURE=false
BUILD_DIR=""

for arg in "$@"; do
    case "${arg}" in
        --icons)       REGEN_ICONS=true ;;
        --reconfigure) FORCE_RECONFIGURE=true ;;
        *)             BUILD_DIR="${arg}" ;;
    esac
done

BUILD_DIR="${BUILD_DIR:-${REPO}/build}"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory '${BUILD_DIR}' not found."
    echo "Run first:  meson setup ${BUILD_DIR} --prefix=/usr/local"
    exit 1
fi

echo ""
echo "MeowMenu dev-install  (log → ${LOG_FILE})"
echo "─────────────────────────────────────────"

# ---------------------------------------------------------------------------
# Optional: re-render icons from master SVG
# ---------------------------------------------------------------------------
# Without --icons the committed PNGs are used as-is.  Pass --icons when you
# have replaced build-aux/art/meowmenu.svg and want the hi*-app-meowmenu.png files
# to be regenerated before the build.  Requires rsvg-convert or inkscape.

if [[ "${REGEN_ICONS}" == true ]]; then
    run "Regen icons from build-aux/art/meowmenu.svg" \
        python3 "${REPO}/build-aux/regen-icons.py" \
            --input  "${REPO}/build-aux/art/meowmenu.svg" \
            --output-dir "${REPO}/icons"
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
# Reconfigure only when top-level Meson inputs or NEWS changed since the last
# configure, or when --reconfigure is passed explicitly. Meson itself catches
# changes to subdirectory meson.build files when compilation starts.

STAMP="${BUILD_DIR}/build.ninja"

needs_reconfigure() {
    [[ "${FORCE_RECONFIGURE}" == true ]] && return 0
    [[ ! -f "${STAMP}" ]] && return 0
    # Reconfigure if meson.build, meson_options.txt, or NEWS are newer than the
    # generated backend.
    local f
    for f in "${REPO}/meson.build" "${REPO}/meson_options.txt" "${REPO}/NEWS"; do
        [[ -f "${f}" && "${f}" -nt "${STAMP}" ]] && return 0
    done
    return 1
}

if needs_reconfigure; then
    run "Reconfigure (pick up NEWS version + meson changes)" \
        meson setup --reconfigure "${BUILD_DIR}"
else
    step "Reconfigure skipped (meson.build and NEWS unchanged)"
fi

# A truncated .ninja_deps file makes Ninja rebuild every C++ target on every
# invocation while repeatedly reporting that it is recovering. Probe only the
# dependency database and discard it when Ninja confirms that exact corruption;
# the compile below recreates the cache from compiler-generated depfiles.
NINJA_DEPS="${BUILD_DIR}/.ninja_deps"
if [[ -f "${NINJA_DEPS}" ]]; then
    NINJA_DEPS_DIAGNOSTIC="$(ninja -C "${BUILD_DIR}" -t deps 2>&1 >/dev/null || true)"
    if [[ "${NINJA_DEPS_DIAGNOSTIC}" == *"premature end of file; recovering"* ]]; then
        step "Reset corrupt Ninja dependency cache"
        rm -f -- "${NINJA_DEPS}"
    fi
fi

run "Compile" \
    meson compile -C "${BUILD_DIR}" -j"$(nproc)"

# ---------------------------------------------------------------------------
# Resolve the install prefix
# ---------------------------------------------------------------------------

PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
    | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"

# Do not call uninstall.sh here. It deliberately removes the active plugin and
# kills its wrapper, which makes Xfce see a configured slot with no module and
# show the "remove plugin" prompt before the new install can complete. Meson
# overwrites the installed files in place; the wrapper is recycled only after
# the new files are present.

# If a previous install left a copy in ~/.local, it silently wins over the
# system prefix because XDG_DATA_DIRS is searched user-first. Keep it until
# after the new system copy is installed so a failed install remains usable.
USER_SO="${HOME}/.local/lib/x86_64-linux-gnu/xfce4/panel/plugins/libmeowmenu.so"

# The explicit compile above is authoritative. --no-rebuild prevents Meson
# from launching Ninja again as part of installation.
if [[ -w "${PREFIX}" ]]; then
    run "Install to ${PREFIX}" \
        meson install --no-rebuild -C "${BUILD_DIR}"
else
    step "Install to ${PREFIX} (sudo)"
    if ! sudo meson install --no-rebuild -C "${BUILD_DIR}" >> "${LOG_FILE}" 2>&1; then
        echo "FAILED: Install"; tail -20 "${LOG_FILE}"; exit 1
    fi
    # Meson may create its install log as root even when rebuilding is disabled.
    # Restore ownership only for that metadata file, leaving build artifacts
    # untouched.
    INSTALL_LOG="${BUILD_DIR}/meson-logs/install-log.txt"
    if [[ -e "${INSTALL_LOG}" ]]; then
        sudo chown "$(id -un):$(id -gn)" "${INSTALL_LOG}"
    fi
fi

# ---------------------------------------------------------------------------
# Reset user state after the replacement is installed
# ---------------------------------------------------------------------------
# Keep the panel slot and its live wrapper valid throughout installation.
# Resetting state here still gives every development run a clean configuration
# while avoiding the transient missing-plugin state caused by uninstall first.

if [[ "${PREFIX}" != "${HOME}/.local" && -e "${USER_SO}" ]]; then
    step "Remove stale ${USER_SO}"
    rm -f "${USER_SO}"
fi

step "Remove user presets"
rm -rf "${HOME}/.local/share/meowmenu/" 2>/dev/null || true

# Legacy on-disk channel file (pre-rename installs); harmless to remove if
# present. Live settings are reset through xfconfd below.
XFCONF_FILE="${HOME}/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml"
if [[ -f "${XFCONF_FILE}" ]]; then
    step "Remove legacy Xfconf channel file (meowmenu.xml)"
    rm -f "${XFCONF_FILE}"
fi

if command -v xfconf-query >/dev/null 2>&1; then
    step "Reset MeowMenu Xfconf state (xfce4-panel channel + /initialized marker)"
    while IFS= read -r prop; do
        if [[ "${prop}" =~ ^/plugins/plugin-[0-9]+$ ]]; then
            value="$(xfconf-query --channel xfce4-panel --property "${prop}" 2>/dev/null || true)"
            if [[ "${value}" == "meowmenu" ]]; then
                # The root property is the panel's slot registration. Reset
                # only its direct children so the panel keeps this plugin ID.
                declare -A child_roots=()
                while IFS= read -r child; do
                    [[ "${child}" == "${prop}/"* ]] || continue
                    child_name="${child#${prop}/}"
                    child_name="${child_name%%/*}"
                    child_roots["${prop}/${child_name}"]=1
                done < <(xfconf-query --channel xfce4-panel --list 2>/dev/null || true)

                for child in "${!child_roots[@]}"; do
                    step "  Reset ${child} (recursive)"
                    xfconf-query --channel xfce4-panel --property "${child}" \
                        --reset --recursive 2>/dev/null || true
                done
            fi
        fi
    done < <(xfconf-query --channel xfce4-panel --list 2>/dev/null || true)
else
    step "xfconf-query not found — skipping channel reset"
fi

# Usage statistics live outside Xfconf and the preset directory. Remove their
# dedicated cache directory so the new plugin instance starts without ranking
# history as well as without settings.
USER_CACHE_ROOT="${XDG_CACHE_HOME:-${HOME}/.cache}"
USER_CACHE_DIR="${USER_CACHE_ROOT}/xfce4/meowmenu"
if [[ -e "${USER_CACHE_DIR}" ]]; then
    step "Remove usage cache ${USER_CACHE_DIR}"
    rm -rf -- "${USER_CACHE_DIR}"
fi

# ---------------------------------------------------------------------------
# Reload panel
# ---------------------------------------------------------------------------
# Kill the wrapper process that holds the old .so in memory, then ask the
# panel to relaunch any missing plugins. If the panel process is already gone,
# --restart is only a request to a nonexistent instance, so start it directly
# from the preserved Xfconf configuration instead.

pkill -f 'wrapper-2.0.*libmeowmenu\.so' 2>/dev/null || true

# restart_panel:
#
# Recycle a running panel when possible, then fall back to a real launch if
# that process disappears. The final check prevents install.sh from reporting
# success while the saved panel configuration is not visible on screen.
restart_panel() {
    local panel_pid=""

    panel_pid="$(pgrep -xo xfce4-panel || true)"
    if [[ -n "${panel_pid}" ]]; then
        step "Restart Xfce panel"
        xfce4-panel -r >> "${LOG_FILE}" 2>&1 &
        disown || true
        sleep 1
    fi

    if ! pgrep -x xfce4-panel >/dev/null 2>&1; then
        step "Start Xfce panel from saved configuration"
        xfce4-panel --disable-wm-check >> "${LOG_FILE}" 2>&1 &
        disown || true
    fi

    for _ in {1..20}; do
        pgrep -x xfce4-panel >/dev/null 2>&1 && return 0
        sleep 0.25
    done

    echo "FAILED: Xfce panel did not stay running"
    echo "Last output (see ${LOG_FILE} for the full log):"
    tail -20 "${LOG_FILE}"
    exit 1
}

restart_panel

echo "─────────────────────────────────────────"
echo "  Done. MeowMenu reloaded from ${PREFIX}."
echo ""
