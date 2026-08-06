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

#include "sidebar-layout.h"

#include <cstring>

namespace WhiskerMenu
{

bool meow_modern_divider_visible(const ModernDividerState& state)
{
	// The separator marks a live vertical sidebar boundary. A horizontal strip,
	// embedded switch, inactive Modern identity, or empty upper region has no
	// matching boundary and must not receive an empty visual allocation.
	return state.modern_preset
			&& state.docked_or_centered
			&& state.vertical_sidebar_switch
			&& (state.profile_visible || state.visible_command_count > 0);
}

SidebarPosition meow_parse_sidebar_position(const char* value)
{
	if (value)
	{
		if (std::strcmp(value, "right") == 0)
			return SidebarPosition::Right;
		if (std::strcmp(value, "horizontal") == 0)
			return SidebarPosition::Horizontal;
	}
	// NOTE: malformed or retired values fail closed to a valid supported edge.
	return SidebarPosition::Left;
}

SidebarPosition meow_resolve_sidebar_edge(SidebarPosition position,
		bool search_bar_bottom, bool fullscreen)
{
	if (position != SidebarPosition::Horizontal)
		return position;
	return (fullscreen || !search_bar_bottom)
			? SidebarPosition::Bottom : SidebarPosition::Top;
}

SwitchPresentation meow_compute_sidebar_layout(const SidebarLayoutState& state)
{
	SwitchPresentation out;

	const bool horizontal_strip = state.sidebar_enabled
			&& state.position == SidebarPosition::Horizontal;

	out.sidebar_visible = state.sidebar_enabled;
	out.categories_horizontal = horizontal_strip;
	out.show_default_category_heading = !state.sidebar_enabled;

	// Switch placement: none without Places; into the search bar when the
	// sidebar is gone but Places remains; otherwise it lives with the
	// category list (vertical sidebar or top/bottom strip).
	if (!state.places_enabled)
		out.switch_location = SwitchLocation::None;
	else if (!state.sidebar_enabled)
		out.switch_location = SwitchLocation::InSearchBar;
	else
		out.switch_location = SwitchLocation::InSidebar;

	// Category names are suppressed on a horizontal strip; otherwise the
	// stored intent stands (left/right honour the user's choice).
	out.effective_show_category_names =
			horizontal_strip ? false : state.category_show_name;

	// The switch is forced to icon-only on a horizontal strip and in the
	// search-bar row; on a vertical sidebar it follows the stored intent.
	if (out.switch_location == SwitchLocation::InSearchBar)
		out.effective_show_icons = true;
	else if (out.switch_location == SwitchLocation::InSidebar && horizontal_strip)
		out.effective_show_icons = true;
	else
		out.effective_show_icons = state.switch_show_icons;

	// Orientation: horizontal everywhere except a vertical sidebar whose
	// category names are hidden, where a vertical switch keeps the sidebar
	// from being forced wider.
	if (out.switch_location == SwitchLocation::InSidebar
			&& !horizontal_strip
			&& !state.category_show_name)
		out.switch_orientation = SwitchOrientation::Vertical;
	else
		out.switch_orientation = SwitchOrientation::Horizontal;

	return out;
}

StripGeometry meow_compute_strip_geometry(SidebarPosition position, bool ltr)
{
	(void)ltr; // order is direction-independent; anchors are direction-relative
	StripGeometry out;
	out.order = (position == SidebarPosition::Bottom)
			? StripOrder::StripBelowResults
			: StripOrder::StripAboveResults;
	// Single-row anchoring: toggle pinned leading, categories pinned trailing,
	// with the slack between them. Leading/Trailing map to GTK START/END, so the
	// physical side follows the text direction without a branch here.
	out.toggle_anchor = StripAnchor::Leading;
	out.categories_anchor = StripAnchor::Trailing;
	out.width_from_main_column = true;
	return out;
}

FullscreenMainColumn meow_fullscreen_main_column(int workarea_width)
{
	FullscreenMainColumn out;
	if (workarea_width <= 0)
	{
		out.width = 0;
		out.margin = 0;
		return out;
	}

	out.margin = workarea_width / 6;
	out.width = workarea_width - (out.margin * 2);
	return out;
}

/* meow_toggle_icon_px:
 * @location: effective home of the Apps/Places selector.
 * @category_px: configured category icon allocation in a sidebar.
 * @search_bar_px: measured search-row icon allocation.
 * @session_px: theme allocation used by Session command icons.
 *
 * Selects the icon allocation that matches the selector's current containing
 * region while keeping the size independent from user preset storage.
 *
 * Returns: the pixel allocation, or zero when the selector is hidden.
 */
int meow_toggle_icon_px(SwitchLocation location, int category_px,
		int search_bar_px, int session_px)
{
	// The toggle inherits the pixel size of the region that contains it; a
	// hidden toggle (None) gets no size, signalled by 0.
	switch (location)
	{
	case SwitchLocation::InSidebar:
		return category_px;
	case SwitchLocation::InSecondaryRow:
		return session_px;
	case SwitchLocation::InSearchBar:
		return search_bar_px;
	case SwitchLocation::None:
	default:
		return 0;
	}
}

int meow_toggle_button_height_px(SwitchLocation location, bool categories_horizontal,
		int category_px)
{
	if (location == SwitchLocation::InSidebar && categories_horizontal)
		return category_px;

	return -1;
}

int meow_strip_spacer_order(bool categories_horizontal)
{
	return categories_horizontal ? 0 : -1;
}

int meow_default_category_order_base(bool strip_spacer_visible,
		bool vertical_switch_controls)
{
	if (strip_spacer_visible)
		return 1;
	return vertical_switch_controls ? 3 : 0;
}

EmbeddedSwitchSlot meow_embedded_switch_slot(bool commands_in_row)
{
	// When commands share the row the switch is anchored before them so the
	// commands stay trailing-most; alone, the switch itself is the trailing
	// element. There is deliberately no slot that follows a present command box.
	return commands_in_row ? EmbeddedSwitchSlot::BeforeCommands
	                       : EmbeddedSwitchSlot::Trailing;
}

bool meow_category_label_visible(bool category_show_name, bool horizontal)
{
	// A Top/Bottom strip is icon-only; otherwise the stored intent stands. This
	// is the one decision both Apps and Places sidebar buttons consult.
	return category_show_name && !horizontal;
}

CategoryLabelCap meow_category_label_cap(long label_chars, int cap_chars)
{
	CategoryLabelCap out;
	// Strictly-longer-than-cap ellipsises; an at-cap label is left uncapped so
	// the size-group floor stays at its natural width (see header rationale).
	out.ellipsize = label_chars > cap_chars;
	out.max_width_chars = cap_chars;
	return out;
}

int meow_sidebar_max_label_width(const int* widths, int count)
{
	int max_width = 0;
	for (int i = 0; i < count; ++i)
	{
		if (widths[i] > max_width)
		{
			max_width = widths[i];
		}
	}
	return max_width;
}

} // namespace WhiskerMenu
