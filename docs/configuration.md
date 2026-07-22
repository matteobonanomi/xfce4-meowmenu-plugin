---
title: Configuration
nav_order: 4
has_children: false
---

# Configuration

All MeowMenu settings are exposed in the **Properties** dialog
(right-click the panel button → **Properties**) and stored in Xfconf.

## Properties dialog

Each tab uses two equal-width columns. The tables below list every option.

### General

| Option | Description |
|--------|-------------|
| Preset | Select, save, rename, delete, export, or import a layout preset. The **?** control beside the selector reveals the active preset's description on hover or keyboard focus. |
| Layout mode | **Docked** (panel-attached window), **Centered** (a floating window pinned to the centre of the screen), or **FullScreen** (full-screen launcher). |
| Panel gap | Distance between the panel and the menu window, in pixels (Docked only). |
| Menu width | Width of the menu window in pixels. |
| Menu height | Height of the menu window in pixels. |
| Corner radius | Rounded-corner radius of the menu window, in pixels. |
| Menu opacity | Opacity of the whole menu background, applied uniformly in every layout mode (0–100, where 0 is fully transparent and 100 fully solid), applied live. Foreground content (labels, icons, the selected row) always stays fully opaque. Requires a compositor; without one the menu is always solid and the control is disabled. |
| Stay visible when focus is lost | Keep the menu open when another window receives focus. |

**Centered** opens on the panel's monitor at the configured size. Resize it
from any edge; it stays centred and remembers the new size. Panel gap does not
apply.

The Layout mode selected controls which other options are available. Disabled
options are greyed out and switch live as you change the mode:

| Option | Docked | Centered | FullScreen |
|--------|:------:|:--------:|:----------:|
| Menu width | ✓ | ✓ | — |
| Menu height | ✓ | ✓ | — |
| Panel gap | ✓ | — | — |
| Corner radius | ✓ | ✓ | — |
| Show panel button title | Display a text label next to the panel button icon. |
| Panel button title | The label text shown on the panel button. |
| Show panel button icon | Show the icon on the panel button. |
| Panel button icon | The icon used for the panel button. |
| Use a single panel row | Force the panel button into a single row regardless of panel size. |

### User / Session

| Option | Description |
|--------|-------------|
| Show user profile picture | Display the account avatar at the top of the menu. |
| Show username | Display the logged-in user's name. |
| Profile position | Where the profile block appears: top left, bottom left, or hidden. **Hidden** removes the avatar and username (and keeps them out of keyboard focus). |
| Session commands position | Where the lock/logout/suspend buttons appear: top-right, bottom-right, or hidden. **Hidden** removes the session buttons. |

Profile and Session commands are **independent**: hiding one always keeps the
other on its own side (profile on the left, session buttons on the right),
regardless of where the category list sits. The row collapses only when **both**
are hidden. When only one cluster remains visible it keeps its natural edge:
Profile stays left-aligned and Session commands stay right-aligned. **Hidden is
fully reversible** — switching a hidden element back to a visible position
restores it (and the row, if it had collapsed) immediately, with no restart and
no need to reset to defaults.

The two positions are **coupled** so the row always stays coherent:

* **Docked layout** — when both clusters are visible they always share one
  row. Choosing Profile = *Top Left* moves Session commands to *Top Right*;
  choosing Profile = *Bottom Left* moves Session commands to *Bottom Right*.
  The same is true in reverse: choosing *Top Right* or *Bottom Right* from the
  Session commands combo moves the Profile block to the matching left edge.
  The opposite-row option stays listed but greyed in both combos as a
  discoverable "move the whole row" action. When one cluster is *Hidden*, the
  visible cluster keeps its own edge and the hidden one stays hidden.
* **Full Screen layout** — both Profile and Session commands follow the
  **search bar** edge. With the search bar on top, both sit on the top edge (the
  bottom options are greyed); moving the search bar to the bottom moves both with
  it. Hiding both leaves only the centred search bar, at exactly the same size and
  position as when they are shown.

