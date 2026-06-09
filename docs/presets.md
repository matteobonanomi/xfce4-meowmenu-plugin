---
title: Presets
nav_order: 3
has_children: false
---

# Presets

A preset is a snapshot of all visual and layout settings saved as a plain
`.meowpreset` file. Switching presets reshapes the menu instantly — size,
corner radius, icon style, sidebar position, and more — without touching
individual options.

A fresh installation starts on the **Modern** preset. Upgrades keep your
existing layout untouched — nothing is reset on upgrade, and the active-preset
label reflects the layout you are actually running.

## Selecting a built-in preset is a full reset

Choosing a built-in preset restores **that preset's complete documented
layout**, regardless of any changes you made beforehand. The settings a preset
governs include the whole appearance and layout surface — window size, corner
radius, opacity, icon size, the sidebar position, **whether the sidebar is
shown**, **whether categories show names or are icon-only**, the Apps/Places
switch style, and more. None of your prior edits to those settings leak across a
switch.

Selecting the preset you are already on re-applies it — a convenient "reset to
this preset". The **Reset preset** button on **Properties → General** does the
same for the currently selected preset.

Changes you make after selecting a preset are not saved back into a built-in
preset. To keep a customised layout, save it as a custom preset (see below); the
built-in presets are never modified.

## Built-in presets

### Classic

<!-- preset-classic.png not yet available -->

Traditional Whisker Menu look. Compact docked window, apps as a list,
sidebar on the right, no rounded corners. The Apps/Places switch uses text
labels.

### Modern

![Modern preset](assets/images/preset-modern.png)

Contemporary layout with rounded corners, fully opaque surfaces (both the
categories and the app list at 100%), hover-to-switch-category, and Places
search enabled. The Apps/Places switch uses icon buttons.

### Full Screen

<!-- preset-fullscreen.png not yet available -->

Launcher fills the entire screen over an 80% translucent backdrop. Ideal for
touch or keyboard-first workflows. Places search enabled. The Apps/Places switch
uses text labels.

### Minimal

<!-- preset-minimal.png not yet available -->

A compact, distraction-free launcher: a search bar over an app list, with no
sidebar, no profile area, and no command buttons. Opens centred on screen in a
short window with lightly translucent surfaces (60%), Places enabled with icons,
opening on the Recent category. Switch to any other built-in to bring the
sidebar, profile, and commands back.

Each preset sets its own default for the Apps/Places **Show icons** option
(Modern and Minimal on; Classic and Full Screen off). Switching presets updates
the default, but your own later changes to the option always win.

## The Unsaved custom state

The moment you change any governed setting, the preset field on
**Properties → General** switches to **Unsaved custom** to show that the live
layout no longer matches the selected preset. This is reversible: edit the value
back to match the preset and the field snaps straight back to the preset's name.
Switching to another preset discards the unsaved changes and applies that
preset. The field is never blank — it always reads either a preset name or
**Unsaved custom**.

## How the dropdown reads at a glance

The preset dropdown distinguishes the three kinds of entry by their styling:

- **Built-in** presets (Classic, Modern, Full Screen, Minimal) are shown in **bold**.
- **Saved custom** presets are shown in regular weight.
- The transient **Unsaved custom** entry is shown in *italic*.

## Saving, renaming, deleting

The preset hub on **Properties → General** manages custom presets without
editing files by hand:

- **Save as new…** — captures the current layout as a new custom preset. Give it
  a name that is not empty and does not duplicate another preset (built-in or
  custom); the new preset appears in the dropdown immediately, already selected.
- **Rename…** — renames the selected custom preset; the dropdown label updates
  at once and the preset stays selected.
- **Delete** — removes the selected custom preset. If it was the active one, the
  layout falls back to **Modern**.

Built-in presets cannot be renamed, deleted, or exported — those actions are
available only for your own saved presets.

## Exporting and importing

- **Export…** writes the selected custom preset to a `.meowpreset` file you can
  share or back up. (Built-in presets are not exportable.)
- **Import…** loads a `.meowpreset` file. If its name clashes with an existing
  custom preset you can **Overwrite**, **Rename**, or **Cancel**; if it clashes
  with a built-in name you can only **Rename** or **Cancel**.

Import is tolerant of schema differences in otherwise valid files: unknown
settings are ignored, settings the file omits fall back to their defaults, and a
file written by a newer (or version-less) MeowMenu is accepted on a best-effort
basis. Only unreadable files, or files missing the `[Preset]`/`[Settings]`
sections or the preset name, are rejected — and a rejected import never changes
your saved presets.

## Advanced: hand-authored preset files

You can also create a preset by writing the file yourself with a `[Preset]`
header block and a `[Settings]` block containing the desired keys:

```ini
[Preset]
Name=My Custom Preset
SchemaVersion=1
Id=my-custom
Description=My preset

[Settings]
layout-mode=docked
corner-radius=8
```

Save the file as `<id>.meowpreset` (the filename stem must match the `Id`
field). Then place it in:

```
~/.local/share/meowmenu/presets/
```

Restart the panel with `xfce4-panel -r`. The preset will appear in
**Properties → General → Preset**.

A file whose `Id` matches a built-in preset overrides the system version.

## File format

```ini
[Preset]
Name=My Custom Preset
SchemaVersion=1
Id=my-custom
Description=My preset

[Settings]
layout-mode=docked
corner-radius=8
```

Unknown keys are silently ignored. Malformed files are skipped without
crashing the panel.
