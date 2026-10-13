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
#   The script reads any prior install prefix from the build directory and
#   cleans exact MeowMenu payloads from that prefix, temporary test prefixes,
#   the conventional user-local prefix, and /usr/local. Files under /usr are
#   left to the package manager.
#
#   The panel is stopped before its MeowMenu slots are reset so no live plugin
#   can recreate settings during cleanup. xfconfd is never killed. Standalone
#   uninstallation restarts the panel from the remaining saved configuration.
#
# USAGE
#   ./dev/uninstall.sh [--no-restart] [BUILD_DIR]
#
#   --no-restart  Leave the panel stopped for install.sh to complete a clean
#                 replacement transaction.
#   BUILD_DIR     Meson build directory (default: ./build at the repo root).

set -euo pipefail

# This script lives under dev/; the repo root (and its default build dir) is its
# parent directory.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
RESTART_PANEL=true
BUILD_DIR=""

for arg in "$@"; do
    case "${arg}" in
        --no-restart) RESTART_PANEL=false ;;
        *)            BUILD_DIR="${arg}" ;;
    esac
done

BUILD_DIR="${BUILD_DIR:-${REPO}/build}"

step() { echo "  » $*"; }

# restore_panel_after_failure:
#
# Restarts a panel stopped by this script when any later cleanup step fails.
# Successful --no-restart use deliberately leaves it stopped for install.sh.
restore_panel_after_failure() {
    local status=$?

    if (( status != 0 )) && [[ "${PANEL_WAS_RUNNING:-false}" == true ]] \
            && ! pgrep -x xfce4-panel >/dev/null 2>&1; then
        setsid -f xfce4-panel --disable-wm-check >/dev/null 2>&1 || true
    fi
    return "${status}"
}

trap restore_panel_after_failure EXIT

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

# remove_dev_payload_under_root:
# @1: approved development prefix to inspect.
#
# Removes only exact MeowMenu payload paths, leaving unrelated files intact.
remove_dev_payload_under_root() {
    local root="$1"
    local candidate

    [[ -d "${root}" ]] || return 0
    while IFS= read -r -d '' candidate; do
        step "Remove development file ${candidate}"
        remove_exact_file "${candidate}"
        if [[ -e "${candidate}" || -L "${candidate}" ]]; then
            echo "FAILED: development file remains: ${candidate}"
            exit 1
        fi
    done < <(find "${root}" \
        \( -path '*/xfce4/panel/plugins/libmeowmenu.so' \
        -o -path '*/xfce4/panel/plugins/meowmenu.desktop' \
        -o -path '*/bin/xfce4-popup-meowmenu' \
        -o -path '*/share/man/man1/xfce4-popup-meowmenu.1' \
        -o -path '*/share/metainfo/io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml' \
        -o -path '*/share/metainfo/xfce4-meowmenu-plugin.appdata.xml' \
        -o -path '*/share/icons/hicolor/*/apps/org.xfce.panel.meowmenu.png' \
        -o -path '*/share/icons/hicolor/scalable/apps/org.xfce.panel.meowmenu.svg' \
        -o -path '*/share/locale/*/LC_MESSAGES/xfce4-meowmenu-plugin.mo' \) \
        \( -type f -o -type l \) -print0 2>/dev/null)
    while IFS= read -r -d '' candidate; do
        step "Remove development directory ${candidate}"
        remove_exact_tree "${candidate}"
        if [[ -e "${candidate}" || -L "${candidate}" ]]; then
            echo "FAILED: development directory remains: ${candidate}"
            exit 1
        fi
    done < <(find "${root}" -type d \
        \( -path '*/share/meowmenu' \
        -o -path '*/share/xfce4-meowmenu-plugin' \) \
        -prune -print0 2>/dev/null)
}

# ---------------------------------------------------------------------------
# Resolve the previous install prefix
# ---------------------------------------------------------------------------

if [[ -d "${BUILD_DIR}" ]]; then
    PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
        | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"
else
    echo "  Build directory '${BUILD_DIR}' not found, falling back to prefix=/usr/local"
    echo "  Known temporary MeowMenu prefixes are searched as well."
    PREFIX="/usr/local"
fi

echo ""
echo "MeowMenu dev-uninstall  (prefix: ${PREFIX})"
echo "─────────────────────────────────────────"

# Validate every cleanup dependency and user-owned path before stopping the
# panel or removing installed files. A failed preflight must leave the current
# development installation untouched.
if ! command -v xfconf-query >/dev/null 2>&1; then
    echo "FAILED: xfconf-query is required for a complete clean uninstall."
    exit 1
fi

