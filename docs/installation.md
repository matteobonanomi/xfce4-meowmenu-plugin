---
title: Installation
nav_order: 2
has_children: false
---

# Installation

Prebuilt packages for the four officially supported distributions are
attached to every [GitHub Release](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases/latest).
Replace `<version>` with the actual release tag (e.g. `0.4.0`).

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

## openSUSE Leap 15.6

```bash
sudo zypper install --allow-unsigned-rpm ./xfce4-meowmenu-plugin-<version>-1.suse15.6.x86_64.rpm
```

## After install

1. Run `xfce4-panel -r` to restart the panel.
2. Right-click the panel → **Add New Items** → **MeowMenu**.
3. Right-click the MeowMenu button → **Properties** to pick a preset.

## Uninstall

Remove the package with the appropriate command for your distribution:

- **Ubuntu / Debian**: `sudo apt purge xfce4-meowmenu-plugin`
- **Fedora**: `sudo dnf remove xfce4-meowmenu-plugin`
- **openSUSE**: `sudo zypper remove xfce4-meowmenu-plugin`
- **Source install**: `sudo ninja -C build uninstall`

Then remove any user configuration left behind:

```bash
rm -rf ~/.local/share/meowmenu/
rm -f ~/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml
```

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

### openSUSE Leap 15.6

Xfce devel packages live in the `X11:xfce` OBS project and their binary
names are not stable across reshuffles, so dependencies are resolved by
their `pkgconfig(...)` capability (the `.pc` filenames are stable):

```bash
sudo zypper addrepo --no-gpgcheck \
    https://download.opensuse.org/repositories/X11:/xfce/15.6/ xfce
sudo zypper refresh
sudo zypper install \
    gcc gcc-c++ make meson ninja pkgconfig \
    'pkgconfig(gtk+-3.0)' \
    'pkgconfig(glib-2.0)' \
    'pkgconfig(gio-2.0)' \
    'pkgconfig(garcon-1)' \
    'pkgconfig(libxfce4panel-2.0)' \
    'pkgconfig(libxfce4ui-2)' \
    'pkgconfig(libxfce4util-1.0)' \
    'pkgconfig(exo-2)' \
    'pkgconfig(libxfconf-0)' \
    'pkgconfig(accountsservice)' \
    'pkgconfig(gtk-layer-shell-0)' \
    gettext-tools
```

### openSUSE Tumbleweed

```bash
sudo zypper install -t pattern devel_C_C++
sudo zypper install \
    meson ninja pkg-config \
    gtk3-devel glib2-devel \
    garcon-devel \
    xfce4-panel-devel libxfce4ui-devel libxfce4util-devel \
    libexo-devel \
    xfconf-devel \
    accountsservice-devel gtk-layer-shell-devel \
    gettext-tools
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
