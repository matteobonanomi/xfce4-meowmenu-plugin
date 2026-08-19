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

### Resizing the menu

Docked and Centered menus provide handles on all four sides and four corners.
The selected edge follows the pointer while the relevant opposite edge stays
anchored; corner handles resize both axes independently. This behavior applies
to Classic and Modern presets and to list, tree, and icon-grid views, including
when the contents rearrange during a drag.

On X11, resize geometry is designed to remain independent of compositing and
rendering speed. A constrained system may display fewer intermediate frames,
but a single continuous drag still reaches the final pointer-selected size
without a compensating drag. Releasing the primary button saves the completed
normal width and height, so closing and reopening restores that size.
Full-screen dimensions are not saved as the normal menu size.

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

### Docked and Centered composition

Docked and Centered use the same menu composition. Profile and Search share
one primary row at the selected Top or Bottom edge. The avatar and username are
one Profile block: hiding Profile removes both from allocation and keyboard
navigation. With a vertical sidebar, Profile matches the sidebar column and
Search matches the Results column. A right sidebar puts Profile on the right;
without a vertical sidebar, Profile uses the logical leading side.

Apps/Places and available Session actions use a derived secondary row:

| Sidebar | Available Session actions | Places | Apps/Places | Secondary row |
|---------|---------------------------|--------|-------------|---------------|
| Left/Right | One or more | On | Physical sidebar side of the secondary row | Visible on the opposite physical edge |
| Left/Right | One or more | Off | Hidden | Visible on the physical edge opposite the sidebar |
| Left/Right | None | On | Secondary-row sidebar-side role | Visible |
| Left/Right | None | Off | Hidden | Hidden |
| Horizontal | One or more | On | Logical leading side of the secondary row | Visible with Session at logical trailing |
| Horizontal | One or more | Off | Hidden | Visible with Session at logical trailing |
| Horizontal | None | On | Secondary row when Profile is visible; primary Search row otherwise | Visible when Profile is visible; hidden otherwise |
| Horizontal | None | Off | Hidden | Hidden |
| Disabled | One or more | On | Logical leading side of the bottommost row | Visible with Session at logical trailing |
| Disabled | One or more | Off | Hidden | Visible with Session at logical trailing |
| Disabled | None | On | Secondary row when Profile is visible; primary Search row otherwise | Visible when Profile is visible; hidden otherwise |
| Disabled | None | Off | Hidden | Hidden |

A configured Session group counts as available only when at least one enabled
action can currently run. An empty group reserves no row or focus target.
Without a vertical sidebar, the visible Session action group uses the logical
trailing edge of its Results region and mirrors in right-to-left interfaces.
With a vertical sidebar, it uses the opposite physical edge. When Apps/Places
shares a secondary row with Session, their icons use the same effective size
while the selector keeps its natural height and is vertically centred rather
than stretching to the height of Session controls.

When Places is enabled without effective Session actions, Apps/Places remains
in the secondary row whenever Profile or a vertical sidebar is visible. With a
vertical sidebar it uses the sidebar-side width; otherwise it uses the logical
leading edge. Only when Profile, the vertical sidebar, and Session are all hidden does
it share the primary Search row, as in Minimal. Horizontal follows
the same no-vertical-sidebar rule.

In Docked and Centered layouts, a vertical sidebar also determines the physical
edges of a shared secondary row: Apps/Places stays on the sidebar's side and
effective Session actions use the opposite side. Thus a Right sidebar places
the selector on the right and Session actions on the left. Profile's avatar and
name use the category button's theme border and padding, so their leading edge
matches the category icon column. With category names hidden, the complete
avatar/name group and the category icons are centred on the same sidebar axis;
if Profile is widest, it determines the sidebar width with equal tolerance on
both sides. The initial width also reserves the theme's vertical scrollbar, so
category overflow or hover does not widen the launcher. Applications mode
opens at the top of the category viewport with Favorites, enabled Recently
Used, and All Applications painted on the first frame. These rules apply only
to Docked and Centered layouts; Full Screen keeps its established selector,
Session, and sidebar sizing arrangement.

Horizontal is one sidebar choice for category navigation only. In Docked and
Centered it appears on the Results edge opposite the primary row; when the
secondary row is present, that row sits farther outward. Apps/Places never
belongs to the Horizontal strip: it follows the same logical row relocation as
a disabled sidebar, including the unified Search-row placement in Minimal-style
compositions. When the category group fits, it is centered across the complete
menu width in Docked and Centered; in Full Screen it is centered within the
Results-width strip. Overflow scrolls inside that width without enlarging the
menu. Logical leading/trailing order and keyboard traversal mirror in
right-to-left interfaces, while explicit Left and Right sidebars stay on their
selected physical sides. Every hidden or unavailable control is removed from
allocation, shared-width sizing, and focus navigation.