Disallowed edges are shown greyed rather than removed, and if a stored
combination is no longer valid for the current layout it is automatically snapped
to the nearest coherent edge (the element stays visible — only its edge moves).
| Lock screen command | Command run when the lock button is clicked. |
| Log out command | Command run when the log out button is clicked. |
| Suspend command | Command run when the suspend button is clicked. |
| Switch user command | Command run when the switch user button is clicked. |

### Search Bar

| Option | Description |
|--------|-------------|
| Position | Place the search bar at the **top** or **bottom** of the menu. |
| Placeholder text | The hint text shown when the search field is empty. |
| Fuzzy search | Enable approximate (typo-tolerant) matching. |
| Fuzzy threshold | Sensitivity of the fuzzy matcher (0 = automatic). |
| Favorites boost | Rank previously-used apps higher in search results. |
| Favorites boost level | Strength of the boost: low, medium, or high. |
| Search actions | Custom keyword-triggered commands (e.g. type `!` to run a shell command). |

Search-action command templates are split into arguments using shell-style
quoting, but they are launched directly rather than through a shell. In a
non-regex action, `%s` inserts the query text after the matched prefix, `%S`
inserts the complete query, `%u` inserts a URI-escaped form of the text after
the prefix, and `%%` inserts a literal percent sign. Use `%u` when arbitrary
text must remain one command argument. `%s` and `%S` preserve spaces and quote
characters, so they should be used only where the command template deliberately
expects argument splitting.

### Results View

| Option | Description |
|--------|-------------|
| View mode | **List** (compact), **Icon grid**, or **Tree** (category tree). |
| Show app name | Display the application name in each result row. |
| Show app description | Display the application comment below the name. |
| Show tooltip | Show a tooltip with the full app description on hover. |
| Icon size | Size of app icons in the results list (-1 = inherit from theme). |
| Grid density | Number of columns in icon-grid mode: low, medium, or high. |
| Grid columns | Explicit column count for icon-grid mode. |
| Grid rows | Number of visible rows in icon-grid mode. |
| Default category | Category shown on open: favorites, recent, or all apps. |
| Hover to switch category | Change the visible category by hovering over sidebar entries. |

### Sidebar

| Option | Description |
|--------|-------------|
| Enable sidebar | Turn the category sidebar on or off. When off, the menu shows no sidebar; if Places is enabled the Apps/Places switch moves to the right end of the search bar, and the results view gains a heading naming the default category (FAVORITES, RECENTLY USED, or ALL APPLICATIONS). |
| Position | Place the sidebar on the **left**, **right**, **top**, or **bottom**. Top and bottom use a horizontal, icon-only strip. In Full Screen, the strip, search bar, and results/application grid share the same width. Top sits below the search bar; Bottom sits below results. The strip scrolls when needed, and **Show category name** is unavailable. |
| Show category name | Display the category label next to its icon. On a left/right sidebar, hiding the names also makes the Apps/Places switch vertical so the sidebar can stay narrow. |
| Category icon size | Size of category icons (`-1` through `6`; `-1` inherits the theme size). |
| Sort categories | Sort the category list alphabetically. |
| Recent items max | Maximum number of recently-used apps to track. |
| Include favorites in recent | Also show favorited apps in the Recent category. |
| Unified bar | Render profile, search, and session controls on one horizontal row (FullScreen mode only). |

Drag an application onto the visible **Favorites** sidebar item to add it once.
The target is unavailable when the sidebar or item is hidden. The menu stays
open and uses a small icon-only preview.

In **Docked** and **Centered** layouts, drag applications to the desktop or an
accepting file manager. The destination decides placement and collisions. In
**FullScreen**, use the context-menu **Add to Desktop** action instead. Dragging
applications or Places does not close MeowMenu; every drag uses a small
icon-only preview.

### Places

