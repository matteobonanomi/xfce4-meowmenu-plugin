#!/usr/bin/env bash
# install.sh — build and clean-install MeowMenu for maintainer testing.
#
# BACKGROUND
#   Every run is a clean-install transaction representative of a normal manual
#   installation. It compiles first, completely removes prior development payloads,
#   panel slots, settings, presets, and usage history, then installs the current
#   payload under /usr/local. The panel is restarted without a MeowMenu instance;
#   add it through Xfce's Add New Items dialog to exercise a genuine first run.
#
# USAGE
#   ./dev/install.sh [--icons] [--reconfigure] [--clean-stale-dev] [BUILD_DIR]
#
#   --icons        Re-render icons/hi*-app-meowmenu.png from build-aux/art/meowmenu.svg
#                  before building.  Requires rsvg-convert (preferred) or inkscape.
#                  Without this flag icons are left as-is (the committed PNGs are used).
#   --reconfigure  Force meson setup --reconfigure even if meson.build and NEWS are
#                  unchanged.  Use after adding a new dependency or option.
#                  Normally the script detects whether reconfigure is needed automatically.
#   --clean-stale-dev
#                  Kept for compatibility with older development workflows.
#                  Exact stale development payloads are now cleaned
#                  automatically before installation.
#   BUILD_DIR      Meson build directory (default: ./build). The script
#                  reconfigures its prefix to /usr/local when necessary.
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

# restore_panel_after_failure:
#
# Restarts the panel if a clean uninstall succeeded but the replacement install
# later failed. This prevents a compiler, privilege, or payload error from
# leaving the maintainer's desktop without its panel.
restore_panel_after_failure() {
    local status=$?

    if (( status != 0 )) && [[ "${PANEL_NEEDS_RESTORE:-false}" == true ]] \
            && ! pgrep -x xfce4-panel >/dev/null 2>&1; then
        setsid -f xfce4-panel --disable-wm-check >> "${LOG_FILE}" 2>&1 || true
    fi
    return "${status}"
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
        --clean-stale-dev) : ;;
        *)             BUILD_DIR="${arg}" ;;
    esac
done

BUILD_DIR="${BUILD_DIR:-${REPO}/build}"
STANDARD_PREFIX="/usr/local"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory '${BUILD_DIR}' not found."
    echo "Run first:  meson setup ${BUILD_DIR} --prefix=/usr/local"
    exit 1
fi

# A maintainer clean install must survive logout and reboot and must use the
# same conventional paths as a manual system-wide source installation. Never
# inherit a temporary or user-local prefix from an earlier build experiment.
CURRENT_PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
    | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"
if [[ "$(realpath -m -- "${CURRENT_PREFIX}")" != "${STANDARD_PREFIX}" ]]; then
    run "Reconfigure durable install prefix (${CURRENT_PREFIX} -> ${STANDARD_PREFIX})" \
        meson setup --reconfigure "${BUILD_DIR}" "--prefix=${STANDARD_PREFIX}"
fi

PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
    | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"
if [[ "$(realpath -m -- "${PREFIX}")" != "${STANDARD_PREFIX}" ]]; then
    echo "FAILED: Meson prefix is '${PREFIX}', expected '${STANDARD_PREFIX}'."
    exit 1
fi

case ":${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:" in
    *:/usr/local/share:*) ;;
    *)
        echo "FAILED: XDG_DATA_DIRS does not include /usr/local/share."
        echo "The panel would not discover this standard installation after login."
        exit 1
        ;;
esac
INSTALLED_MODULE="$(meson introspect --installed "${BUILD_DIR}" \
    | python3 -c '
import json
import sys
installed = json.load(sys.stdin)
print(next(destination for source, destination in installed.items()
           if source.endswith("libmeowmenu.so")))
')"

# A sanitizer-instrumented shared module cannot be loaded safely by the
# unsanitized xfce4-panel wrapper: libasan must be initialized by the host
# executable before the module is opened. Refuse the install before replacing
# the working panel plugin when this build directory is configured for ASan or
# UBSan. Sanitizer builds remain suitable for tests and dedicated harnesses.
SANITIZERS="$(meson introspect --buildoptions "${BUILD_DIR}" \
    | python3 -c '
