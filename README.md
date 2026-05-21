# MeowMenu: make your XFCE run!

[![Version](https://img.shields.io/github/v/release/matteobonanomi/xfce4-meowmenu-plugin)](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases)
[![CI](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/actions/workflows/ci.yml/badge.svg?branch=release)](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/actions/workflows/ci.yml)
[![License: GPL v2+](https://img.shields.io/badge/License-GPLv2%2B-blue.svg)](COPYING)
[![Language: C++](https://img.shields.io/badge/language-C%2B%2B-00599C.svg?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Build: Meson](https://img.shields.io/badge/build-Meson-A41E50.svg)](https://mesonbuild.com/)
[![Toolkit: GTK 3](https://img.shields.io/badge/toolkit-GTK%203-7FAE3D.svg?logo=gtk)](https://www.gtk.org/)
[![Desktop: Xfce 4.20](https://img.shields.io/badge/desktop-Xfce%204.20-2284F2.svg?logo=xfce&logoColor=white)](https://xfce.org/)
[![Upstream: Whisker Menu](https://img.shields.io/badge/fork%20of-Whisker%20Menu-orange.svg)](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin)

MeowMenu is a panel-plugin launcher for the Xfce desktop. It is a standalone
project that originated as a fork of [Whisker Menu](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin)
and keeps the familiar panel-launcher feel while bringing a cleaner modern
look and a more capable search bar. MeowMenu coexists with Whisker Menu and
does not replace it.

Built for Xubuntu 26.04 with Xfce 4.20.x.

---

## Installation / uninstallation

Prebuilt `.deb` / `.rpm` packages for the four officially supported
distributions — Ubuntu 26.04, Debian 13, Fedora 44, and openSUSE Leap 15.6 —
are built and smoke-tested in CI and attached to every
[GitHub Release](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases/latest).
On any other distribution (including Arch-based ones), build from source;
those builds are best-effort and are not exercised by CI.
Replace `<version>` with the actual release version (e.g. `0.4.0`).
After any install or uninstall, restart the panel with `xfce4-panel -r`;
then right-click the panel → **Add New Items** → **MeowMenu** to add it.

### Ubuntu 26.04

- **Suggested — package:**
  ```bash
  sudo apt install ./xfce4-meowmenu-plugin_<version>_ubuntu26.04_amd64.deb
  ```
  Uninstall: `sudo apt purge xfce4-meowmenu-plugin`
- **Optional — from source:** see [Build from source](#build-from-source) below.
  Uninstall: `sudo ninja -C build uninstall`

### Debian 13

- **Suggested — package:**
  ```bash
  sudo apt install ./xfce4-meowmenu-plugin_<version>_debian13_amd64.deb
  ```
  Uninstall: `sudo apt purge xfce4-meowmenu-plugin`
- **Optional — from source:** see [Build from source](#build-from-source) below.
  Uninstall: `sudo ninja -C build uninstall`

### Fedora 44

- **Suggested — package:**
  ```bash
  sudo dnf install ./xfce4-meowmenu-plugin-<version>-1.fc44.x86_64.rpm
  ```
  Uninstall: `sudo dnf remove xfce4-meowmenu-plugin`
- **Optional — from source:** see [Build from source](#build-from-source) below.
  Uninstall: `sudo ninja -C build uninstall`

### openSUSE Leap 15.6

- **Suggested — package:**
  ```bash
  sudo zypper install --allow-unsigned-rpm ./xfce4-meowmenu-plugin-<version>-1.suse15.6.x86_64.rpm
  ```
  Uninstall: `sudo zypper remove xfce4-meowmenu-plugin`
- **Optional — from source:** see [Build from source](#build-from-source) below.
  Uninstall: `sudo ninja -C build uninstall`

### Arch / other distros

Only from source — see [Build from source](#build-from-source) below.
Uninstall: `sudo ninja -C build uninstall`.

### Remove user configuration

Any uninstall leaves user data behind. After removing the MeowMenu button
from any panel, also clean:

```bash
# User-installed presets
rm -rf ~/.local/share/meowmenu/

# Xfconf settings (MeowMenu's own channel)
rm -f ~/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml
```

### Build from source

```bash
git clone https://github.com/matteobonanomi/xfce4-meowmenu-plugin.git
cd xfce4-meowmenu-plugin
meson setup build
meson compile -C build
sudo meson install -C build
```

Development dependencies per distro:

<details>
<summary><strong>Ubuntu 26.04 / Debian 13</strong></summary>

```bash
sudo apt update
sudo apt install \
    build-essential meson ninja-build pkg-config \
    libgtk-3-dev libglib2.0-dev \
    libgarcon-1-dev libgarcon-gtk3-1-dev \
    libxfce4panel-2.0-dev libxfce4ui-2-dev libxfce4util-dev \
    libexo-2-dev \
    libxfconf-0-dev \
    libaccountsservice-dev libgtk-layer-shell-dev \
    gettext
```

On Ubuntu 24.04 / Linux Mint 22 / LMDE 6, replace `libgarcon-1-dev` /
`libgarcon-gtk3-1-dev` with `libgarcon-1-0-dev` / `libgarcon-gtk3-1-0-dev`.

</details>

<details>
<summary><strong>Fedora 44+</strong></summary>

```bash
sudo dnf install \
    gcc gcc-c++ make meson ninja-build pkgconf-pkg-config \
    gtk3-devel glib2-devel \
    garcon-devel \
    xfce4-panel-devel libxfce4ui-devel libxfce4util-devel \
    exo-devel \
    xfconf-devel \
    accountsservice-devel gtk-layer-shell-devel \
    gettext
```

</details>

<details>
<summary><strong>openSUSE (Tumbleweed / Leap 15.6+)</strong></summary>

```bash
sudo zypper install -t pattern devel_C_C++
sudo zypper install \
    meson ninja pkg-config \
    gtk3-devel glib2-devel \
    garcon-devel \
    xfce4-panel-devel libxfce4ui-devel libxfce4util-devel \
    exo-devel \
    xfconf-devel \
    accountsservice-devel gtk-layer-shell-devel \
    gettext-tools
```

</details>

<details>
<summary><strong>Arch / Manjaro / EndeavourOS</strong></summary>

```bash
sudo pacman -S --needed \
    base-devel meson ninja pkgconf \
    gtk3 glib2 \
    garcon \
    xfce4-panel libxfce4ui libxfce4util \
    exo \
    xfconf \
    accountsservice gtk-layer-shell \
    gettext
```

</details>

---

## Presets

A preset is a snapshot of all visual and layout settings saved as a plain `.meowpreset`
file. Switching presets instantly reshapes the menu — size, corner radius, icon style,
sidebar position, and more — without touching individual options by hand.

MeowMenu ships three built-in presets:

| Preset | Description |
|---|---|
| **Classic** | Traditional Whisker Menu look. Compact docked window, apps as a list, sidebar on the right, no rounded corners. |
| **Modern** | Contemporary layout. Rounded corners, categories on the left, apps as an icon grid, search bar at the bottom, hover-to-switch enabled. |
| **Full Screen** | Launcher fills the entire screen. Large icon grid, categories on the left, ideal for touch or keyboard-first workflows. |

### User presets (drop-in location)

Drop any `.meowpreset` file here to make it appear in the **Properties → General → Preset**
selector after a panel restart:

```
~/.local/share/meowmenu/presets/
```

A file with the same `Id` as a built-in preset overrides the system version.

### Preset file format

```ini
[Preset]
Name=My Custom Preset
SchemaVersion=1
Id=my-custom           # optional; must match the filename stem
Description=My preset  # optional

[Settings]
layout-mode=docked
corner-radius=8
# ... any key from the Properties dialog
```

Unknown keys are silently ignored. Malformed files are skipped without crashing the panel.

---

## Properties window

Right-click the MeowMenu panel button → **Properties** to open the customization dialog.
It is organized into five sections:

| Section | What you configure |
|---|---|
| **General** | Preset selector, layout mode (docked / full screen), window size, corner radius, panel gap, and the panel button appearance (icon, label, style). |
| **User / Session** | Profile picture visibility, username display, and the session command buttons (lock, logout, suspend, etc.). |
| **Search Bar** | Search bar position (top / bottom), placeholder text, and search action shortcuts. |
| **App Grid** | Default view (list or icon grid), icon size, grid density, default category on open, and hover-to-switch-category behaviour. |
| **Sidebar** | Sidebar position (left / right), which category buttons are visible, and the commands bar position. |
| **Places** | Enable Places feature in MeowMenu: search for files, navigate folders, quick actions available|

---

## Configuration file (for power users)

All settings are stored in Xfconf — the Xfce configuration system. You can read and write
them without the GUI using `xfconf-query`:

```bash
# List all MeowMenu properties for a panel plugin instance
xfconf-query -c xfce4-panel -lv | grep meowmenu

# Change a single property, e.g. corner radius
xfconf-query -c xfce4-panel -p /plugins/<id>/corner-radius -s 8
```

Built-in presets are plain-text `.meowpreset` files installed to:

```
/usr/local/share/xfce4-meowmenu-plugin/
```

You can inspect them directly or copy one to `~/.local/share/meowmenu/presets/` and edit
it as your own custom preset.

---

## Localization

MeowMenu ships translations for **54 languages**. All upstream Whisker Menu
translations are preserved; MeowMenu-specific strings (Places, Presets,
FullScreen mode, Sidebar, Favourites, fuzzy-search controls, and more) have
been extended across all locales via LLM-assisted translation anchored to the
Italian catalogue.

Native speakers are warmly invited to review and improve any translation by
opening a Pull Request. The PO files live in `po/` — pick your language, edit
`msgstr` entries, run `msgfmt --check po/<lang>.po`, and submit.

---

## License and credits

MeowMenu is a fork of Whisker Menu and is distributed under the
[GNU General Public License v2](COPYING) (or any later version), the same license as
the original project.

Original Whisker Menu was created by **Graeme Gott** — thank you for building such a
solid foundation.

- Whisker Menu source: <https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin>
- Graeme Gott's site: <https://gottcode.org>
