---
title: Configuration
nav_order: 4
has_children: false
---

# Configuration

All MeowMenu settings are exposed in the **Properties** dialog
(right-click the panel button → **Properties**) and stored in Xfconf.

## Properties dialog

### General

| Option | Description |
|--------|-------------|
| Preset | Select, save, rename, delete, export, or import a layout preset. |
| Layout mode | **Docked** (standard panel-attached window) or **FullScreen** (full-screen launcher). |
| Menu width | Width of the menu window in pixels. |
| Menu height | Height of the menu window in pixels. |
| Full-screen opacity | Opacity of the entire full-screen menu, including the results area (0–100, where 0 is fully transparent and 100 fully solid), applied live. |
| Stay visible when focus is lost | Keep the menu open when another window receives focus. |
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
| Profile position | Where the profile block appears: top, bottom, or hidden. **Hidden** removes the avatar and username (and keeps them out of keyboard focus). |
| Session commands position | Where the lock/logout/suspend buttons appear: top-right, bottom-right, or hidden. **Hidden** removes the session buttons. When both Profile and Session commands are hidden, the whole row collapses (the shared search row is kept in Full Screen). |
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
| Position | Place the sidebar on the **left**, **right**, **top**, or **bottom**. Top and bottom turn the categories into a horizontal, icon-only strip. The strip is centred, as wide as the search box, and surrounded by equal gaps above and below; **Top** places it just below the search bar and above the results, **Bottom** places it below the results. It scrolls sideways when it has more icons than fit, and "Show category name" is unavailable there. The same geometry holds in Full Screen, where the centred results box keeps its symmetric side margins. |
| Show category name | Display the category label next to its icon. On a left/right sidebar, hiding the names also makes the Apps/Places switch vertical so the sidebar can stay narrow. |
| Category icon size | Size of category icons (-2 = inherit from theme). |
| Sort categories | Sort the category list alphabetically. |
| Recent items max | Maximum number of recently-used apps to track. |
| Include favorites in recent | Also show favorited apps in the Recent category. |
| Unified bar | Render profile, search, and session controls on one horizontal row (FullScreen mode only). |

### Places

| Option | Description |
|--------|-------------|
| Enable Places | Show a file/folder browser pane in the menu. |
| Show icons | Render the Apps/Places switch as two icon buttons (an app-grid icon and a folder icon, with tooltips) instead of text labels. Forced on, and shown greyed in Preferences, when the sidebar is on top/bottom or disabled. |
| Show recent files | Include recently opened files in the Places view. |
| Show bookmarks | Include user bookmarks (from Thunar or GTK bookmarks). |
| Bookmark sync | Keep the Places bookmarks in sync with **MeowMenu** or **Thunar**. |
| Max items | Maximum number of items shown in the Places view. |
| Remember last mode | Reopen MeowMenu showing the last-used Places sub-section. |

#### Missing recent files and bookmarks

When a recent item or bookmark points to a file or folder that has been deleted
or renamed, MeowMenu shows it greyed-out and its tooltip notes that the target
is missing. A greyed-out entry cannot be opened directly — clicking it does
nothing — but its right-click menu still offers **Open Containing Folder**,
which opens the parent directory so a renamed file can be located. Missing
bookmarks can also be removed with **Remove from Favourites**.

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
| `corner-radius` | int | 0 | Menu window corner radius in pixels. |
| `panel-gap` | int | 0 | Gap between the panel and the menu window. |
| `menu-width` | int | 450 | Menu window width in pixels. |
| `menu-height` | int | 500 | Menu window height in pixels. |
| `categories-opacity` | int | 100 | Sidebar opacity in docked mode (0 = fully transparent, 100 = fully solid). |
| `apps-opacity` | int | 100 | Results area opacity in docked mode (0 = fully transparent, 100 = fully solid). |
| `full-screen-opacity` | int | 100 | Opacity of the whole full-screen menu, results area included (0 = fully transparent, 100 = fully solid), applied live. |
| `stay-on-focus-out` | bool | false | Keep menu open when focus moves away. |

### Layout

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `layout-mode` | string | `docked` | `docked` or `fullscreen`. |
| `sidebar-position` | string | `left` | `left`, `right`, `top`, or `bottom`. (A legacy `hidden` value is migrated to `sidebar-enabled = false`.) |
| `search-bar-position` | string | `top` | `top` or `bottom`. |
| `profile-position` | string | `top` | `top`, `bottom`, or `hidden`. (A legacy `bottom-right` value is migrated to `bottom`.) |
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

### Sidebar

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `sidebar-enabled` | bool | true | Show the category sidebar. When false the sidebar is removed and, with Places on, the Apps/Places switch moves into the search-bar row. |
| `category-show-name` | bool | true | Show category label text. |
| `category-icon-size` | int | -2 | Category icon size (-2 = theme default). |
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
| `places/history-enabled` | bool | true | Show recently opened files. |
| `places/favourites-enabled` | bool | true | Show bookmarks. |
| `places/favourite-sync` | string | `meowmenu` | Keep bookmarks in sync with `meowmenu` or `thunar`. |
| `places/max-items` | int | 20 | Maximum items in the Places view. |
| `places/remember-last-mode` | bool | false | Reopen in the last-used Places sub-section. |