Docked and Centered menus use two related GTK-theme surfaces. Search and
Results form the content surface; visible category navigation and the
secondary Apps/Places and Session row form the chrome surface. Profile follows
the surface of the column it occupies. The distinction comes from the active
theme rather than the selected preset, so changing themes updates both
surfaces while the menu is open. A thin theme-derived line separates a visible
secondary row from Content. When a Horizontal strip and secondary row are both
visible, an equal thin line also separates those two Chrome bands. These lines
do not add spacing; the controls remain vertically centred between the existing
boundaries and launcher edge.

Outer insets and gaps between major regions use one spacing rhythm derived
from the active GTK theme. Hiding a region removes its gap as well as its
allocation, avoiding doubled spacing and empty seams. Results and the
scrollbar trough have no persistent inner frame; result selection, keyboard
focus, and the scrollbar slider keep their normal theme feedback. While
scrolling, Results icons and labels remain clipped inside the Results box and
never paint over the Horizontal strip or secondary row.

### Full Screen composition

Full Screen always uses one primary row at the top. Profile occupies the
logical outer leading edge and available Session actions occupy the logical
outer trailing edge. Search stays in a centred middle region whose width and
edges exactly match the Results box.

Search remains visible with a positive usable allocation for every Full Screen
sidebar choice. A visible vertical sidebar owns category navigation and
Apps/Places; a Horizontal strip owns category navigation only. Neither replaces
or hides Search.

When the sidebar is disabled or Horizontal and Places is enabled, Apps/Places
and Search share the fixed results-width middle region in Full Screen:
the switch is logical-leading, Search is logical-trailing, and a visible gap
separates them without overflow. In Docked and Centered, Apps/Places moves to
the primary Search row only when Profile and Session are also hidden, as in
Minimal; otherwise it remains in the windowed secondary row. With a visible
vertical sidebar, Full Screen keeps Apps/Places attached to that sidebar while
Search remains in the same centered middle region. The Horizontal strip keeps
category navigation separate and never owns the selector. There is no separate
bottom-control variant.

The complete logical no-sidebar order is Profile, Apps/Places, Search,
Session. It mirrors naturally in right-to-left interfaces, including the
outer anchors and the two edges of the centred middle region. Hidden Profile,
unavailable Session actions, and disabled Places reserve neither space nor a
keyboard focus target.

Unlike Docked and Centered, Full Screen deliberately uses one uniform surface
for Profile, Search, Results, categories, and Session controls. Theme changes
and menu opacity still apply uniformly without introducing windowed surface
boundaries.

### Search Bar

| Option | Description |
|--------|-------------|
| Search bar position | Place the primary row at the **top** or **bottom** of Docked and Centered menus. Full Screen always uses the top. |
| Show profile | Show the avatar and username as one block in the primary row. |
| Avatar shape | Use a round or square avatar. Available when **Show profile** is on. |
| Placeholder text | The hint text shown when the search field is empty. |
| Fuzzy search | Enable approximate (typo-tolerant) matching. |
| Fuzzy threshold | Sensitivity of the fuzzy matcher (0 = automatic). |
| Favorites boost | Rank previously-used apps higher in search results. |
| Favorites boost level | Strength of the boost: low, medium, or high. |
| Search actions | Custom keyword-triggered commands (e.g. type `!` to run a shell command). |

### Session

| Option | Description |
|--------|-------------|
| Show session controls | Show the configured session actions when at least one action is available. |
| Show confirmation dialog | Ask for confirmation before a session action runs. |
| Session commands | Configure the icon, label, executable, visibility, and confirmation behavior for each session action. |

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
| Transparent grid | Blend idle grid tiles into the results background. Clicking empty grid space clears the tile selection without leaving an unrelated solid tile; theme-provided hover, selection, press, drag, and keyboard-focus feedback remains visible. |
| Default category | Category shown on open: favorites, recent, or all apps. |
| Hover to switch category | Change the visible category by hovering over sidebar entries. |

### Sidebar

