#!/usr/bin/env bash
# install.sh — build, install, and hot-reload MeowMenu in a running Xfce session.
#
# BACKGROUND
#   MeowMenu runs inside xfce4-panel's `wrapper-2.0` process, not inside the panel
#   itself.  Simply restarting the panel with `xfce4-panel -r` does NOT recycle the
#   wrapper, so the old libmeowmenu.so stays loaded.  This script kills the wrapper
#   explicitly so the panel re-spawns it against the freshly installed library.
#
#   Every run is a clean-install transaction: prior MeowMenu user data and settings
#   are removed, the complete current payload is installed into the Meson-configured
#   prefix, and the restarted plugin performs its normal first-run initialization.
#   Configure Meson with a system prefix to exercise system paths; a user-local
#   prefix exercises the same payload and first-run behavior without root access.
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
#                  Remove exact MeowMenu modules found in known development
#                  prefixes before installing. Without this flag, the script
#                  stops when another module could shadow the selected payload.
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
CLEAN_STALE_DEV=false
BUILD_DIR=""

for arg in "$@"; do
    case "${arg}" in
        --icons)       REGEN_ICONS=true ;;
        --reconfigure) FORCE_RECONFIGURE=true ;;
        --clean-stale-dev) CLEAN_STALE_DEV=true ;;
        *)             BUILD_DIR="${arg}" ;;
    esac
done

BUILD_DIR="${BUILD_DIR:-${REPO}/build}"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory '${BUILD_DIR}' not found."
    echo "Run first:  meson setup ${BUILD_DIR} --prefix=/usr/local"
    exit 1
fi

# Resolve the selected install location before compiling so a conflicting
# source payload is found before the running panel can load it.
PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
    | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"
INSTALLED_MODULE="$(meson introspect --installed "${BUILD_DIR}" \
    | python3 -c '
import json
import sys
installed = json.load(sys.stdin)
print(next(destination for source, destination in installed.items()
           if source.endswith("libmeowmenu.so")))
')"

same_path() {
    [[ "$(realpath -m -- "$1")" == "$(realpath -m -- "$2")" ]]
}

# /usr is reserved for package managers. /usr/local remains a supported
# development prefix and is intentionally excluded from this protection.
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

KNOWN_MODULES=()
KNOWN_ROOTS=()

# add_known_root:
# @1: candidate development prefix to inspect.
#
# Adds an existing, non-package-managed prefix once to the audit set.
add_known_root() {
    local root="$1"
    local existing

    root="$(realpath -m -- "${root}")"
    [[ -d "${root}" ]] || return 0
    is_protected_system_path "${root}" && return 0
    for existing in "${KNOWN_ROOTS[@]}"; do
        same_path "${root}" "${existing}" && return 0
    done
    KNOWN_ROOTS+=("${root}")
}

# collect_known_modules:
#
# Finds exact MeowMenu module paths below the approved development roots while
# excluding the module selected by the current Meson install manifest.
collect_known_modules() {
    local root candidate existing

    for root in "${KNOWN_ROOTS[@]}"; do
        while IFS= read -r -d '' candidate; do
            same_path "${candidate}" "${INSTALLED_MODULE}" && continue
            for existing in "${KNOWN_MODULES[@]}"; do
                same_path "${candidate}" "${existing}" && continue 2
            done
            KNOWN_MODULES+=("${candidate}")
        done < <(find "${root}" \
            -path '*/xfce4/panel/plugins/libmeowmenu.so' \
            \( -type f -o -type l \) -print0 2>/dev/null)
    done
}

# describe_module:
# @1: exact module path to inspect.
#
# Reports basic file and dynamic-loader metadata without loading the module.
describe_module() {
    local candidate="$1"
    local sanitizer_deps=""

    echo "  ${candidate}"
    if command -v file >/dev/null 2>&1; then
        echo "    $(file -b -- "${candidate}")"
    fi
    if command -v readelf >/dev/null 2>&1; then
        sanitizer_deps="$(readelf -d "${candidate}" 2>/dev/null \
            | grep -E 'Shared library: \[lib(asan|ubsan)\.so' || true)"
        if [[ -n "${sanitizer_deps}" ]]; then
            echo "    sanitizer dependencies: ${sanitizer_deps//$'\n'/, }"
        else
            echo "    sanitizer dependencies: none detected"
        fi
    fi
}

# remove_stale_module:
# @1: exact stale development module path.
#
# Removes one approved path and fails if the payload remains afterward.
remove_stale_module() {
    local candidate="$1"

    step "Remove stale ${candidate}"
    if ! rm -f -- "${candidate}" 2>/dev/null; then
        sudo rm -f -- "${candidate}"
    fi
    if [[ -e "${candidate}" || -L "${candidate}" ]]; then
        echo "FAILED: stale module remains: ${candidate}"
        exit 1
    fi
}

add_known_root "${PREFIX}"
add_known_root "${HOME}/.local"
add_known_root "/usr/local"
collect_known_modules

