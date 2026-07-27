---
title: Keyboard navigation
layout: default
nav_order: 9
has_children: false
---

# Keyboard navigation

MeowMenu can be driven entirely from the keyboard: open it, start typing,
or use the keys below. These shortcuts are tested on the primary X11 path.
Wayland remains experimental and unverified for the current candidate.

## Quick reference

| Key                  | Effect                                                                  |
|----------------------|-------------------------------------------------------------------------|
| Type any letter      | Starts or extends a search — focus jumps to the search box.             |
| `Enter`              | Launches the highlighted item (the first match on the search box).      |
| `Backspace`          | Deletes the last character of the query.                                |
| `Tab` / `Shift+Tab`  | Switches between **Applications** and **Places** (when Places is on).   |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Moves focus through the areas: Search → Results → Sidebar → Session buttons (and back). |
| `↑` / `↓`            | Moves the selection inside the current list.                            |
| `Page Up` / `Page Down` | Moves the selection one screenful at a time.                         |
| `Home` / `End`       | Jumps to the first / last item.                                         |
| `←` / `→`            | Crosses between the sidebar and the results (only while not searching). |
| `Esc`                | Steps back one level (see below).                                       |
| `Shift+F10` or `Menu`| Opens the context menu for the highlighted item.                        |

The context menu is also the non-drag route for desktop entries. Highlight an
application and open its context menu to use **Add to Desktop**. In Places,
highlight a file or folder and open its context menu to use **Add Desktop
Link**. These actions are available in FullScreen as the supported alternative
to dragging to the desktop.

## Switching Applications and Places

`Tab` and `Shift+Tab` switch between **Applications** and **Places** from
anywhere in the menu. The new view opens on its default content
(Applications → your default category, Places → Home) with focus on the
results, ready to arrow. When Places is disabled, `Tab` does nothing.

## Moving between areas

`Ctrl+Tab` moves focus through the areas in order — Search → Results →
Sidebar → Session buttons — wrapping around at the end, and `Ctrl+Shift+Tab`
goes back. Any area that is currently hidden, empty, or otherwise cannot take
focus is skipped, so a press always lands on a usable area; the sidebar is
skipped while you are searching. When only one area can take focus, `Ctrl+Tab`
simply stays put.

> Some desktops bind `Ctrl+Tab` globally; where they do, the menu never
> receives it and area cycling is unavailable.

## Moving within an area

- **Results** — `↑` / `↓` move the selection; `Home` / `End` jump to the
  ends. Exactly one item is highlighted at a time, whether you move by
  keyboard or by mouse — the highlight follows you and never leaves a trail.
- **Sidebar** — the arrow keys along the sidebar move through the categories
  one per press and **keep focus in the sidebar** the whole time, wrapping
  around at the ends; `Home` / `End` jump to the first / last category. The
  along-axis keys are `↑` / `↓` for a vertical sidebar and `←` / `→` for a
  horizontal one. Selecting a category with the **mouse** instead hands focus
  to the search box so you can start typing right away.
- **Sidebar ↔ Results** — the arrow pointing toward the results leaves the
  sidebar, and the opposite arrow returns (only while not searching).
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

Start typing at any time. Letters, digits, punctuation, and emoji all go
to the search box, wherever focus is; non-text keys are left untouched.
In-progress input-method (CJK) composition is preserved.

## Notes

- The area order is identical in right-to-left locales; only the
  sidebar-exit arrow is mirrored visually.
- On Wayland, compositor and optional layer-shell behavior can differ. Close
  the menu with `Esc` or the configured global shortcut if focus behavior is
  unexpected.
