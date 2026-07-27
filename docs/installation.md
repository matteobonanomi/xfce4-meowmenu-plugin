---
title: Installation
nav_order: 2
has_children: false
---

# Installation

Candidate packages are attached to the matching
[GitHub Release](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases).
Replace `<version>` with the public release version, such as `0.9.0-rc1`.

## Ubuntu 26.04

```bash
sudo apt install ./xfce4-meowmenu-plugin_<version>_ubuntu26.04_amd64.deb
```

## Debian 13

```bash
sudo apt install ./xfce4-meowmenu-plugin_<version>_debian13_amd64.deb
```

## Fedora 44

```bash
sudo dnf install ./xfce4-meowmenu-plugin-<version>-1.fc44.x86_64.rpm
```

## Arch Linux (AUR)

Arch and Arch-based distributions — including CachyOS, EndeavourOS, and
Manjaro — can install the AUR package with an AUR helper:

```bash
yay -S xfce4-meowmenu-plugin
```

The package
([xfce4-meowmenu-plugin](https://aur.archlinux.org/packages/xfce4-meowmenu-plugin))
is maintainer-published and distributed through the AUR. The repository recipe
is built, tested, linted, and smoke-installed before publication, but the AUR
package is published separately by the maintainer.

Uninstall with `yay -R xfce4-meowmenu-plugin`.

## Optional calculator engine

Ubuntu, Debian, and Fedora packages recommend `bc` for the optional
Calculator. It is never required. Arch package metadata has no Calculator
dependency or optional dependency.

To deliberately omit the recommendation, install a downloaded package with:

```bash
sudo apt install --no-install-recommends ./xfce4-meowmenu-plugin_*.deb
sudo dnf install --setopt=install_weak_deps=False ./xfce4-meowmenu-plugin-*.rpm
```

If selected `bc` is missing, Calculator shows **bc package required** and
ordinary search continues to work. Qalculate and GNOME Calculator engines are
also available when installed; none is a build or hard runtime dependency.

## After install

1. Run `xfce4-panel -r` to restart the panel.
2. Right-click the panel → **Add New Items** → **MeowMenu**.
3. Right-click the MeowMenu button → **Properties** to pick a preset.

## Remove the package

Remove the package with the appropriate command for your distribution:

- **Ubuntu / Debian**: `sudo apt purge xfce4-meowmenu-plugin`
- **Fedora**: `sudo dnf remove xfce4-meowmenu-plugin`
- **Source install**: `sudo ninja -C build uninstall`

Package removal keeps user configuration so a later reinstall can restore it.

## Full cleanup

To deliberately delete settings and saved custom presets after removing the
package:

```bash
rm -rf ~/.local/share/meowmenu/
rm -f ~/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml
```

This cleanup is optional and cannot be undone without a backup.

## Build from source

```bash
git clone https://github.com/matteobonanomi/xfce4-meowmenu-plugin.git
cd xfce4-meowmenu-plugin
meson setup build
meson compile -C build
sudo meson install -C build
```

Install the required build dependencies for your distribution first:

### Ubuntu 26.04 / Debian 13

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

### Fedora 44+

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

### Arch / Manjaro / EndeavourOS

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
