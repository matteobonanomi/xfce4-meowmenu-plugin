---
title: Installation
nav_order: 2
has_children: false
---

# Installation

Release packages are attached to the selected
[GitHub Release](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases).
Each release contains the three native packages below, one matching source
archive, and `SHA256SUMS`. Replace `<version>` with the version shown on that
release.

Download all five files into one directory and verify the four payloads before
installing:

```bash
sha256sum -c SHA256SUMS
```

The manifest must report one successful check for each of the three packages
and `xfce4-meowmenu-plugin-<version>.tar.gz`. It does not contain a checksum
entry for itself.

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
is published manually by the maintainer and distributed through the AUR. The
repository recipe is built, tested, linted, and smoke-installed as a mandatory
release check, but no Arch binary is attached to the GitHub Release and the
workflow does not publish AUR metadata.

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

## One-time reset when upgrading pre-1.0 installations

The first start of this version resets each existing pre-1.0 MeowMenu panel
instance to the **Modern** preset. The reset happens once per instance because
the older experimental layout settings and saved custom presets are not
compatible with the supported composition.

The reset removes that instance's MeowMenu preferences, favourites, recent
history, search customizations, and GUI-managed custom presets. It preserves
the panel item and its position, other panel plugins, other MeowMenu instances
until their own first start, and `.meowpreset` files stored outside Xfconf.

Before upgrading, stop the panel and keep a reference copy of its configuration:

```bash
xfce4-panel --quit
cp ~/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml \
   ~/xfce4-panel-before-meowmenu-upgrade.xml
cp -a ~/.local/share/meowmenu ~/meowmenu-preset-files-backup 2>/dev/null || true
xfce4-panel &
```

The XML copy is for reference or whole-profile recovery; do not copy obsolete
MeowMenu properties back into a running panel. Reconfigure the supported
options in **Properties**, or import a preset created with the current layout
model. An older preset containing retired layout fields is rejected with an
incompatible-preset message and changes nothing.

## Remove the package

Remove the package with the appropriate command for your distribution:

- **Ubuntu / Debian**: `sudo apt purge xfce4-meowmenu-plugin`
- **Fedora**: `sudo dnf remove xfce4-meowmenu-plugin`
- **Source install**: `sudo ninja -C build uninstall`

Package removal keeps user configuration so a later reinstall can restore it.

## Clean one MeowMenu instance

To clear one instance without deleting its panel registration or sibling
plugins, first find its numeric base:

```bash
xfconf-query -c xfce4-panel -lv | grep '/plugins/meowmenu-'
```

Then replace `7` below with that exact instance number:

```bash
instance_base=/plugins/meowmenu-7
xfconf-query -c xfce4-panel -l \
  | awk -v prefix="$instance_base/" 'index($0, prefix) == 1' \
  | while IFS= read -r property; do
      xfconf-query -c xfce4-panel -p "$property" -r
    done
xfce4-panel -r
```

This removes only descendants of the selected instance. On restart it receives
Modern defaults. Exported `.meowpreset` files are deliberately left alone.

## Build from source

```bash
git clone https://github.com/matteobonanomi/xfce4-meowmenu-plugin.git
cd xfce4-meowmenu-plugin
meson setup build
meson compile -C build
sudo meson install -C build
```

Install the required core build dependencies for the exact distribution
release first. Optional integrations are listed separately so omitting one
never blocks the core launcher.

### Ubuntu 26.04

```bash
sudo apt update
sudo apt install \
    build-essential meson ninja-build pkg-config \
    libgtk-3-dev libglib2.0-dev \
    libgarcon-1-dev libgarcon-gtk3-1-dev \
    libxfce4panel-2.0-dev libxfce4ui-2-dev libxfce4util-dev \
    libexo-2-dev \
    libxfconf-0-dev \
    gettext
```

Optional integrations on Ubuntu 26.04:

```bash
sudo apt install libaccountsservice-dev libgtk-layer-shell-dev
```

### Debian 13

```bash
sudo apt update
sudo apt install \
    build-essential meson ninja-build pkg-config \
    libgtk-3-dev libglib2.0-dev \
    libgarcon-1-dev libgarcon-gtk3-1-dev \
    libxfce4panel-2.0-dev libxfce4ui-2-dev libxfce4util-dev \
    libexo-2-dev \
    libxfconf-0-dev \
    gettext
```

Optional integrations on Debian 13:

```bash
sudo apt install libaccountsservice-dev libgtk-layer-shell-dev
```

### Fedora 44+

```bash
sudo dnf install \
    gcc gcc-c++ make meson ninja-build pkgconf-pkg-config \
    gtk3-devel glib2-devel \
    garcon-devel \
    xfce4-panel-devel libxfce4ui-devel libxfce4util-devel \
    exo-devel \
    xfconf-devel \
    gettext
```

Optional integrations on Fedora 44:

```bash
sudo dnf install accountsservice-devel gtk-layer-shell-devel
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
    gettext
```

Optional integrations on Arch:

```bash
sudo pacman -S --needed accountsservice gtk-layer-shell
```

## Xfce dependency boundary

Xfce 4.16, 4.18, and 4.20 builds require Exo development files and helper
programs. The commands above therefore retain `libexo-2-dev`, `exo-devel`, or
`exo` for their current repositories. Starting with libxfce4ui 4.21, the
required chooser, opener, and launcher editor are supplied by libxfce4ui; the
replacement compatibility build is tested with Exo absent.

Do not remove Exo from a distribution recipe merely because the replacement
source stack passes. A concrete target may remove it only after that target's
repository crosses the boundary and its package build, linkage inspection,
installed actions, and upgraded stored actions all pass without Exo.

`gtk-layer-shell` is optional. If a named release does not provide a suitable
version, leave it out: MeowMenu still builds and uses its normal
session-compatible positioning fallback.
