---
layout: default
title: Testing
nav_order: 7
---

# Testing MeowMenu

A useful test does not need to cover every feature or desktop setup. A short,
clear report from your own system is enough.

## Before you start

Please note:

- the MeowMenu version;
- your distribution and Xfce version;
- whether you use X11 or Wayland;
- how you installed MeowMenu.

Use a disposable profile or virtual machine only when testing upgrades or
changes that could affect your panel configuration.

## Five-minute check

1. Install MeowMenu and restart the panel with `xfce4-panel -r`.
2. Add **MeowMenu** through **Add New Items**.
3. Open the menu and search for an application you know.
4. Launch the application.
5. Press `Tab` to switch between Applications and Places, if Places is enabled.
6. Move through a few items with the arrow keys and close the menu with `Esc`.
7. Open the Properties dialog and change one setting or preset.
8. Reopen the menu and confirm that the change was saved.

That is enough for a basic test.

## Optional checks

If you have a little more time, try one or two of these:

- switch between the built-in presets;
- try both list and icon-grid results;
- test a Calculator expression if an engine is installed;
- drag an application or Place to Favourites;
- repeat a quick search after logging out and back in;
- try a dark theme, vertical panel, or scaled display.

There is no need to complete every optional check.

## Report a result

For a problem, open an
[ordinary bug report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=bug-report.yml).

If MeowMenu works and you want to share your system details, use a
[compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml).

Describe what you tried and what happened. Screenshots are helpful for visual
problems, but they are not required.

See [Support and compatibility](support) for the currently supported systems
and [Keyboard navigation](keyboard-navigation) for the complete key reference.
