---
title: Keyboard navigation
layout: default
nav_order: 9
has_children: false
---

# Keyboard navigation

MeowMenu can be used entirely from the keyboard. Open it and start typing to
search, or use the keys below.

## Quick reference

| Key | Action |
|-----|--------|
| Type any text | Start or continue a search. |
| `Enter` | Open the selected result. |
| `Space` | Add a space to the search query. |
| `Backspace` | Delete from the search query. |
| `Tab` / `Shift+Tab` | Switch between Applications and Places. |
| Arrow keys | Move through results and visible controls. |
| `Page Up` / `Page Down` | Move through results one page at a time. |
| `Esc` | Step back or close the menu. |
| `Shift+F10` or `Menu` | Open the selected item's context menu. |

`Ctrl+Tab` and `Ctrl+Shift+Tab` are intentionally unused.

## Searching

Start typing from anywhere in the menu. Text, punctuation, spaces, emoji, and
normal input-method text are sent to the search field.

`Enter` opens the selected result. If focus is still in the search field, it
opens the first result.

## Applications and Places

Press `Tab` or `Shift+Tab` to switch between Applications and Places. The new
view opens its default page and selects the first available result. These keys
do nothing when Places is disabled.

## Moving around

Arrow keys first move within the current area. At an edge, they move to the
nearest visible area in that direction. Navigation does not wrap around.

- In a list, `Up` and `Down` move one result at a time.
- In a grid, all four arrow keys follow the visible rows and columns.
- In a vertical sidebar, `Up` and `Down` move through categories.
- In a horizontal sidebar, `Left` and `Right` move through categories.
- In Session controls, `Left` and `Right` move between buttons.

When **Switch categories by hovering** is enabled, moving to a category also
opens it. Otherwise, press `Enter` or `Space` to open the selected category.

## The Esc key

Repeated presses of `Esc` step back in this order:

1. close an open context menu;
2. cancel a resize;
3. clear the search query;
4. close MeowMenu.

## Context menus

Select an application and press `Shift+F10` or the `Menu` key to see its
actions. This includes **Add to Desktop**.

For a file or folder in Places, the context menu includes **Add Desktop Link**.
These actions are also available in Full Screen mode, where desktop dragging is
not used.

Wayland behavior can depend on the compositor. See
[Known limitations](known-limitations) for the current support notes.
