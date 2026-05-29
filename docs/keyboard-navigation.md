---
title: Keyboard navigation
layout: default
nav_order: 6
has_children: false
---

# Keyboard navigation

MeowMenu is fully keyboard-driven. Every visible area — search box,
results, sidebar categories, the Apps/Places switch, and the session
buttons — can be reached and operated without touching the mouse.

This page lists the shortcuts that work on every preset and on both
X11 and Wayland.

## Quick reference

| Key                  | Effect                                                                  |
|----------------------|-------------------------------------------------------------------------|
| Type any letter      | Starts (or extends) a search query — focus jumps to the search box.     |
| Space                | While searching, inserts a space into the query (it does not "click").  |
| `Backspace`          | Removes the last character from the query. Stops at the empty query.    |
| `Enter`              | Launches the highlighted item; on the search box, launches the first match. |
| `Tab` / `Shift+Tab`  | Cycles focus between zones: Search → Results → Sidebar → Apps/Places → Session buttons (and back). |
| `↑` / `↓`            | Moves the selection one row up or down inside the current list.         |
| `Page Up` / `Page Down` | Moves the selection by one visible page inside the list.             |
| `Home` / `End`       | Jumps to the first / last item inside the current list or sidebar.      |
| `←` / `→`            | Inside the sidebar: exits to the results. Inside the results: exits to the sidebar (only while not searching). |
| `Esc`                | Closes context menus, then cancels a resize, then clears the query, then closes the menu. |
| `Shift+F10` or `Menu`| Opens the context menu for the highlighted launcher.                    |

## Tab order

Pressing `Tab` walks the menu in a fixed logical order:

1. **Search box** — always the first stop.
2. **Results list** — current category or search results.
3. **Sidebar** — category buttons. Skipped while a query is active.
4. **Apps / Places switch** — only present when Places is enabled.
5. **Session buttons** — lock, logout, suspend, etc.

`Shift+Tab` walks the same cycle in reverse. Zones that are hidden by
the current preset (for example, no profile bar) are skipped silently.

## Inside each zone

**Sidebar.** `↑` / `↓` (or `←` / `→` when the sidebar is horizontal)
move between categories and wrap at the ends. `Home` / `End` jump to
the first / last category. The arrow that points toward the results
exits the sidebar; the opposite arrow does nothing.

**Results list.** `↑` / `↓` move row by row (no wrap). `Page Up` /
`Page Down` move one screenful at a time. `Home` / `End` jump to the
first / last item. `Enter` launches.

**Apps / Places switch.** Any arrow key flips to the other side. The
arrow that points toward the search box does **not** flip — use `Tab`
to leave. `Space` is treated as a normal space character when you are
typing, so it does not toggle the switch.

**Session buttons.** `←` / `→` move between visible buttons (no wrap).
`Enter` (or `Space`) activates the focused button. `↑` / `↓` do
nothing inside this strip.

## The `Esc` ladder

A single press peels exactly one layer, in this strict order:

1. If a right-click context menu is open, close just that menu.
2. Otherwise, if a window resize is in progress, cancel the resize and
   restore the previous size.
3. Otherwise, if the search box contains text, clear the query.
4. Otherwise, close the menu.

`Backspace` and `Delete` never close the menu — only `Esc` (or the
configured global toggle shortcut) does.

## Type-to-search

You can start typing at any time. Letters, digits, punctuation, and
emoji are all routed into the search box, regardless of which zone
holds focus. Keys that do not produce text (`Shift`, `Ctrl`, function
keys, arrow keys, `Insert`, `Delete`…) are passed through to the
focused widget unchanged.

The redirect respects active input-method composition: a half-typed
Japanese / Chinese / Korean character is finished where it was
started, not in the search box.

## RTL languages

The logical Tab order (Search → Results → Sidebar → Switch → Buttons)
is the same in RTL locales such as Arabic and Hebrew. Only the visual
direction of the sidebar-exit arrow is mirrored, so the arrow always
points the right way on screen.

## Wayland note

On Wayland the layer-shell preset keeps the menu open even when focus
moves to another window briefly (for example, when a notification
takes focus). Close the menu the way you opened it (the global
shortcut) or press `Esc`.

[Back to top](#keyboard-navigation)
