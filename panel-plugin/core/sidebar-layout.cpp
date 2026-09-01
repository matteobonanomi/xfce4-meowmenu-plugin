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

SelectorPresentation meow_resolve_selector_presentation(
		MenuControlLocation location, LayoutMode layout_mode,
		bool category_names_visible, bool icons_requested, bool places_active,
		int category_px, int search_px, int session_px)
{
	SelectorPresentation result = {};
	const bool show_icons = icons_requested
			|| location == MenuControlLocation::PrimaryRow;
	result.content = show_icons ? SelectorContent::Icons : SelectorContent::Labels;
	result.active_mode = places_active
			? SelectorActiveMode::Places : SelectorActiveMode::Applications;
	result.natural_height = true;
	result.orientation = SwitchOrientation::Horizontal;

	switch (location)
	{
	case MenuControlLocation::Sidebar:
		result.home = SelectorHome::Sidebar;
		result.icon_size_source = SelectorIconSizeSource::Category;
		result.icon_px = category_px;
		if (!category_names_visible)
			result.orientation = SwitchOrientation::Vertical;
		break;
	case MenuControlLocation::SecondaryRow:
		result.home = SelectorHome::SecondaryRow;
		result.icon_size_source = SelectorIconSizeSource::SessionToolbar;
		result.icon_px = session_px;
		break;
	case MenuControlLocation::PrimaryRow:
		if (layout_mode == LayoutMode::FullScreen)
		{
			result.home = SelectorHome::FullScreenSearch;
			result.icon_size_source = SelectorIconSizeSource::SessionToolbar;
			result.icon_px = session_px;
		}
		else
		{
			result.home = SelectorHome::WindowedPrimary;
			result.icon_size_source = SelectorIconSizeSource::Search;
			result.icon_px = search_px;
		}
		break;
	case MenuControlLocation::Hidden:
	default:
		result.home = SelectorHome::Hidden;
		result.icon_size_source = SelectorIconSizeSource::None;
		result.icon_px = 0;
		break;
	}

	return result;
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

SidebarPresentation meow_compute_sidebar_presentation(
		const SidebarLayoutState& state)
{
	SidebarPresentation out;

	const bool horizontal_strip = state.sidebar_enabled
			&& state.position == SidebarPosition::Horizontal;

	out.sidebar_visible = state.sidebar_enabled;
	out.categories_horizontal = horizontal_strip;
	out.show_default_category_heading = !state.sidebar_enabled;

	// Category names are suppressed on a horizontal strip; otherwise the
	// stored intent stands (left/right honour the user's choice).
	out.effective_show_category_names =
			horizontal_strip ? false : state.category_show_name;

	return out;
}

StripGeometry meow_compute_strip_geometry(SidebarPosition position, bool ltr)
{
	(void)ltr; // stacking and centered category placement are direction-independent
	StripGeometry out;
	out.order = (position == SidebarPosition::Bottom)
			? StripOrder::StripBelowResults
			: StripOrder::StripAboveResults;
	// The selector has its own derived home; only category navigation belongs to
	// this strip. Symmetric expansion in the GTK box centers the group without
	// changing its physical result in RTL.
	out.toggle_anchor = StripAnchor::Leading;
	out.categories_anchor = StripAnchor::Center;
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

int meow_strip_spacer_order(bool categories_horizontal)
{
	return categories_horizontal ? 0 : -1;
}

bool meow_strip_spacers_visible(bool categories_horizontal)
{
	return categories_horizontal;
}

bool meow_sidebar_group_separator_visible(bool leading_group_visible,
		bool following_group_visible)
{
	return leading_group_visible && following_group_visible;
}

int meow_favourites_icon_render_size(int category_icon_px)
{
	if (category_icon_px <= 1)
		return category_icon_px;

	const int optical_px = ((category_icon_px * 2) + 1) / 3;
	const int visible_px = optical_px < 16 ? 16 : optical_px;
	return visible_px > category_icon_px ? category_icon_px : visible_px;
}

int meow_default_category_order_base(bool strip_spacer_visible,
		bool vertical_switch_controls)
{
	if (strip_spacer_visible)
		return 1;
	return vertical_switch_controls ? 2 : 0;
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
	// A Horizontal strip is icon-only; otherwise the stored intent stands. This
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
