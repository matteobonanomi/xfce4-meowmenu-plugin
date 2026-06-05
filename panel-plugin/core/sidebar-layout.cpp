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

SidebarPosition meow_parse_sidebar_position(const char* value)
{
	if (value)
	{
		if (std::strcmp(value, "right") == 0)
			return SidebarPosition::Right;
		if (std::strcmp(value, "top") == 0)
			return SidebarPosition::Top;
		if (std::strcmp(value, "bottom") == 0)
			return SidebarPosition::Bottom;
	}
	// NOTE: "left", NULL, the legacy "hidden", and any unknown value all
	// resolve to Left — a valid Position. "hidden" is migrated to the
	// Enable-sidebar switch, so it must never be treated as a position.
	return SidebarPosition::Left;
}

SwitchPresentation meow_compute_sidebar_layout(const SidebarLayoutState& state)
{
	SwitchPresentation out;

	const bool horizontal_strip = state.sidebar_enabled
			&& (state.position == SidebarPosition::Top
				|| state.position == SidebarPosition::Bottom);

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
	out.width_from_search_box = true;
	return out;
}

int meow_toggle_icon_px(SwitchLocation location, int category_px, int search_bar_px)
{
	// The toggle inherits the pixel size of the region that contains it; a
	// hidden toggle (None) gets no size, signalled by 0.
	switch (location)
	{
	case SwitchLocation::InSidebar:
		return category_px;
	case SwitchLocation::InSearchBar:
		return search_bar_px;
	case SwitchLocation::None:
	default:
		return 0;
	}
}

bool meow_category_label_visible(bool category_show_name, bool horizontal)
{
	// A Top/Bottom strip is icon-only; otherwise the stored intent stands. This
	// is the one decision both Apps and Places sidebar buttons consult.
	return category_show_name && !horizontal;
}

} // namespace WhiskerMenu