| Option | Description |
|--------|-------------|
| Enable Places | Show a file/folder browser pane in the menu. |
| Show icons | Render the Apps/Places switch as two icon buttons (an app-grid icon and a folder icon, with tooltips) instead of text labels. Forced on, and shown greyed in Preferences, when the sidebar is on top/bottom or disabled. |
| Switch button shape | Choose whether the Apps/Places switch uses the active GTK theme's normal button shape or MeowMenu's rounded segmented shape. |
| Show recent files | Include recently opened files in the Places view. |
| Show bookmarks | Include user bookmarks (from Thunar or GTK bookmarks). |
| Bookmark sync | Keep the Places bookmarks in sync with **MeowMenu** or **Thunar**. |
| Max items | Maximum number of items shown in the Places view. |
| Remember last mode | Reopen MeowMenu showing the last-used Places sub-section. |

Drag a Places file or folder onto **Favourites** when **Bookmark sync** is
**MeowMenu**. The target is unavailable for disabled Places, sidebar, or
Favourites, and when sync is **Thunar**. The menu stays open and uses a small
icon-only preview.

In **Docked** and **Centered** layouts, drag Places items to the desktop or an
accepting file manager. Files keep their URI; folders create a link rather than
copying their contents. The destination handles placement and collisions. In
**FullScreen**, use **Add Desktop Link** from the context menu instead.

#### Missing recent files and bookmarks

When a recent item or bookmark points to a file or folder that has been deleted
or renamed, MeowMenu-owned Places favourites are removed from the visible
Favourites list the next time it is rebuilt. Externally managed Places
favourites are hidden from MeowMenu's visible Favourites list without changing
the external source. Recent items remain read-only; unavailable entries can
appear greyed-out, cannot be opened directly, and still offer **Open Containing
Folder** when a parent directory can be located.

### Extras

**Extras → Calculator** enables an optional inline calculator. Select an
installed engine and enter an expression such as `2 + 2`; use `=` to force a
lone number to calculate. Activating a result copies its full value.

Choose **None** to disable it. Missing engines are marked in the list; a missing
selected `bc` shows **bc package required**. Choose Auto or a semantic result
size, and set 0–10 decimal places. Display values round half away from zero;
available precision can differ by engine. These settings persist and are
included in presets. Long answers stay on one line, with the full value
available to hover and accessibility tools.

With Auto selected, MeowMenu follows the active preset: Minimal and custom or
unknown presets use Normal, Modern and Calculator-enabled Classic use Large,
and Full Screen uses Larger. Named choices stay exactly as selected. This only
changes presentation: Auto remains stored as Auto, long values keep the normal
result height, and the complete value remains available when the visible label
is shortened.

Calculator results use the normal result height and icon scale. They take one
full-width row in list and tree views, and span the grid at one tile high. A
successful result hides custom search actions and the Run fallback, while
application matches remain visible. Other states keep those fallbacks available.

## Xfconf reference

For scripting or headless configuration, read and write settings directly
via `xfconf-query`:

```bash
# List all MeowMenu properties for a panel plugin instance
xfconf-query -c xfce4-panel -lv | grep meowmenu

# Set a single property
xfconf-query -c xfce4-panel -p /plugins/<id>/<key> -s <value>
```

Replace `<id>` with the numeric plugin ID shown by
`xfconf-query -c xfce4-panel -lv | grep meowmenu`.

### Appearance

| Key (relative to `/plugins/<id>/`) | Type | Default | Description |
|------------------------------------|------|---------|-------------|
| `corner-radius` | int | 0 | Radius, in pixels, that rounds the menu's visible outer corners (0 = square). |
| `panel-gap` | int | 0 | Gap between the panel and the menu window. |
| `menu-width` | int | 450 | Menu window width in pixels. |
| `menu-height` | int | 500 | Menu window height in pixels. |
| `menu-opacity` | int | 100 | Opacity of the whole menu background in every layout mode (0 = fully transparent, 100 = fully solid), applied live. Foreground content always stays fully opaque; has no effect without a compositor. |
| `stay-on-focus-out` | bool | false | Keep menu open when focus moves away. |

