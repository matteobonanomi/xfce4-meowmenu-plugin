---
title: Configuration
nav_order: 4
has_children: false
---

# Configuration

Right-click the MeowMenu panel button and choose **Properties**. Changes apply
immediately and are saved in Xfconf.

If you want a quick starting point, choose one of the four built-in presets.
See [Presets](presets) for screenshots and a short description of each one.

## General

| Option | What it does |
|--------|--------------|
| Preset | Select or manage a layout preset. |
| Layout mode | Choose Docked, Centered, or Full Screen. |
| Panel gap | Set the space between a Docked menu and the panel. |
| Menu width and height | Set the size of a Docked or Centered menu. |
| Corner radius | Round the corners of a Docked or Centered menu. |
| Menu opacity | Set the background opacity. This requires a compositor. |
| Stay visible when focus is lost | Keep the menu open when another window receives focus. |

### Layout modes

| Mode | Description |
|------|-------------|
| Docked | Opens next to the panel. |
| Centered | Opens in the centre of the panel's monitor. |
| Full Screen | Fills the screen and keeps the search bar at the top. |

Docked and Centered menus can be resized by dragging an edge or corner. The new
size is saved when you release the mouse button. Full Screen uses the current
screen size instead.

## Panel button

| Option | What it does |
|--------|--------------|
| Show panel button title | Display a label beside the panel icon. |
| Panel button title | Set the label. Pango markup is supported. |
| Show panel button icon | Show or hide the icon. |
| Panel button icon | Choose the icon. |
| Use a single panel row | Keep the button on one row in a multi-row panel. |

## Search bar

| Option | What it does |
|--------|--------------|
| Search bar position | Put the main row at the top or bottom of a Docked or Centered menu. |
| Show profile | Show the current avatar and username. |
| Avatar shape | Use a round or square avatar. |
| Placeholder text | Change the hint shown in an empty search field. |
| Fuzzy search | Find close matches when a query contains a typo. |
| Fuzzy threshold | Adjust fuzzy-match sensitivity. `0` uses the automatic value. |
| Favorites boost | Give frequently used applications a higher search rank. |
| Favorites boost level | Choose a low, medium, or high boost. |
| Search actions | Add keyword-triggered commands. |

Search-action templates support these placeholders:

- `%s` inserts the text after the matched prefix.
- `%S` inserts the complete query.
- `%u` inserts URI-escaped text as one argument.
- `%%` inserts a percent sign.

Commands run directly, without a shell. Use `%u` when the query must remain one
argument.

## Results

| Option | What it does |
|--------|--------------|
| View mode | Show results as a list, icon grid, or category tree. |
| Show app name | Display application names. |
| Show app description | Display application descriptions. |
| Show tooltip | Show the full description on hover. |
| Icon size | Set the result icon size. `-1` follows the theme. |
| Grid density | Choose how many columns the icon grid uses. |
| Transparent grid | Blend idle grid tiles into the results background. |
| Default category | Open on Favourites, Recently Used, or All Applications. |
| Hover to switch category | Open a category when the pointer moves over it. |

For keyboard controls, see [Keyboard navigation](keyboard-navigation).

## Sidebar

| Option | What it does |
|--------|--------------|
| Enable sidebar | Show or hide category navigation. |
| Position | Put the sidebar on the left, right, or in a horizontal strip. |
| Show category name | Show labels beside category icons. |
| Category icon size | Set the icon size. `-1` follows the theme. |
| Sort categories | Sort categories alphabetically. |
| Recent items max | Limit the number of recently used applications. |
| Include favorites in recent | Include favourite applications in Recently Used. |

When the sidebar is hidden, MeowMenu keeps the search and results available and
moves other controls into the remaining space.

## Places

Places provides shortcuts to Home, standard user folders, recent files, and
bookmarks. It opens folders and files in your normal file manager or application;
it is not an embedded file browser.

