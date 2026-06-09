---
title: Keyboard navigation
layout: default
nav_order: 6
has_children: false
---

# Keyboard navigation

MeowMenu can be driven entirely from the keyboard: open it, start typing,
or use the keys below. These shortcuts work on every preset and on both
X11 and Wayland.

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

- **Results / Sidebar** — `↑` / `↓` move the selection; `Home` / `End`
  jump to the ends. The sidebar wraps and also takes `←` / `→` when it is
  laid out horizontally. Exactly one item is highlighted at a time, whether
  you move by keyboard or by mouse — the highlight follows you and never
  leaves a trail.
- **Sidebar ↔ Results** — the arrow pointing toward the results leaves the
  sidebar, and the opposite arrow returns (only while not searching).
- **Session buttons** — `←` / `→` move between buttons; `Enter` activates.

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
- On Wayland (layer-shell preset) the menu stays open if focus briefly
  leaves it — close it with `Esc` or your global shortcut.
