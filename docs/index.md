---
title: Home
layout: default
nav_order: 1
has_children: false
---

# MeowMenu

![MeowMenu demo](assets/images/demo.gif)

MeowMenu is a panel-plugin launcher for XFCE4. It originated as a fork of
[Whisker Menu](https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin)
and brings saved layout presets, file search, a full-screen launcher mode,
and deeper visual customization — while staying native to Xfce and light
on resources.

## Quick Install

**Ubuntu / Debian** — download the `.deb` from the
[Releases page](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases/latest)
and run:

```bash
sudo apt install ./xfce4-meowmenu-plugin_<version>_ubuntu26.04_amd64.deb
xfce4-panel -r
```

**Build from source (all distros)**

```bash
git clone https://github.com/matteobonanomi/xfce4-meowmenu-plugin.git
cd xfce4-meowmenu-plugin
meson setup build && meson compile -C build && sudo meson install -C build
xfce4-panel -r
```

Then right-click the panel → **Add New Items** → **MeowMenu**.

## What's different from Whisker Menu

| Feature | Whisker Menu | MeowMenu |
|---|:---:|:---:|
| Saved layout presets | ✗ | ✓ |
| Places / file search | ✗ | ✓ |
| Full-screen launcher mode | ✗ | ✓ |
| Corner radius / opacity | ✗ | ✓ |
| Modern layout options | ✗ | ✓ |
| 54 languages | ✓ | ✓ |
| XFCE panel native | ✓ | ✓ |
| Keyboard navigation | ✓ | ✓ |

→ [Installation](installation) · [Presets](presets) · [Configuration](configuration) · [Keyboard navigation](keyboard-navigation) · [FAQ](faq)
