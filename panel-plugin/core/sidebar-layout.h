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

// Supported stored positions plus derived physical edges for Horizontal.
enum class SidebarPosition
{
	Left,
	Right,
	Horizontal,
	Top,
	Bottom
};

// Resolved placement of the Apps/Places mode switch for a layout pass.
enum class SwitchLocation
{
	InSidebar,      // packed with the category list (vertical sidebar or strip)
	InSecondaryRow, // shared Docked/Centered secondary row with Session actions
	InSearchBar,    // relocated into the search-bar row (sidebar disabled)
	None            // Places mode off — no switch at all
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

/* ModernDividerState:
 *
 * The resolved, display-independent inputs for the contextual Modern upper
 * divider. vertical_sidebar_switch is true only when the selector is visible
 * in a left/right sidebar, never in a top/bottom strip or search-bar row.
 */
struct ModernDividerState
{
	bool modern_preset;
	bool docked_or_centered;
	bool vertical_sidebar_switch;
	bool profile_visible;
	unsigned int visible_command_count;
};

/* meow_modern_divider_visible:
 * @state: resolved Modern identity, layout, selector placement, and upper-row
 *         content availability for the current layout pass.
 *
 * Determines whether the contextual upper separator belongs before the
 * Apps/Places selector. This stays independent of GTK and settings so all
 * presentation paths share one exhaustive visibility rule.
 *
 * Returns: true when the separator is visible and must be packed; false when
 *          it must be hidden without reserving space.
 */
bool meow_modern_divider_visible(const ModernDividerState& state);

/* meow_parse_sidebar_position:
 * @value: a stored /sidebar-position string; may be NULL.
 *
 * Maps left, right, and horizontal to the supported positions. Any other value
 * falls back to Left without mutating storage.
 *
 * Returns: the matching SidebarPosition (Left for unknown/NULL/"hidden").
 */
SidebarPosition meow_parse_sidebar_position(const char* value);

/* meow_resolve_sidebar_edge:
 * @position: stored supported or tolerated legacy sidebar position.
 * @search_bar_bottom: true when the windowed primary row is at the bottom.
 * @fullscreen: true for the fixed Full Screen composition.
 *
 * Converts the single Horizontal choice to its physical edge. Windowed menus
 * place it opposite the primary row; Full Screen always places it below
 * Results. Explicit Left and Right pass through unchanged.
 *
 * Returns: Top or Bottom for Horizontal, otherwise @position.
 */
SidebarPosition meow_resolve_sidebar_edge(SidebarPosition position,
		bool search_bar_bottom, bool fullscreen);

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
 * and that its width tracks the Full Screen main column. The Apps/Places toggle
 * anchors to the leading edge and the category-icon group to the trailing edge,
 * with the slack between them; width_from_main_column is an invariant (always
 * true). The fields are surfaced so the unit test pins them against drift.
 */
struct StripGeometry
{
	StripOrder order;
	StripAnchor toggle_anchor;      // always Leading — toggle pinned to the row's leading edge
	StripAnchor categories_anchor;  // always Trailing — categories pinned to the trailing edge
	bool width_from_main_column;    // always true — results/application column
};

struct FullscreenMainColumn
{
	int width;
	int margin;
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
 * categories_anchor always Trailing, and width_from_main_column always true.
 */
StripGeometry meow_compute_strip_geometry(SidebarPosition position, bool ltr);

/* meow_fullscreen_main_column:
 * @workarea_width: current monitor workarea width in pixels.
 *
 * Computes the shared Full Screen main-column metrics used by the search bar,
 * Top/Bottom horizontal sidebar strip, and results/application grid. The
 * results grid is the width authority; the other rows reuse these metrics so
 * they do not drift wider or narrower than it.
 *
 * Returns: width and symmetric side margin in pixels; both 0 for non-positive
 *          workarea widths.
 */
FullscreenMainColumn meow_fullscreen_main_column(int workarea_width);

/* meow_toggle_icon_px:
 * @location: where the Apps/Places toggle is rendered this pass.
 * @category_px: the configured category icon pixel size (sidebar source).
 * @search_bar_px: the measured search-bar-height-derived pixel size.
 * @session_px: the effective Session icon allocation from the current theme.
 *
 * Pure decision for the toggle icon's pixel size: the toggle always inherits
 * from the region that contains it — the category icon size when it lives in a
 * sidebar (vertical or strip), the Session icon allocation in a shared
 * secondary row, and the search-bar height when it lives in the search-bar row.
 * There is deliberately no independent user setting.
 *
 * Returns: the pixel size to apply with gtk_image_set_pixel_size(); 0 when the
 * toggle is hidden (SwitchLocation::None), meaning no size is applied.
 */
int meow_toggle_icon_px(SwitchLocation location, int category_px, int search_bar_px,
		int session_px);

/* meow_toggle_button_height_px:
 * @location: where the Apps/Places toggle is rendered this pass.
 * @categories_horizontal: true for the top/bottom icon-only sidebar strip.
 * @category_px: the configured category icon pixel size.
 *
 * Pure decision for an explicit switch-button height. Only the horizontal
 * sidebar strip pins the buttons to the sidebar icon size; other placements keep
 * their theme/natural button allocation. Width is deliberately not constrained.
 *
 * Returns: button height in pixels, or -1 to clear the height request.
 */
int meow_toggle_button_height_px(SwitchLocation location, bool categories_horizontal,
		int category_px);

/* meow_strip_spacer_order:
 * @categories_horizontal: true when the category box is the top/bottom strip.
 *
 * The horizontal strip keeps one expanding spacer before all category buttons.
 * Reapplying this order on every layout pass keeps built-in category buttons
 * grouped with dynamic categories after hover, mode switches, close, and reopen.
 *
 * Returns: child index for the spacer, or -1 when no strip spacer is active.
 */
int meow_strip_spacer_order(bool categories_horizontal);

/* meow_default_category_order_base:
 * @strip_spacer_visible: true when the horizontal strip spacer is visible.
 * @vertical_switch_controls: true when the vertical sidebar owns its upper
 *                            separator, Apps/Places selector, and lower
 *                            separator.
 *
 * Default-category reordering must leave fixed leading children ahead of the
 * built-in category buttons. The strip keeps its spacer at index 0; a vertical
 * sidebar keeps its upper separator, selector, and lower separator at 0..2.
 *
 * Returns: the first child index available for default-category buttons.
 */
int meow_default_category_order_base(bool strip_spacer_visible,
		bool vertical_switch_controls);

/* meow_category_label_visible:
 * @category_show_name: the stored "show category names" intent.
 * @horizontal: whether the sidebar is a horizontal Top/Bottom strip.
 *
 * The single label-visibility decision shared by every sidebar button — Apps
 * category buttons and Places section buttons alike — so names appear or hide
 * identically in both modes (supported behavior). A horizontal strip is always
 * icon-only regardless of the stored intent.
 *
 * Returns: true when sidebar buttons should show their text label.
 */
bool meow_category_label_visible(bool category_show_name, bool horizontal);

// The fixed maximum sidebar category-label width, in characters. A single
// mode-agnostic cap (INV-3): every CategoryButton — Apps category and Places
// section alike — ellipsises only when its label is longer than this. The
// sidebar width floor itself is carried by pinning every button's minimum
// label width (in pixels) to the widest measured label across both modes; the
// cap only bounds how much of an over-long label is measured, so one very long
// entry cannot push the floor without bound.
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

/* meow_sidebar_max_label_width:
 * @widths: array of every sidebar button's measured natural label width in
 *   pixels (Apps category buttons and Places section buttons together), each
 *   already capped at measurement time.
 * @count: number of entries in @widths; may be 0.
 *
 * The single minimum label width that every sidebar button is pinned to: the
 * widest measured label across both modes. Pinning all buttons to this value
 * keeps the sidebar width on the *visible* buttons, so an Apps<->Places switch
 * (which only toggles visibility) cannot collapse it. This is deliberately
 * independent of the size group's hidden-widget accounting, which GTK does not
 * negotiate reliably across re-layouts.
 *
 * Returns: the per-button minimum label width in pixels; 0 when @count is 0.
 */
int meow_sidebar_max_label_width(const int* widths, int count);

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