import json
import sys
option = next(o for o in json.load(sys.stdin) if o["name"] == "b_sanitize")
values = option["value"] if isinstance(option["value"], list) else [option["value"]]
print(",".join(str(v) for v in values if v != "none"))
')"
if [[ -n "${SANITIZERS}" ]]; then
    echo "Build directory '${BUILD_DIR}' uses b_sanitize=${SANITIZERS}."
    echo "Refusing to install an instrumented plugin into the running Xfce panel."
    echo "Reconfigure it first with: meson setup --reconfigure '${BUILD_DIR}' -Db_sanitize=none"
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

# Resolve destinations from Meson's install manifest rather than assuming the
# Debian multiarch libdir. This also covers Arch lib and Fedora lib64 layouts.
mapfile -t INSTALLED_PRESETS < <(
    meson introspect --installed "${BUILD_DIR}" \
        | python3 -c '
import json
import sys
installed = json.load(sys.stdin)
for source, destination in installed.items():
    if source.endswith(".meowpreset"):
        print(destination)
'
)
mapfile -t INSTALLED_PAYLOAD < <(
    meson introspect --installed "${BUILD_DIR}" \
        | python3 -c '
import json
import sys
installed = json.load(sys.stdin)
for destination in installed.values():
    print(destination)
'
)
if (( ${#INSTALLED_PRESETS[@]} == 0 )); then
    echo "FAILED: Meson install manifest contains no built-in presets."
    exit 1
fi

# Compile before cleanup so a build failure cannot destroy a working
# development installation. Once compilation succeeds, use the standalone
# uninstaller as the single source of truth for payload and user-state cleanup.
step "Completely remove prior MeowMenu installation and user state"
PANEL_NEEDS_RESTORE=false
trap restore_panel_after_failure EXIT
if ! "${SCRIPT_DIR}/uninstall.sh" --no-restart "${BUILD_DIR}" \
        >> "${LOG_FILE}" 2>&1; then
    echo "FAILED: Clean uninstall"
    echo "Last output (see ${LOG_FILE} for the full log):"
    tail -20 "${LOG_FILE}"
    exit 1
fi
PANEL_NEEDS_RESTORE=true

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

# A package-equivalent install must leave both the loadable module and every
# built-in preset from the current manifest present on disk.
if [[ ! -f "${INSTALLED_MODULE}" ]]; then
    echo "FAILED: installed module is missing: ${INSTALLED_MODULE}"
    exit 1
fi
echo "Installed module: ${INSTALLED_MODULE}"
if command -v readelf >/dev/null 2>&1; then
    INSTALLED_SANITIZER_DEPS="$(readelf -d "${INSTALLED_MODULE}" 2>/dev/null \
        | grep -E 'Shared library: \[lib(asan|ubsan)\.so' || true)"
    if [[ -n "${INSTALLED_SANITIZER_DEPS}" ]]; then
        echo "FAILED: installed module has sanitizer dependencies:"
        echo "${INSTALLED_SANITIZER_DEPS}"
        exit 1
    fi
fi
for preset in "${INSTALLED_PRESETS[@]}"; do
    if [[ ! -f "${preset}" ]]; then
        echo "FAILED: installed built-in preset is missing: ${preset}"
        exit 1
    fi
done
for payload in "${INSTALLED_PAYLOAD[@]}"; do
    if [[ ! -e "${payload}" ]]; then
        echo "FAILED: installed package payload is missing: ${payload}"
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# Reload panel
# ---------------------------------------------------------------------------
# The clean uninstall removed every MeowMenu panel slot. Start the panel with
# its normal login environment so discovery now and after reboot is identical.

# restart_panel:
#
# Starts the panel after the clean transaction and verifies that it remains
# alive. MeowMenu is intentionally not added automatically: normal package and
# source installations expose it through Xfce's Add New Items dialog.
restart_panel() {
    if ! pgrep -x xfce4-panel >/dev/null 2>&1; then
        step "Start Xfce panel from saved configuration"
        setsid -f xfce4-panel --disable-wm-check >> "${LOG_FILE}" 2>&1
    fi

    for _ in {1..20}; do
        pgrep -x xfce4-panel >/dev/null 2>&1 && return 0
        sleep 0.1
    done

    echo "FAILED: Xfce panel did not stay running"
    echo "Last output (see ${LOG_FILE} for the full log):"
    tail -20 "${LOG_FILE}"
    exit 1
}

restart_panel
PANEL_NEEDS_RESTORE=false
trap - EXIT

echo "─────────────────────────────────────────"
echo "  Done. MeowMenu clean-installed to ${PREFIX}."
echo "  Add Meow Menu through Xfce's Add New Items dialog for a fresh instance."
echo ""
