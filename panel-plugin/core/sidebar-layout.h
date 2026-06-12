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

// Direction-relative anchoring of a group within the horizontal strip. Leading
// maps to GTK START and Trailing to GTK END, so RTL is handled for free: the
// leading edge is the left in LTR and the right in RTL.
enum class StripAnchor
{
	Leading,
	Trailing
};

/* StripGeometry:
 *
 * The render-time geometry of a docked Top/Bottom category strip: where it sits
 * relative to the results box, where its two groups anchor on the single row,
 * and that its width tracks the search box. The Apps/Places toggle anchors to
 * the leading edge and the category-icon group to the trailing edge, with the
 * slack between them; width_from_search_box is an invariant (always true). The
 * fields are surfaced so the unit test pins them against drift.
 */
struct StripGeometry
{
	StripOrder order;
	StripAnchor toggle_anchor;      // always Leading — toggle pinned to the row's leading edge
	StripAnchor categories_anchor;  // always Trailing — categories pinned to the trailing edge
	bool width_from_search_box;     // always true — width source is the search box
};

/* meow_compute_strip_geometry:
 * @position: the stored sidebar position (only Top/Bottom produce a strip).
 * @ltr: text direction; passed for completeness — the vertical strip order is
 *       direction-independent (a Top strip is above the results in LTR and RTL),
 *       and the leading/trailing anchors are themselves direction-relative.
 *
 * Pure decision for a Top/Bottom strip's stacking order, row anchoring, and
 * width source. Top places the strip above the results box (it sits below the
 * search bar); Bottom places it below the results box. Left/Right are not strips
 * and default to the Top arrangement (the caller does not render a strip for
 * them).
 *
 * Returns: the resolved StripGeometry; toggle_anchor is always Leading,
 * categories_anchor always Trailing, and width_from_search_box always true.
 */
StripGeometry meow_compute_strip_geometry(SidebarPosition position, bool ltr);

/* meow_toggle_icon_px:
 * @location: where the Apps/Places toggle is rendered this pass.
 * @category_px: the configured category icon pixel size (sidebar source).
 * @search_bar_px: the measured search-bar-height-derived pixel size.
 *
 * Pure decision for the toggle icon's pixel size: the toggle always inherits
 * from the region that contains it — the category icon size when it lives in a
 * sidebar (vertical or strip), the search-bar height when it lives in the
 * search-bar row. There is deliberately no independent value and no fourth
 * state.
 *
 * Returns: the pixel size to apply with gtk_image_set_pixel_size(); 0 when the
 * toggle is hidden (SwitchLocation::None), meaning no size is applied.
 */
int meow_toggle_icon_px(SwitchLocation location, int category_px, int search_bar_px);

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

// The fixed maximum sidebar category-label width, in characters. A single
// mode-agnostic cap (INV-3): every CategoryButton — Apps category and Places
// section alike — ellipsises only when its label is longer than this, so the
// shared horizontal GtkSizeGroup settles on the widest *uncapped* entry across
// both modes. Capping every label would drop each minimum width to ~one glyph
// and, because the group aggregates minimums as a max-of-minimums, collapse the
// non-expanding sidebar to the switch width; leaving sub-cap labels uncapped
// keeps minimum == natural and holds the sidebar at its widest item.
constexpr int MEOW_SIDEBAR_LABEL_MAX_CHARS = 22;

/* CategoryLabelCap:
 *
 * The decision of whether a single sidebar label is capped, plus the
 * max-width-chars value to apply when it is. Pure data — the caller wires the
 * GTK label properties (PANGO_ELLIPSIZE_END, gtk_label_set_max_width_chars).
 */
struct CategoryLabelCap
{
	bool ellipsize;        // apply PANGO_ELLIPSIZE_END to the label
	int  max_width_chars;  // gtk_label_set_max_width_chars() value; only
	                       // meaningful when ellipsize is true
};

/* meow_category_label_cap:
 * @label_chars: the label length in characters (e.g. g_utf8_strlen(text, -1)).
 * @cap_chars: the fixed maximum label width in characters
 *   (MEOW_SIDEBAR_LABEL_MAX_CHARS).
 *
 * The one cap rule shared by every sidebar button in both modes (INV-3): a
 * label is ellipsised only when it is strictly longer than @cap_chars; shorter
 * labels are left at their natural width. The rule depends solely on the label
 * length and the fixed cap — never on the active mode.
 *
 * Returns: a CategoryLabelCap; max_width_chars is @cap_chars and is only to be
 * applied when ellipsize is true.
 */
CategoryLabelCap meow_category_label_cap(long label_chars, int cap_chars);

// Where the embedded Apps/Places switch is anchored relative to the command
// buttons in the standard (non-unified) search-bar row. Direction-relative:
// "before" is the leading side (left in LTR, right in RTL), so RTL is handled
// by GTK packing without a value here.
enum class EmbeddedSwitchSlot
{
	BeforeCommands,   // switch sits before (leading of) a present command box
	Trailing          // no command box shares the row → switch is trailing-most
};

/* meow_embedded_switch_slot:
 * @commands_in_row: true when the command box currently shares the embedded
 *   search-bar row; false when no commands are present in that row.
 *
 * Decides where the embedded Apps/Places switch is anchored in the standard
 * (non-unified) search-bar row. The switch is always to the left of (before)
 * the command buttons in LTR, so the commands stay the trailing-most element;
 * when no commands share the row the switch itself becomes the trailing
 * element. Pure: no GTK, no globals, no I/O. Does not apply to the unified
 * centring cluster (the switch trails the entry there — a separate code path).
 *
 * Returns: BeforeCommands when commands share the row, Trailing otherwise.
 */
EmbeddedSwitchSlot meow_embedded_switch_slot(bool commands_in_row);

} // namespace WhiskerMenu

#endif // MEOWMENU_CORE_SIDEBAR_LAYOUT_H
