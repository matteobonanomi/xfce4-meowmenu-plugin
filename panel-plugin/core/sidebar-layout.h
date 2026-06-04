/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MEOWMENU_CORE_SIDEBAR_LAYOUT_H
#define MEOWMENU_CORE_SIDEBAR_LAYOUT_H

namespace WhiskerMenu
{

// Where the four stored sidebar positions place the category list.
enum class SidebarPosition
{
	Left,
	Right,
	Top,
	Bottom
};

// Resolved placement of the Apps/Places mode switch for a layout pass.
enum class SwitchLocation
{
	InSidebar,    // packed with the category list (vertical sidebar or strip)
	InSearchBar,  // relocated into the search-bar row (sidebar disabled)
	None          // Places mode off — no switch at all
};

enum class SwitchOrientation
{
	Horizontal,
	Vertical
};

/* SidebarLayoutState:
 *
 * The stored user intent plus the environmental inputs that determine how the
 * sidebar and Apps/Places switch are presented. All fields are the *stored*
 * values; forcing rules are applied by meow_compute_sidebar_layout() at render
 * time only, so nothing here is ever mutated to encode a forced state.
 */
struct SidebarLayoutState
{
	bool sidebar_enabled;       // /sidebar-enabled
	SidebarPosition position;   // /sidebar-position
	bool category_show_name;    // /category-show-name (intent)
	bool switch_show_icons;     // /places/switch-show-icons (intent)
	bool search_bar_bottom;     // /search-bar-position == "bottom"
	bool fullscreen;            // /layout-mode == "fullscreen"
	bool places_enabled;        // /places/enabled
};

/* SwitchPresentation:
 *
 * The effective, render-time presentation derived from SidebarLayoutState.
 * The effective_* flags may differ from the corresponding stored intent when a
 * layout forces them (top/bottom strip, sidebar disabled); the stored values
 * are never overwritten, so removing the forcing layout restores them for free.
 */
struct SwitchPresentation
{
	bool sidebar_visible;
	bool categories_horizontal;
	SwitchLocation switch_location;
	SwitchOrientation switch_orientation;
	bool effective_show_icons;          // Apps/Places switch shows icons not text
	bool effective_show_category_names;
	bool show_default_category_heading;
};

/* meow_parse_sidebar_position:
 * @value: a stored /sidebar-position string; may be NULL.
 *
 * Maps the stored string to a SidebarPosition. The legacy "hidden" value and
 * any unrecognised string fall back to Left (a valid position); "hidden" is
 * migrated to the Enable-sidebar switch elsewhere, so it is never honoured as
 * a Position here.
 *
 * Returns: the matching SidebarPosition (Left for unknown/NULL/"hidden").
 */
SidebarPosition meow_parse_sidebar_position(const char* value);

/* meow_compute_sidebar_layout:
 * @state: the stored intent and environment for this layout pass.
 *
 * Pure mapping from stored sidebar/switch intent to the effective presentation,
 * applying every forcing rule (top/bottom strip ⇒ icon-only categories and a
 * horizontal icon switch pinned leading; sidebar disabled ⇒ switch relocated to
 * the search bar, icon-only, with a default-category heading). No GTK calls.
 *
 * Returns: the resolved SwitchPresentation; never modifies @state.
 */
SwitchPresentation meow_compute_sidebar_layout(const SidebarLayoutState& state);

// Vertical stacking of a docked Top/Bottom category strip relative to the
// results box.
enum class StripOrder
{
	StripAboveResults,   // strip is rendered above the results box (Top)
	StripBelowResults    // strip is rendered below the results box (Bottom)
};

/* StripGeometry:
 *
 * The render-time geometry of a docked Top/Bottom category strip: where it sits
 * relative to the results box, that it is horizontally centred, and that its
 * width tracks the search box. centred / width_from_search_box are invariants
 * (always true) — they are surfaced so the unit test pins them against drift.
 */
struct StripGeometry
{
	StripOrder order;
	bool centred;                 // always true — strip is horizontally centred
	bool width_from_search_box;   // always true — width source is the search box
};

/* meow_compute_strip_geometry:
 * @position: the stored sidebar position (only Top/Bottom produce a strip).
 * @ltr: text direction; passed for completeness — the vertical strip order is
 *       direction-independent (a Top strip is above the results in LTR and RTL).
 *
 * Pure decision for a Top/Bottom strip's stacking order and width source. Top
 * places the strip above the results box (it sits below the search bar); Bottom
 * places it below the results box. Left/Right are not strips and default to the
 * Top arrangement (the caller does not render a strip for them).
 *
 * Returns: the resolved StripGeometry; centred and width_from_search_box are
 * always true.
 */
StripGeometry meow_compute_strip_geometry(SidebarPosition position, bool ltr);

/* meow_category_label_visible:
 * @category_show_name: the stored "show category names" intent.
 * @horizontal: whether the sidebar is a horizontal Top/Bottom strip.
 *
 * The single label-visibility decision shared by every sidebar button — Apps
 * category buttons and Places section buttons alike — so names appear or hide
 * identically in both modes (FR-015/016). A horizontal strip is always
 * icon-only regardless of the stored intent.
 *
 * Returns: true when sidebar buttons should show their text label.
 */
bool meow_category_label_visible(bool category_show_name, bool horizontal);

} // namespace WhiskerMenu

#endif // MEOWMENU_CORE_SIDEBAR_LAYOUT_H
