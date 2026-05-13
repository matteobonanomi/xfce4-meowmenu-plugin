# MeowMenu: make your XFCE run!

MeowMenu is a fork of [Whisker Menu](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin)
for Xfce that keeps the familiar panel launcher feel while bringing a cleaner modern look,
a more capable search bar, and extra customization options for users who want the menu to
fit their workflow.

Built for Xubuntu 26.04 with Xfce 4.20.x.

---

## Installation

Requirements: `meson`, `ninja`, `gcc`/`clang`, and the Xfce development libraries
(`garcon-1`, `libxfce4panel-2.0`, `libxfce4ui-2`, `libxfce4util-1.0`, `libxfconf-0`,
`gtk+-3.0`).

```bash
git clone https://github.com/matteobonanomi/xfce4-meowmenu-plugin.git
cd xfce4-meowmenu-plugin

meson setup build
meson compile -C build
sudo meson install -C build
```

After installing, restart the Xfce panel:

```bash
xfce4-panel -r
```

Then right-click the panel → **Add New Items** → search for **MeowMenu** and add it.

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
/usr/local/share/xfce4-whiskermenu-plugin/
```

You can inspect them directly or copy one to `~/.local/share/meowmenu/presets/` and edit
it as your own custom preset.

---

## License and credits

MeowMenu is a fork of Whisker Menu and is distributed under the
[GNU General Public License v2](COPYING) (or any later version), the same license as
the original project.

Original Whisker Menu was created by **Graeme Gott** — thank you for building such a
solid foundation.

- Whisker Menu source: <https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin>
- Graeme Gott's site: <https://gottcode.org>