| Option | What it does |
|--------|--------------|
| Enable Places | Add the Applications and Places switch. |
| Show icons | Use icons instead of text in the switch. |
| Show recent files | Show recently opened files in History. |
| Show bookmarks | Show saved folder and file bookmarks. |
| Bookmark sync | Keep bookmarks in MeowMenu or synchronize them with Thunar. |
| Max items | Limit the number of items shown. |
| Remember last mode | Reopen in the last-used Applications or Places mode. |

Missing files and folders cannot be opened. MeowMenu hides missing bookmarks it
manages itself. Bookmarks managed by another application are left unchanged.

## Session controls

| Option | What it does |
|--------|--------------|
| Show session controls | Show available lock, log out, suspend, and power actions. |
| Show confirmation dialog | Ask before running a session action. |
| Session commands | Choose the icon, label, command, and confirmation behavior for each action. |

Unavailable actions are hidden automatically.

## Calculator

Open **Extras → Calculator** to enable inline calculations. Select an installed
engine, then enter an expression such as `2 + 2`. Activating the result copies
its full value.

Choose **None** to disable the Calculator. Missing engines are marked in the
list. The engine packages and installation options are described in the
[installation guide](installation#optional-calculator-engine).

You can also choose the result text size and set between 0 and 10 decimal
places. Calculator choices are included in presets.

## Drag and drop

- Drag an application onto **Favourites** to add it.
- Drag a Place onto **Favourites** when bookmarks are managed by MeowMenu.
- In Docked and Centered modes, applications and Places can be dragged to the
  desktop or an accepting file manager.
- In Full Screen, use **Add to Desktop** or **Add Desktop Link** from the item's
  context menu.

## Advanced Xfconf reference

Most users should change settings through **Properties**. For scripting, keys
are stored below `/plugins/<id>/` in the `xfce4-panel` channel.

```bash
# Find the MeowMenu plugin ID and list its settings
xfconf-query -c xfce4-panel -lv | grep meowmenu

# Change one setting
xfconf-query -c xfce4-panel -p /plugins/<id>/<key> -s <value>
```

| Key | Type | Default |
|-----|------|---------|
| `corner-radius` | int | `0` |
| `panel-gap` | int | `0` |
| `menu-width` | int | `450` |
| `menu-height` | int | `500` |
| `menu-opacity` | int | `100` |
| `stay-on-focus-out` | bool | `false` |
| `layout-mode` | string | `docked` |
| `sidebar-position` | string | `left` |
| `search-bar-position` | string | `top` |
| `show-profile` | bool | `true` |
| `show-session` | bool | `true` |
| `view-mode` | int | `1` |
| `launcher-show-name` | bool | `true` |
| `launcher-show-description` | bool | `true` |
| `launcher-show-tooltip` | bool | `true` |
| `launcher-icon-size` | int | `-1` |
| `grid-density` | string | `medium` |
| `transparent-grid` | bool | `false` |
| `sidebar-enabled` | bool | `true` |
| `category-show-name` | bool | `true` |
| `category-icon-size` | int | `-1` |
| `hover-switch-category` | bool | `false` |
| `sort-categories` | bool | `true` |
| `default-category` | int | `0` |
| `recent-items-max` | int | `10` |
| `favorites-in-recent` | bool | `false` |
| `search/fuzzy-enabled` | bool | `true` |
| `search/fuzzy-threshold` | int | `0` |
| `search/favorites-boost-enabled` | bool | `true` |
| `search/favorites-boost-level` | int | `2` |
| `search/frecency-alpha` | int | `70` |
| `places/enabled` | bool | `false` |
| `places/switch-show-icons` | bool | `false` |
| `places/history-enabled` | bool | `true` |
| `places/favourites-enabled` | bool | `true` |
| `places/favourite-sync` | string | `meowmenu` |
| `places/max-items` | int | `20` |
| `places/remember-last-mode` | bool | `false` |
| `extras/calculator-engine` | string | `none` |
| `extras/calculator-result-font-size` | int | `-1` |
| `extras/calculator-max-decimal-places` | int | `4` |

X11 is the primary supported session. Wayland support and its current fallback
behavior are summarized in [Known limitations](known-limitations).
