---
title: Keyboard navigation
layout: default
nav_order: 9
has_children: false
---

# Keyboard navigation

MeowMenu can be driven entirely from the keyboard: open it, start typing,
or use the keys below. These shortcuts are tested on the primary X11 path.
Wayland remains experimental and unverified live.

## Quick reference

| Key                  | Effect                                                                  |
|----------------------|-------------------------------------------------------------------------|
| Type any text        | Starts or extends a search from any normal region.                       |
| `Enter`              | Launches the highlighted item (the first match on the search box).      |
| `Space`              | Adds one literal space to the query; it never activates the focused item. |
| `Backspace`          | Deletes from the query; on an empty query it does nothing.               |
| `Tab` / `Shift+Tab`  | Switches between **Applications** and **Places** (when Places is on).   |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Does nothing; it never starts focus traversal.                    |
| `↑` / `↓` / `←` / `→` | Moves within the focused region, then crosses to the nearest usable region. |
| `Page Up` / `Page Down` | Moves the selection one screenful at a time.                         |
| `Esc`                | Steps back one level (see below).                                       |
| `Shift+F10` or `Menu`| Opens the context menu for the highlighted item.                        |

The context menu is also the non-drag route for desktop entries. Highlight an
application and open its context menu to use **Add to Desktop**. In Places,
highlight a file or folder and open its context menu to use **Add Desktop
Link**. These actions are available in FullScreen as the supported alternative
to dragging to the desktop.

## Switching Applications and Places

`Tab` and `Shift+Tab` switch between **Applications** and **Places** from
anywhere in the menu. The new view clears the old query, opens its default
content (Applications → your default category, Places → Home), and focuses
the first visual result. If no result exists, Search keeps focus. When Places
is disabled, `Tab` and `Shift+Tab` do nothing. `Ctrl+Tab` and
`Ctrl+Shift+Tab` are consumed no-ops.

## Moving between areas

Arrow navigation is one physical model across Search, Results, Sidebar, and
Session controls. It first moves within the current region. Only at a true
edge does it score visible targets in the pressed direction by forward
separation, perpendicular alignment, then visual order. Hidden, insensitive,
decorative, empty, and mode-switch controls are skipped. There is no wrapping;
an outer edge with no target is a consumed no-op.

In Full Screen, Profile, Search, and available Session actions share the fixed
top row. With no vertical sidebar, Apps/Places also belongs to that row between
Profile and Search; with a vertical sidebar, it remains part of the sidebar.
Horizontal category navigation stays in its strip, while Apps/Places follows
the no-vertical-sidebar row and spans the same centred width as Results. When
the category entries fit, the Horizontal group is centered within its strip;
the strip uses the full menu width in Docked/Centered and the Results-width
column in Full Screen. Overflow remains inside the strip, and Apps/Places is
not part of the centered category group. The whole logical row mirrors in
right-to-left interfaces. No hidden alternate search or bottom row participates
in focus navigation.

## Moving within an area

- **Results** — list results move one displayed row at a time; grids use their
  actual visible cells in all four directions, including an incomplete final
  row. Calculator is the first visual result when visible. Exactly one item is
  highlighted at a time, whether you move by keyboard or by mouse.
- **Sidebar** — the arrow keys along the sidebar move through the categories
  one per press and **keep focus in the sidebar** the whole time, without
  wrapping. The along-axis keys are `↑` / `↓` for a vertical sidebar and
  `←` / `→` for a horizontal one. While a query is non-empty, Sidebar remains
  excluded from Search and vertical Results exits, but a focused Results item
  may leave horizontally toward a visible vertical sidebar. Selecting a
  category with the **mouse** instead
  hands focus to the search box so you can start typing right away.
- **Region crossing** — any arrow toward a visible neighbouring region uses
  the same live-geometry rule. Search remains reachable even while typing.
- **Session buttons** — `←` / `→` move between buttons; `Enter` activates.

How arrowing through categories affects the results depends on the
**“Switch categories by hovering”** sidebar setting. With it **on**, the highlighted
category opens live as you arrow, so the results preview each one. With it
**off**, arrowing only moves the highlight and the results stay put until you
press `Enter` (or `Space`) to confirm the highlighted category.

## The `Esc` ladder

One press undoes one thing, in this order: close an open context menu →
cancel a resize → clear the search text → close the menu.

## Type to search

Start typing at any time. Letters, digits, punctuation, spaces, shifted
symbols, non-Latin characters, emoji, and committed input-method text go to the
search box, wherever normal menu focus is. Space is inserted literally before
button activation or input-method candidate handling. In-progress composition
and dead-key input remain Search-owned; Ctrl/Alt/Super/Meta shortcuts do not
alter the query.

When a query refreshes, Calculator is the first anchor when visible, followed
by the first ranked list row or top-left grid item. If no result exists, Search
keeps focus. Home recursive Places results use a one-result focus lease: the
first current result selects, reveals, and focuses once; stale results or a
deliberate arrow, pointer, Tab, section, or mode departure may still display
but never steal focus.

The configured desktop shortcut, including a selected bare Super shortcut,
remains outside this in-menu model and continues to toggle MeowMenu when the
desktop delivers it. MeowMenu does not claim or reconfigure that shortcut.

## Notes

- Left and Right remain physical directions in right-to-left locales. Native
  Search cursor editing keeps its locale-specific behavior, while ties between
  equally placed targets use physical leading-to-trailing order.
- On Wayland, compositor and optional layer-shell behavior can differ. Close
  the menu with `Esc` or the configured global shortcut if focus behavior is
  unexpected.
