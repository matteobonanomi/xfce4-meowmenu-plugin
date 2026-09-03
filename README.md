# MeowMenu: a modern and configurable XFCE application launcher

<p align="center">
  <a href="https://www.xfce.org/"><img src="https://img.shields.io/badge/Xfce-4.16%2B-2284F2?logo=xfce&amp;logoColor=white" alt="Xfce 4.16 or later"></a>
  <a href="https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases"><img src="https://img.shields.io/github/v/release/matteobonanomi/xfce4-meowmenu-plugin?include_prereleases" alt="Version"></a>
  <a href="https://github.com/matteobonanomi/xfce4-meowmenu-plugin/actions/workflows/ci.yml"><img src="https://github.com/matteobonanomi/xfce4-meowmenu-plugin/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI"></a>
  <a href="COPYING"><img src="https://img.shields.io/badge/License-GPLv2%2B-blue.svg" alt="License: GPL v2+"></a>
  <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/language-C%2B%2B-00599C.svg" alt="Language: C++"></a>
  <a href="https://www.gtk.org/"><img src="https://img.shields.io/badge/toolkit-GTK%203-7FAE3D.svg" alt="Toolkit: GTK 3"></a>
</p>

![MeowMenu demo](docs/assets/images/demo.gif)

<p align="center"><strong><a href="https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases">Releases</a></strong> | <strong><a href="docs/index.md">Documentation</a></strong> | <strong><a href="https://aur.archlinux.org/packages/xfce4-meowmenu-plugin">AUR</a></strong></p>

MeowMenu is a native Xfce panel launcher and a standalone fork of
[Whisker Menu](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin).
It adds saved presets, Places integration, flexible docked, centered, and
full-screen layouts, drag-and-drop actions, and an optional inline Calculator
while keeping a familiar launcher workflow.

It is fully keyboard-driven: Tab switches Applications and Places, ordinary
arrows follow the visible arrangement without wrapping, and Ctrl+Tab is a
deliberate no-op. See
[keyboard navigation](docs/keyboard-navigation.md).

## Install

Each GitHub Release provides native packages for Ubuntu 26.04, Debian 13, and
Fedora 44, plus the matching source archive and `SHA256SUMS` manifest. Arch
packaging is validated separately and is not attached as a binary; Arch users
can use the maintainer-published
[AUR package](https://aur.archlinux.org/packages/xfce4-meowmenu-plugin).
Follow the [installation guide](docs/installation.md) for package commands,
checksum verification, dependencies, source builds, removal, and full cleanup.

After installation, restart the panel with `xfce4-panel -r`, then add
**MeowMenu** through **Add New Items**.

## Release channels

Before final 1.0.0, 0.x releases are experimental feature releases. An
available release candidate (an `-rcN` version) is the more stable channel for
testing and feedback. Choose the newest 0.x release when it is newer than the
latest RC and you want the newest features; choose the RC when stability while
testing matters more. Both are standard public GitHub releases.

Before final 1.0.0, including every 0.x release and RC, compatibility and
configuration preservation are not guaranteed in any way. From final 1.0.0
onward, configuration preservation is guaranteed. Keep a backup of your panel
configuration before testing a pre-1.0 release.

## Support and compatibility

MeowMenu supports Xfce 4.16 through 4.21, with Xfce 4.20 as the primary
quality target. Package availability is listed under [Install](#install);
testing evidence is documented separately.

X11 is the only officially supported environment and `x86_64`/`amd64` on
Xfce 4.20 is the primary live quality target. Wayland is experimentally
supported with a documented positioning fallback when optional layer-shell
support is unavailable.

- [Support and compatibility](docs/support.md)
- [Known limitations](docs/known-limitations.md)
- [Testing](docs/testing.md)
- [Translation status](docs/translations.md)

## Build from source

Install the dependencies in the
[source-build guide](docs/installation.md#build-from-source), then run:

```bash
git clone https://github.com/matteobonanomi/xfce4-meowmenu-plugin.git
cd xfce4-meowmenu-plugin
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
sudo meson install -C build
```

## Configure

Right-click the panel button and choose **Properties**. Four built-in presets
ship with MeowMenu: Classic, Modern, Full Screen, and Minimal. Settings live in
Xfconf and can be changed through the graphical preferences. See the
[preset guide](docs/presets.md) and
[configuration reference](docs/configuration.md).

## Contribute and report

- [Contributing guide](CONTRIBUTING.md)
- [Ordinary bug reports](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=bug-report.yml)
- [Compatibility reports](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml)
- [Security policy](.github/SECURITY.md)
- [Translation review](docs/translations.md)

MeowMenu includes 56 gettext catalogs. Technical validity, translation
coverage, fallback behavior, and fluent review are reported separately. Italian
and British English are currently pending maintainer fluent review; review by
fluent speakers is welcome for every language.

## License and credits

MeowMenu is distributed under the [GNU General Public License v2](COPYING), or
any later version. Original Whisker Menu was created by Graeme Gott; its
[source remains available from Xfce](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin).

MeowMenu was developed with AI assistance; every change is maintainer-reviewed.