USER_DATA_ROOT="${XDG_DATA_HOME:-${HOME}/.local/share}"
USER_MEOWMENU_DATA="${USER_DATA_ROOT%/}/meowmenu"
if [[ "${USER_DATA_ROOT}" != /* || "${USER_DATA_ROOT}" == "/" ]] \
        || is_protected_system_path "${USER_MEOWMENU_DATA}"; then
    echo "Refusing unsafe XDG_DATA_HOME '${USER_DATA_ROOT}'."
    exit 1
fi

USER_CONFIG_ROOT="${XDG_CONFIG_HOME:-${HOME}/.config}"
XFCONF_FILE="${USER_CONFIG_ROOT%/}/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml"
if [[ "${USER_CONFIG_ROOT}" != /* || "${USER_CONFIG_ROOT}" == "/" ]] \
        || is_protected_system_path "${XFCONF_FILE}"; then
    echo "Refusing unsafe XDG_CONFIG_HOME '${USER_CONFIG_ROOT}'."
    exit 1
fi

USER_CACHE_ROOT="${XDG_CACHE_HOME:-${HOME}/.cache}"
USER_CACHE_DIR="${USER_CACHE_ROOT%/}/xfce4/meowmenu"
if [[ "${USER_CACHE_ROOT}" != /* || "${USER_CACHE_ROOT}" == "/" ]] \
        || is_protected_system_path "${USER_CACHE_DIR}"; then
    echo "Refusing unsafe XDG_CACHE_HOME '${USER_CACHE_ROOT}'."
    exit 1
fi

# Package-managed files must be removed by their package manager so its
# database remains consistent. Refuse the transaction instead of allowing a
# /usr payload to coexist with and later reappear beneath the /usr/local build.
SYSTEM_PAYLOAD=()
for candidate in \
        /usr/bin/xfce4-popup-meowmenu \
        /usr/lib/xfce4/panel/plugins/libmeowmenu.so \
        /usr/lib/*/xfce4/panel/plugins/libmeowmenu.so \
        /usr/lib64/xfce4/panel/plugins/libmeowmenu.so \
        /usr/share/xfce4/panel/plugins/meowmenu.desktop; do
    [[ -e "${candidate}" || -L "${candidate}" ]] \
        && SYSTEM_PAYLOAD+=("${candidate}")
done
if (( ${#SYSTEM_PAYLOAD[@]} > 0 )); then
    echo "FAILED: a package-managed MeowMenu installation is present:"
    printf '  %s\n' "${SYSTEM_PAYLOAD[@]}"
    echo "Remove the xfce4-meowmenu-plugin package with the system package manager"
    echo "before using the maintainer clean-install or clean-uninstall scripts."
    exit 1
fi

# ---------------------------------------------------------------------------
# Stop the panel before changing payload or live configuration
# ---------------------------------------------------------------------------

PANEL_WAS_RUNNING=false
if pgrep -x xfce4-panel >/dev/null 2>&1; then
    PANEL_WAS_RUNNING=true
    step "Stop Xfce panel before complete cleanup"
    xfce4-panel --quit >/dev/null 2>&1 || true
    for _ in {1..30}; do
        pgrep -x xfce4-panel >/dev/null 2>&1 || break
        sleep 0.1
    done
fi

pkill -f 'wrapper-2.0.*libmeowmenu\.so' 2>/dev/null || true
if pgrep -x xfce4-panel >/dev/null 2>&1; then
    echo "FAILED: Xfce panel did not stop before MeowMenu cleanup."
    exit 1
fi

# ---------------------------------------------------------------------------
# Wipe user configuration
# ---------------------------------------------------------------------------

step "Remove user presets"
rm -rf -- "${USER_MEOWMENU_DATA}"
if [[ -e "${USER_MEOWMENU_DATA}" || -L "${USER_MEOWMENU_DATA}" ]]; then
    echo "FAILED: user preset directory remains: ${USER_MEOWMENU_DATA}"
    exit 1
fi

# Legacy on-disk channel file (pre-rename installs); harmless to remove if present.
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
step "Remove every MeowMenu panel slot and its Xfconf settings"
MEOWMENU_SLOTS=()
while IFS= read -r prop; do
    if [[ "${prop}" =~ ^/plugins/plugin-[0-9]+$ ]]; then
        value="$(xfconf-query --channel xfce4-panel --property "${prop}" \
            2>/dev/null || true)"
        [[ "${value}" == "meowmenu" ]] && MEOWMENU_SLOTS+=("${prop}")
    fi
done < <(xfconf-query --channel xfce4-panel --list 2>/dev/null || true)

# Remove each MeowMenu plugin ID from the panel arrays as well as deleting its
# settings subtree. Resetting only /plugins/plugin-N leaves a saved placeholder
# that Xfce tries to restore at the next login.
for panel_plugins in $(xfconf-query --channel xfce4-panel --list 2>/dev/null \
        | grep -E '^/panels/panel-[0-9]+/plugin-ids$' || true); do
    mapfile -t panel_ids < <(LC_ALL=C xfconf-query --channel xfce4-panel \
        --property "${panel_plugins}" 2>/dev/null | grep -E '^[0-9]+$' || true)
    kept_ids=()
    removed_id=false
    for panel_id in "${panel_ids[@]}"; do
        remove_id=false
        for prop in "${MEOWMENU_SLOTS[@]}"; do
            [[ "${prop}" == "/plugins/plugin-${panel_id}" ]] && remove_id=true
        done
        if [[ "${remove_id}" == true ]]; then
            removed_id=true
        else
            kept_ids+=("${panel_id}")
        fi
    done

    [[ "${removed_id}" == true ]] || continue
    if (( ${#kept_ids[@]} == 0 )); then
        xfconf-query --channel xfce4-panel --property "${panel_plugins}" \
            --reset
    else
        set_args=()
        for panel_id in "${kept_ids[@]}"; do
            set_args+=(--type int --set "${panel_id}")
        done
        xfconf-query --channel xfce4-panel --property "${panel_plugins}" \
            --force-array "${set_args[@]}"
    fi
done

for prop in "${MEOWMENU_SLOTS[@]}"; do
    step "  Reset ${prop} (recursive)"
    if ! xfconf-query --channel xfce4-panel --property "${prop}" \
            --reset --recursive; then
        echo "FAILED: could not reset ${prop}."
        exit 1
    fi
done

for panel_plugins in $(xfconf-query --channel xfce4-panel --list 2>/dev/null \
        | grep -E '^/panels/panel-[0-9]+/plugin-ids$' || true); do
    mapfile -t panel_ids < <(LC_ALL=C xfconf-query --channel xfce4-panel \
        --property "${panel_plugins}" 2>/dev/null | grep -E '^[0-9]+$' || true)
    for panel_id in "${panel_ids[@]}"; do
        for prop in "${MEOWMENU_SLOTS[@]}"; do
            if [[ "${prop}" == "/plugins/plugin-${panel_id}" ]]; then
                echo "FAILED: ${panel_plugins} still references ${prop}."
                exit 1
            fi
        done
    done
done

for prop in "${MEOWMENU_SLOTS[@]}"; do
    if xfconf-query --channel xfce4-panel --property "${prop}" \
            >/dev/null 2>&1; then
        echo "FAILED: MeowMenu panel slot remains: ${prop}"
        exit 1
    fi
done

# Remove exact payload names only after the saved panel layout no longer
# references MeowMenu. If filesystem cleanup then fails, the recovery restart
# cannot produce a missing-plugin placeholder.
KNOWN_ROOTS=("/usr/local")
for candidate in /tmp/meowmenu-*; do
    [[ -d "${candidate}" ]] && KNOWN_ROOTS+=("${candidate}")
done
if ! is_protected_system_path "${PREFIX}"; then
    KNOWN_ROOTS+=("${PREFIX}")
fi
KNOWN_ROOTS+=("${HOME}/.local")
KNOWN_ROOTS+=("${USER_DATA_ROOT}")
declare -A seen_roots=()
for root in "${KNOWN_ROOTS[@]}"; do
    root="$(realpath -m -- "${root}")"
    [[ -d "${root}" ]] || continue
    if [[ -n "${seen_roots[${root}]+x}" ]]; then
        continue
    fi
    seen_roots["${root}"]=1
    remove_dev_payload_under_root "${root}"
done

# Usage statistics live outside Xfconf and the preset directory. Remove the
# same dedicated cache directory as install.sh so ranking history is cleared.
if [[ -e "${USER_CACHE_DIR}" ]]; then
    step "Remove usage cache ${USER_CACHE_DIR}"
    rm -rf -- "${USER_CACHE_DIR}"
fi
if [[ -e "${USER_CACHE_DIR}" || -L "${USER_CACHE_DIR}" ]]; then
    echo "FAILED: usage cache remains: ${USER_CACHE_DIR}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Restore the panel for standalone uninstallation
# ---------------------------------------------------------------------------

if [[ "${RESTART_PANEL}" == true && "${PANEL_WAS_RUNNING}" == true ]]; then
    step "Restart Xfce panel without MeowMenu slots"
    setsid -f xfce4-panel --disable-wm-check >/dev/null 2>&1
    for _ in {1..20}; do
        pgrep -x xfce4-panel >/dev/null 2>&1 && break
        sleep 0.1
    done
    if ! pgrep -x xfce4-panel >/dev/null 2>&1; then
        echo "FAILED: Xfce panel did not restart after MeowMenu removal."
        exit 1
    fi
fi

echo "─────────────────────────────────────────"
echo "  Done. MeowMenu development payloads and user state were removed."
echo ""
trap - EXIT