if (( ${#KNOWN_MODULES[@]} > 0 )); then
    echo ""
    echo "Potential conflicting MeowMenu development modules found:"
    for candidate in "${KNOWN_MODULES[@]}"; do
        describe_module "${candidate}"
    done
    if [[ "${CLEAN_STALE_DEV}" != true ]]; then
        echo ""
        echo "Refusing to install while another module may shadow ${INSTALLED_MODULE}."
        echo "Run ./dev/uninstall.sh <old-build-directory> when known, or rerun with:"
        echo "  ./dev/install.sh --clean-stale-dev ${BUILD_DIR}"
        exit 1
    fi
    echo ""
    echo "Removing only the exact known development modules above."
    for candidate in "${KNOWN_MODULES[@]}"; do
        remove_stale_module "${candidate}"
    done
fi

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

# Do not call uninstall.sh here. It deliberately removes the active plugin and
# kills its wrapper, which makes Xfce see a configured slot with no module and
# show the "remove plugin" prompt before the new install can complete. Meson
# overwrites the installed files in place; the wrapper is recycled only after
# the new files are present.

# User preset drop-ins and a user-local package prefix intentionally share the
# XDG data directory. Remove the old tree before installation so Meson restores
# the packaged built-ins afterward instead of deleting the new payload.
USER_DATA_ROOT="${XDG_DATA_HOME:-${HOME}/.local/share}"
USER_MEOWMENU_DATA="${USER_DATA_ROOT%/}/meowmenu"
if [[ "${USER_DATA_ROOT}" != /* || "${USER_DATA_ROOT}" == "/" ]] \
        || is_protected_system_path "${USER_MEOWMENU_DATA}"; then
    echo "Refusing unsafe XDG_DATA_HOME '${USER_DATA_ROOT}'."
    exit 1
fi
step "Remove prior MeowMenu user data"
rm -rf -- "${USER_MEOWMENU_DATA}"

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
# Reset user state after the replacement is installed
# ---------------------------------------------------------------------------
# Keep the panel slot and its live wrapper valid throughout installation.
# Stop the panel only after the new module is in place. Resetting a live plugin
# makes its settings callbacks repopulate keys while they are being removed,
# producing a hybrid "Custom" profile instead of the fresh Modern preset.
# Keeping the registered panel slot and installed module intact avoids the
# missing-plugin prompt while the short reset is in progress.

# stop_panel_for_reset:
#
# Stops the panel and its out-of-process plugin wrapper before Xfconf mutation.
# Returns only when no old MeowMenu instance can write settings back into the
# channel; restart_panel restores the saved panel configuration afterward.
stop_panel_for_reset() {
    if pgrep -x xfce4-panel >/dev/null 2>&1; then
        step "Stop Xfce panel for clean state reset"
        xfce4-panel --quit >> "${LOG_FILE}" 2>&1 || true
        for _ in {1..20}; do
            pgrep -x xfce4-panel >/dev/null 2>&1 || break
            sleep 0.1
        done
    fi

    pkill -f 'wrapper-2.0.*libmeowmenu\.so' 2>/dev/null || true
    if pgrep -x xfce4-panel >/dev/null 2>&1; then
        echo "FAILED: Xfce panel did not stop before the state reset"
        echo "See ${LOG_FILE} for command output."
        exit 1
    fi
}

stop_panel_for_reset

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
USER_CACHE_DIR="${USER_CACHE_ROOT%/}/xfce4/meowmenu"
if [[ "${USER_CACHE_ROOT}" != /* || "${USER_CACHE_ROOT}" == "/" ]] \
        || is_protected_system_path "${USER_CACHE_DIR}"; then
    echo "Refusing unsafe XDG_CACHE_HOME '${USER_CACHE_ROOT}'."
    exit 1
fi
if [[ -e "${USER_CACHE_DIR}" ]]; then
    step "Remove usage cache ${USER_CACHE_DIR}"
    rm -rf -- "${USER_CACHE_DIR}"
fi

# ---------------------------------------------------------------------------
# Reload panel
# ---------------------------------------------------------------------------
# Start the panel from its preserved slot configuration. The reset above keeps
# the slot root registered while removing only MeowMenu's child settings.

# restart_panel:
#
# Recycle a running panel when possible, then fall back to a real launch if
# that process disappears. The final check prevents install.sh from reporting
# success while the saved panel configuration is not visible on screen.
restart_panel() {
    local panel_pid=""
    local stable_checks=0

    panel_pid="$(pgrep -xo xfce4-panel || true)"
    if [[ -n "${panel_pid}" ]]; then
        step "Restart Xfce panel"
        xfce4-panel -r >> "${LOG_FILE}" 2>&1 &
        disown || true
        sleep 1
    fi

    if ! pgrep -x xfce4-panel >/dev/null 2>&1; then
        step "Start Xfce panel from saved configuration"
        setsid -f xfce4-panel --disable-wm-check >> "${LOG_FILE}" 2>&1
    fi

    # The launcher process can briefly exist before it daemonizes or fails.
    # Require both processes to survive several checks so a transient PID
    # cannot turn a failed reload into a reported success.
    for _ in {1..50}; do
        if pgrep -x xfce4-panel >/dev/null 2>&1 \
                && pgrep -f 'wrapper-2\.0 .*libmeowmenu\.so' \
                    >/dev/null 2>&1; then
            stable_checks=$((stable_checks + 1))
            if (( stable_checks >= 5 )); then
                return 0
            fi
        else
            stable_checks=0
        fi
        sleep 0.2
    done

    echo "FAILED: Xfce panel and MeowMenu wrapper did not stay running"
    echo "Last output (see ${LOG_FILE} for the full log):"
    tail -20 "${LOG_FILE}"
    exit 1
}

restart_panel

echo "─────────────────────────────────────────"
echo "  Done. MeowMenu reloaded from ${PREFIX}."
echo ""
