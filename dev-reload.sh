#!/usr/bin/env bash
# Build, install and reload the MeowMenu plugin in a running Xfce session.
#
# Why this script exists:
#   - The plugin runs inside a separate `wrapper-2.0` process. `xfce4-panel -r`
#     does not always recycle that wrapper, so the old .so stays in memory.
#   - There must never be two copies of libmeowmenu.so on disk, otherwise the
#     panel can load the wrong one (XDG_DATA_DIRS user prefix wins over /usr).
#
# Usage: ./dev-reload.sh [meson build dir] (default: build)

set -euo pipefail

REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-${REPO}/build}"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Build directory '${BUILD_DIR}' not found. Run: meson setup ${BUILD_DIR} --prefix=/usr/local" >&2
    exit 1
fi

# 1. Compile
meson compile -C "${BUILD_DIR}"

# 2. Install (sudo only if prefix needs it)
PREFIX="$(meson introspect --buildoptions "${BUILD_DIR}" \
    | python3 -c 'import sys,json; print([o["value"] for o in json.load(sys.stdin) if o["name"]=="prefix"][0])')"

if [[ -w "${PREFIX}" ]]; then
    meson install -C "${BUILD_DIR}"
else
    sudo meson install -C "${BUILD_DIR}"
fi

# 3. Remove any stale user-prefix copy that would shadow the system one.
#    Xfce searches XDG_DATA_DIRS in user-first order; a leftover copy in
#    ~/.local will silently override the freshly-installed /usr/local one.
USER_SO="${HOME}/.local/lib/x86_64-linux-gnu/xfce4/panel/plugins/libmeowmenu.so"
if [[ "${PREFIX}" != "${HOME}/.local" && -e "${USER_SO}" ]]; then
    echo "Removing stale ${USER_SO} that would shadow ${PREFIX}"
    rm -f "${USER_SO}"
fi

# 4. Reset plugin config so every dev run starts from a clean Modern-preset state.
#    Delete the xfconf XML file, then kill xfconfd with SIGKILL so it cannot flush
#    its in-memory state back to disk.  xfconfd restarts automatically via D-Bus
#    activation; reading the (now absent) channel, it returns an empty property set
#    which the plugin's schema migration treats as a fresh install and applies the
#    Modern preset defaults (Settings::migrate_schema, PRESET_MODERN).
XFCONF_FILE="${HOME}/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml"
if [[ -f "${XFCONF_FILE}" ]]; then
    echo "Removing ${XFCONF_FILE} to reset to Modern-preset defaults."
    rm -f "${XFCONF_FILE}"
fi
pkill -9 xfconfd 2>/dev/null || true

# 5. Kill the plugin wrapper so it reloads the new .so. `xfce4-panel -r`
#    alone does NOT recycle wrappers reliably.
pkill -f 'wrapper-2.0.*libmeowmenu\.so' 2>/dev/null || true

# 6. Tell the panel to relaunch missing plugins.
xfce4-panel -r >/dev/null 2>&1 &
disown || true

echo "MeowMenu reloaded from ${PREFIX}."
