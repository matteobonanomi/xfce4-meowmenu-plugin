# MeowMenu: make your XFCE run!

[![Version](https://img.shields.io/github/v/release/matteobonanomi/xfce4-meowmenu-plugin)](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases)
[![CI](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/actions/workflows/ci.yml)
[![License: GPL v2+](https://img.shields.io/badge/License-GPLv2%2B-blue.svg)](COPYING)
[![Language: C++](https://img.shields.io/badge/language-C%2B%2B-00599C.svg?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Build: Meson](https://img.shields.io/badge/build-Meson-A41E50.svg)](https://mesonbuild.com/)
[![Toolkit: GTK 3](https://img.shields.io/badge/toolkit-GTK%203-7FAE3D.svg?logo=gtk)](https://www.gtk.org/)
[![Desktop: Xfce 4.20](https://img.shields.io/badge/desktop-Xfce%204.20-2284F2.svg?logo=xfce&logoColor=white)](https://xfce.org/)
[![Upstream: Whisker Menu](https://img.shields.io/badge/fork%20of-Whisker%20Menu-orange.svg)](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin)

![MeowMenu demo](docs/assets/images/demo.gif)

MeowMenu is a native Xfce panel launcher and a standalone fork of
[Whisker Menu](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin).
It adds saved presets, Places integration, flexible layouts, and an optional
inline Calculator while keeping the familiar launcher workflow.

**Fully keyboard-driven** — **Tab** switches Applications/Places,
**Ctrl+Tab** moves between areas, and the arrows move within them. See
[Keyboard navigation](docs/keyboard-navigation.md) for the full reference.

📖 **[Full documentation](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/)** &nbsp;·&nbsp;
📦 **[Changelog & downloads](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases)**

---

## Table of contents

1. [Installation](#installation)
2. [Tested distributions](#tested-distributions)
3. [Build from source](#build-from-source)
4. [Presets](#presets)
5. [Properties window](#properties-window)
6. [Configuration](#configuration)
7. [Localization](#localization)
8. [License and credits](#license-and-credits)

---

## Installation

Release packages are available for Ubuntu 26.04, Debian 13, and Fedora 44.
Arch and Arch-based distributions can use the maintainer-published
[AUR package](https://aur.archlinux.org/packages/xfce4-meowmenu-plugin):
`yay -S xfce4-meowmenu-plugin`.

After installing or removing MeowMenu, run `xfce4-panel -r`. Then add
**MeowMenu** from the panel's **Add New Items** menu. The
[installation guide](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/installation)
has package commands, dependencies, and cleanup instructions.

[↑ Back to top](#table-of-contents)

---

## Tested distributions

| Distribution | Tested | Notes |
|---|---|---|
| Xubuntu 26.04 | 2026-06 | author-verified |
| Debian 13 | 2026-06 | author-verified |
| Arch Linux | 2026-06 | author-verified |

Using MeowMenu on another distribution? Please
[open an issue](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues)
with its name and version, and help test future releases.

[↑ Back to top](#table-of-contents)

---

## Build from source

```bash
git clone https://github.com/matteobonanomi/xfce4-meowmenu-plugin.git
cd xfce4-meowmenu-plugin
meson setup build
meson compile -C build
sudo meson install -C build
```

Install the required dependencies first. See
[Build from source](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/installation#build-from-source)
for the distribution-specific lists. Remove a source install with
`sudo ninja -C build uninstall`.

[↑ Back to top](#table-of-contents)

---

## Presets

A preset saves the visual and layout settings as a `.meowpreset` file.
Switching one changes the menu immediately.

Four built-in presets ship with MeowMenu:

| Preset | Description |
|---|---|
| **Classic** | Traditional docked list with a right sidebar. |
| **Modern** | Rounded layout with Places search and hover category switching. |
| **Full Screen** | Full-screen launcher for touch or keyboard-first use. |
| **Minimal** | Centered, compact app list without sidebar, profile, or commands. |

Open **Properties → General** to select, save, import, or export presets. See
the [preset guide](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/presets)
for defaults and custom files.

[↑ Back to top](#table-of-contents)

---

## Properties window

Right-click the MeowMenu panel button → **Properties**.

| Section | What you configure |
|---|---|
| **General** | Presets, layout, size, appearance, and panel button. |
| **User / Session** | Profile and session-command placement. |
| **Search Bar** | Position, matching, ranking, and search actions. |
| **Results View** | List, tree, or grid results and their icons. |
| **Sidebar** | Categories, Favorites, and the unified bar. |
| **Places** | File search, bookmarks, and Apps/Places switch style. |
| **Extras** | Optional Calculator engine, result size, and precision. |

[↑ Back to top](#table-of-contents)

---

## Configuration

Settings are stored in Xfconf and available through **Properties**. The
[configuration guide](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/configuration)
includes every setting and its Xfconf key.

[↑ Back to top](#table-of-contents)

---

## Localization

MeowMenu ships 54 language catalogs inherited from Whisker Menu. Only Italian
has been checked by a native speaker, so translation reviews are welcome. Edit
the relevant `po/<lang>.po` file, run `msgfmt --check`, and open a pull request.

[↑ Back to top](#table-of-contents)

---

## License and credits

MeowMenu is distributed under the [GNU General Public License v2](COPYING)
(or any later version), the same license as the original project.

Original Whisker Menu was created by **Graeme Gott**.

- Whisker Menu source: <https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin>
- Graeme Gott's site: <https://gottcode.org>

[↑ Back to top](#table-of-contents)

---

> [!CAUTION]
> **Coding Agents - AI usage disclaimer**
>
> MeowMenu was developed with the help of AI coding agents and large language
> models, used under a spec-driven approach: each change starts from a written
> specification, so the results stay explainable and auditable rather than
> opaque.
>
> Open-source and open-weights tools were preferred wherever a suitable option
> was available.
>
> AI assistance was applied to three areas in particular: the initial analysis
> of the original Whisker Menu codebase; the analysis and bug-fixing of
> critical features (such as Places and code refactoring); and security
> hardening. The maintainer reviews all changes before they ship.
