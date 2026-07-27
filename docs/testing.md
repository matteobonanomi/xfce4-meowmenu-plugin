---
layout: default
title: Test a release candidate
nav_order: 7
---

# Test a release candidate

Record the MeowMenu version, distribution/version, Xfce version, architecture,
session type, and installation method before starting. Use a disposable profile
or VM for upgrade tests. Do not publish private contact details in evidence.

## Five-minute core check

1. Install the candidate package. Confirm the package version and the version
   shown in **About**, then restart the panel.
2. Add **MeowMenu** through **Add New Items**. On a fresh profile, confirm the
   Modern preset is selected.
3. Open the menu, search for a known application, and launch it.
4. Press Tab to switch Applications/Places, Ctrl+Tab to move among areas,
   arrows to navigate, and Enter to activate.
5. Close with Escape. Log out and in, then confirm the panel item, search, and
   keyboard flow still work.

A pass provides **Ran it** evidence only for the exact candidate and environment
recorded above. File a
[compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml);
prefer a durable public issue or release note over a short-lived workflow link.

## Optional five-minute extension

- Switch among the built-in presets.
- Test a Calculator expression, then select or simulate a missing engine and
  confirm ordinary search still works.
- Drag an application or Place to Favourites.
- Try the available desktop action for an application or Place.
- If convenient, repeat a quick open/search check with a dark theme, vertical
  panel, or scaled display. Missing optional visual evidence is not a failure.

## Fresh Xubuntu release gate

Use Xubuntu 26.04, Xfce 4.20, X11, and `x86_64` with a new profile. Complete
the core check and record the installed/About versions, Modern default, every
core result, and the result after login.

## Upgrade from 0.8.0

1. In a separate profile, install 0.8.0 and add MeowMenu to the panel.
2. Add representative ordered favourites; choose a non-default layout; select
   Calculator engine, result size, and precision; save a custom preset.
3. Record `xfconf-query -c meowmenu -lv` and copy the saved `.meowpreset`
   files to a comparison directory.
4. Upgrade to RC1 without deleting configuration.
5. Compare the panel item, favourites and order, layout/preferences,
   Calculator choices, Xfconf output, and preset files.
6. Log out and in and compare again.
7. Restart MeowMenu once more to exercise migration a second time. Confirm the
   values are unchanged.

Expected result: no panel item, user choice, or preset is lost or silently
reset.

## Removal and full cleanup

Removing a DEB, RPM, or Arch package removes installed program files but
normally retains the user's Xfconf settings and saved presets. This makes a
later reinstall recover the configuration. Follow the
[full cleanup instructions](installation#full-cleanup) only when you
deliberately want to erase that user state.