| Option | Description |
|--------|-------------|
| Enable sidebar | Turn the category sidebar on or off. When off, the menu shows no sidebar; if Places is enabled the Apps/Places switch stays in the secondary row unless Profile and Session are also hidden, in which case it shares the Search row. The results view gains a heading naming the default category (FAVORITES, RECENTLY USED, or ALL APPLICATIONS). |
| Position | Place the sidebar on the **left**, **right**, or in a **Horizontal** strip. In Docked and Centered, Horizontal appears on the Results edge opposite the primary row; in Full Screen it appears below Results. The strip contains category navigation only; a fitting category group is centered across the full menu in Docked/Centered or the Results-width column in Full Screen. Apps/Places remains in the same row home used when the sidebar is disabled. The strip scrolls when needed without widening the menu, and **Show category name** is unavailable. |
| Show category name | Display the category label next to its icon. On a left/right sidebar, hiding the names also makes the Apps/Places switch vertical so the sidebar can stay narrow. |
| Category icon size | Size of category icons (`-1` through `6`; `-1` inherits the theme size). |
| Sort categories | Sort the category list alphabetically. |
| Recent items max | Maximum number of recently used applications to track for the Applications **Recently Used** category. |
| Include favorites in recent | Also show favorited apps in the Recent category. |
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
| Enable Places | Show a flat shortcut surface for Home, configured standard user folders, recent files, and bookmarks. |
| Show icons | Render the Apps/Places switch as two icon buttons (an app-grid icon and a folder icon, with tooltips) instead of text labels. Forced on, and shown greyed in Properties, when the sidebar is Horizontal or disabled. |
| Show recent files | Include recently opened files in Places **History**. This is separate from the Applications **Recently Used** category. |
| Show bookmarks | Include user bookmarks (from Thunar or GTK bookmarks). |
| Bookmark sync | Keep the Places bookmarks in sync with **MeowMenu** or **Thunar**. |
| Max items | Maximum number of items shown in the Places view. |
| Remember last mode | Reopen MeowMenu in the last-used top-level mode: Applications or Places. Entering Places starts on Home. |

Apps/Places is always shown as two flat, trackless choices. Exactly one mode
is selected, using the active GTK theme's checked state; hover, press, focus,
and disabled feedback remain theme-native. Icons follow the size of their
current host region, while text labels use their natural height so they remain
readable with enlarged text.

With Places active in a left or right sidebar, **Home**, **History**, and
**Favourites** start at the same top edge used by application categories. Any
unused sidebar space remains below them. A divider appears only when another
visible group follows, so no trailing line is shown below Favourites. The three
icons keep one aligned column, with the star optically balanced against the
Home and History artwork.

Places **Home** always starts with Home, followed by configured standard user
folders such as Documents, Downloads, Music, Pictures, and Videos when those
directories exist. It does not list every folder under Home and is not an
embedded file browser: activating a shortcut opens the external file manager.
Missing, Home-equivalent, and duplicate standard-folder entries are omitted.

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
For keyboard use, a visible successful Calculator result is the first visual
anchor ahead of ordinary matches. If a current calculation finishes after the
ordinary results, the query is re-anchored to Calculator; stale or cancelled
calculations do not change focus.

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
| `sidebar-position` | string | `left` | `left`, `right`, or `horizontal`. |
| `search-bar-position` | string | `top` | `top` or `bottom`. |
| `show-profile` | bool | true | Show the avatar and username in the primary row. |
| `show-session` | bool | true | Show available session actions. |

### Results view

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `view-mode` | int | 1 | 0 = icons, 1 = list, 2 = tree. |
| `launcher-show-name` | bool | true | Show the app name. |
| `launcher-show-description` | bool | true | Show the app description. |
| `launcher-show-tooltip` | bool | true | Show a hover tooltip. |
| `launcher-icon-size` | int | -1 | Icon size (-1 = theme default). |
| `grid-density` | string | `medium` | `low`, `medium`, or `high` columns in grid mode. |
| `transparent-grid` | bool | false | In grid mode, make resting result tiles blend into the results area. Empty-space clicks clear the selection without leaving a solid idle tile, while theme-provided hover, selection, press, drag, and keyboard-focus feedback remains visible. |

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
| `sidebar-enabled` | bool | true | Show the category sidebar. When false the sidebar is removed; with Places on, Apps/Places follows the derived secondary-row or unified Search-row placement. |
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
| `places/enabled` | bool | false | Enable the Places shortcut surface. |
| `places/switch-show-icons` | bool | false | Render the Apps/Places switch as icon buttons instead of text. Forced on (render-time only) when the sidebar is Horizontal or disabled. |
| `places/history-enabled` | bool | true | Show recently opened files in Places History. |
| `places/favourites-enabled` | bool | true | Show bookmarks. |
| `places/favourite-sync` | string | `meowmenu` | Keep bookmarks in sync with `meowmenu` or `thunar`. |
| `places/max-items` | int | 20 | Maximum items in the Places view. |
| `places/remember-last-mode` | bool | false | Reopen in the last-used top-level mode, Applications or Places. |

X11 is the primary quality path. Wayland support for menu mode, Places, and
Transparent grid is experimental; the menu remains usable, but theme- or
compositor-specific pixel differences may occur.