### Layout

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `layout-mode` | string | `docked` | `docked`, `centered`, or `fullscreen`. |
| `sidebar-position` | string | `left` | `left`, `right`, `top`, or `bottom`. (A legacy `hidden` value is migrated to `sidebar-enabled = false`.) |
| `search-bar-position` | string | `top` | `top` or `bottom`. |
| `profile-position` | string | `top-left` | `top-left`, `bottom-left`, or `hidden`. Legacy `top`, `bottom`, and `bottom-right` values are accepted on load and rewritten to the canonical left-anchored form. |
| `commands-position` | string | `top-right` | `top-right`, `bottom-right`, or `hidden`. |
| `unified-bar` | bool | false | Merge profile, search, and session into one row (FullScreen only). |

### Results view

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `view-mode` | int | 1 | 0 = icons, 1 = list, 2 = tree. |
| `launcher-show-name` | bool | true | Show the app name. |
| `launcher-show-description` | bool | true | Show the app description. |
| `launcher-show-tooltip` | bool | true | Show a hover tooltip. |
| `launcher-icon-size` | int | -1 | Icon size (-1 = theme default). |
| `grid-density` | string | `medium` | `low`, `medium`, or `high` columns in grid mode. |
| `transparent-grid` | bool | false | In grid mode, make resting result tiles blend into the results area while keeping icons, labels, and interaction states visible. |

When results are shown as an icon grid, application tiles and Places file or
folder tiles use the same application-style tile height. This keeps switching
between Applications and Places visually stable across docked, centered, and
full-screen layouts.

### Calculator

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `extras/calculator-engine` | string | `none` | `none`, `bc`, `qalc`, or `gcalccmd`. |
| `extras/calculator-result-font-size` | int | -1 | -1 = Auto; 0 through 6 select Very Small through Very Large. |
| `extras/calculator-max-decimal-places` | int | 4 | Maximum fractional digits, from 0 through 10. |

The effective built-in defaults differ by preset: Classic disables Calculator;
Modern, Full Screen, and Minimal select `bc`. All use Auto and four decimal
places. Invalid stored values recover to the safe defaults instead of selecting
an arbitrary command or clamping to a different preference.

### Sidebar

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `sidebar-enabled` | bool | true | Show the category sidebar. When false the sidebar is removed and, with Places on, the Apps/Places switch moves into the search-bar row. |
| `category-show-name` | bool | true | Show category label text. |
| `category-icon-size` | int | -1 | Category icon size: `-1` uses the theme size; `0` through `6` select named sizes. |
| `hover-switch-category` | bool | false | Switch category on hover instead of click. |
| `sort-categories` | bool | true | Sort categories alphabetically. |
| `default-category` | int | 0 | 0 = favorites, 1 = recent, 2 = all. |
| `recent-items-max` | int | 10 | Maximum recent app count. |
| `favorites-in-recent` | bool | false | Include favorites in the Recent list. |

### Search

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `search/fuzzy-enabled` | bool | true | Enable fuzzy (typo-tolerant) matching. |
| `search/fuzzy-threshold` | int | 0 | Fuzzy sensitivity (0 = automatic). |
| `search/favorites-boost-enabled` | bool | true | Boost frequently-used apps in results. |
| `search/favorites-boost-level` | int | 2 | Boost strength: 1 = low, 2 = medium, 3 = high. |
| `search/frecency-alpha` | int | 70 | Recency weight in the ranking score (0–100). |

### Places

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `places/enabled` | bool | false | Enable the Places pane. |
| `places/switch-show-icons` | bool | false | Render the Apps/Places switch as icon buttons instead of text. Forced on (render-time only) when the sidebar is on top/bottom or disabled. |
| `places/switch-button-shape` | string | `gtk-theme` | Apps/Places switch shape: `gtk-theme` uses normal GTK theme button radii, `rounded` uses MeowMenu's rounded segmented shape. |
| `places/history-enabled` | bool | true | Show recently opened files. |
| `places/favourites-enabled` | bool | true | Show bookmarks. |
| `places/favourite-sync` | string | `meowmenu` | Keep bookmarks in sync with `meowmenu` or `thunar`. |
| `places/max-items` | int | 20 | Maximum items in the Places view. |
| `places/remember-last-mode` | bool | false | Reopen in the last-used Places sub-section. |
