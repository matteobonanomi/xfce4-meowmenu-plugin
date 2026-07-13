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

MeowMenu is a panel-plugin launcher for the Xfce desktop. It is a standalone
project that originated as a fork of [Whisker Menu](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin)
and keeps the familiar panel-launcher feel while bringing a cleaner modern
look and more customization options.

**Fully keyboard-driven** — **Tab** switches Applications/Places,
**Ctrl+Tab** moves between areas, and the arrows move within them. See
[Keyboard navigation](docs/keyboard-navigation.md) for the full reference.

📖 **[Full documentation](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/)** &nbsp;·&nbsp;
📦 **[Changelog & downloads](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases)**

---

## Table of contents

1. [Installation / uninstallation](#installation--uninstallation)
2. [Tested distributions](#tested-distributions)
3. [Build from source](#build-from-source)
4. [Presets](#presets)
5. [Properties window](#properties-window)
6. [Configuration file (for power users)](#configuration-file-for-power-users)
7. [Localization](#localization)
8. [License and credits](#license-and-credits)

---

## Installation / uninstallation

Prebuilt `.deb` / `.rpm` packages for the three officially supported
distributions — Ubuntu 26.04, Debian 13, and Fedora 44 —
are built and smoke-tested in CI and attached to every
[GitHub Release](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases/latest).
Arch and Arch-based distributions (CachyOS, EndeavourOS, Manjaro) can install
the maintainer-published [AUR package](https://aur.archlinux.org/packages/xfce4-meowmenu-plugin)
with `yay -S xfce4-meowmenu-plugin` — see the Arch entry below. On any other
distribution build from source (see next section); the AUR package and
source builds are best-effort and not exercised by CI.

After install or uninstall, restart the panel with `xfce4-panel -r`, then
right-click the panel → **Add New Items** → **MeowMenu**. Replace `<version>`
with the actual release (e.g. `0.7.2`).

<details>
<summary><strong>Ubuntu 26.04</strong></summary>

```bash
sudo apt install ./xfce4-meowmenu-plugin_<version>_ubuntu26.04_amd64.deb
```

Uninstall: `sudo apt purge xfce4-meowmenu-plugin`

</details>

<details>
<summary><strong>Debian 13</strong></summary>

```bash
sudo apt install ./xfce4-meowmenu-plugin_<version>_debian13_amd64.deb
```

Uninstall: `sudo apt purge xfce4-meowmenu-plugin`

</details>

<details>
<summary><strong>Fedora 44</strong></summary>

```bash
sudo dnf install ./xfce4-meowmenu-plugin-<version>-1.fc44.x86_64.rpm
```

Uninstall: `sudo dnf remove xfce4-meowmenu-plugin`

</details>

<details>
<summary><strong>Arch Linux (AUR) — CachyOS, EndeavourOS, Manjaro</strong></summary>

Install the AUR package with an AUR helper:

```bash
yay -S xfce4-meowmenu-plugin
```

This covers Arch and Arch-based distributions including CachyOS,
EndeavourOS, and Manjaro. The package
([xfce4-meowmenu-plugin](https://aur.archlinux.org/packages/xfce4-meowmenu-plugin))
is maintainer-published and distributed through the AUR; unlike the three
packages above it is not built or smoke-tested in CI.

Uninstall: `yay -R xfce4-meowmenu-plugin`

</details>

<details>
<summary><strong>Other distros</strong></summary>

No prebuilt package — see [Build from source](#build-from-source).
Uninstall: `sudo ninja -C build uninstall`.

</details>

### Remove user configuration

Any uninstall leaves user data behind. After removing the panel button:

```bash
rm -rf ~/.local/share/meowmenu/
rm -f ~/.config/xfce4/xfconf/xfce-perchannel-xml/meowmenu.xml
```

[↑ Back to top](#table-of-contents)

---

## Tested distributions

These distributions have been verified by the author. The list grows over
time — reports for other distributions are very welcome.

| Distribution  | Tested  | Notes           |
| ------------- | ------- | --------------- |
| Xubuntu 26.04 | 2026-06 | author-verified |
| Debian 13     | 2026-06 | author-verified |
| Arch Linux    | 2026-06 | author-verified |

Running MeowMenu on something not listed here? Please open an issue or a Pull
Request with your distribution and version so it can be added.

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

Uninstall: `sudo ninja -C build uninstall`.

Install the build dependencies for your distribution first. The complete
per-distribution dependency lists (Ubuntu/Debian, Fedora, Arch) live on the
documentation site:
**[Installation → Build from source](https://matteobonanomi.github.io/xfce4-meowmenu-plugin/installation#build-from-source)**.

[↑ Back to top](#table-of-contents)

---

## Presets

A preset is a snapshot of all visual and layout settings saved as a plain
`.meowpreset` file. Switching presets reshapes the menu instantly — size,
corner radius, icon style, sidebar position, and more — without touching
individual options.

Four built-in presets ship with MeowMenu:

| Preset          | Description                                                                                                     |
| -----------------| -----------------------------------------------------------------------------------------------------------------|
| **Classic**     | Traditional Whisker Menu look. Compact docked window, apps as a list, sidebar on the right, no rounded corners. |
| **Modern**      | Contemporary layout (rounded corners, fully opaque surfaces, change category by hovering, etc). Places search enabled. |
| **Full Screen** | Launcher fills the entire screen over an 80% translucent backdrop. Ideal for touch or keyboard-first workflows. Places search enabled. |
| **Minimal**     | Compact, distraction-free launcher: a centred window with a search bar over an app list and lightly translucent surfaces; no sidebar, profile, or command buttons. |

Drop any `.meowpreset` file into `~/.local/share/meowmenu/presets/` to make
it appear in **Properties → General → Preset** after a panel restart. A file
with the same `Id` as a built-in preset overrides the system version.

File format:

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

Unknown keys are silently ignored. Malformed files are skipped without
crashing the panel.

[↑ Back to top](#table-of-contents)

---

## Properties window

Right-click the MeowMenu panel button → **Properties**.

| Section | What you configure |
|---|---|
| **General** | Preset selector, layout mode (docked / full screen), window size, corner radius, panel gap, panel button appearance (icon, label, style). |
| **User / Session** | Profile picture visibility, username display, session command buttons (lock, logout, suspend, etc.). |
| **Search Bar** | Position (top / bottom), placeholder text, search action shortcuts. |
| **App Grid** | Default view (list or icon grid), icon size, grid density, default category on open, hover-to-switch-category. |
| **Sidebar** | Position (left / right), visible category buttons, commands bar position. |
| **Places** | Enable file/folder search, navigation, and quick actions in MeowMenu. |

[↑ Back to top](#table-of-contents)

---

## Configuration file (for power users)

All settings live in Xfconf. Read and write them without the GUI:

```bash
# List all MeowMenu properties for a panel plugin instance
xfconf-query -c xfce4-panel -lv | grep meowmenu

# Change a single property
xfconf-query -c xfce4-panel -p /plugins/<id>/corner-radius -s 8
```

Built-in `.meowpreset` files are installed under
`/usr/local/share/xfce4-meowmenu-plugin/` (or `/usr/share/...` from a
distribution package); copy one to `~/.local/share/meowmenu/presets/` to
customise it.

[↑ Back to top](#table-of-contents)

---

## Localization

MeowMenu ships translations for **54 languages**, just like WhiskerMenu does. All upstream Whisker Menu
translations are preserved; MeowMenu-specific strings (Places, Presets,
FullScreen mode, Sidebar, Favourites, fuzzy-search controls, and more) have
been extended across all locales via LLM-assisted translation. MeowMenu has been develop in English and only Italian has been validated so far by native spokers

Native speakers are warmly invited to review and improve any translation.
The PO files live in `po/` — pick your language, edit `msgstr` entries, run
`msgfmt --check po/<lang>.po`, and open a Pull Request.

[↑ Back to top](#table-of-contents)

---

## License and credits

MeowMenu is distributed under the [GNU General Public License v2](COPYING)
(or any later version), the same license as the original project.

Original Whisker Menu was created by **Graeme Gott** — thank you for
building such a solid foundation.

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
