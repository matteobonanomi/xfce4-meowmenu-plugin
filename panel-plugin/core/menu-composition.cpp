/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "menu-composition.h"

#include <algorithm>

namespace WhiskerMenu
{

namespace
{

bool usable_rectangle(const MenuSurfaceRectangle& rectangle)
{
	return rectangle.visible && rectangle.width > 0 && rectangle.height > 0;
}

int band_index(const MenuComposition& composition, MenuBand band)
{
	const auto found = std::find(composition.vertical_bands.begin(),
			composition.vertical_bands.end(), band);
	return found == composition.vertical_bands.end()
			? -1 : static_cast<int>(found - composition.vertical_bands.begin());
}

/* append_logical_slots:
 * @target: physical slot list to replace.
 * @logical: semantic leading-to-trailing slot list.
 * @direction: current interface direction.
 *
 * Converts one logical row to physical left-to-right order. Reversing only at
 * this boundary keeps all placement rules expressed in semantic order.
 */
void append_logical_slots(std::vector<MenuSlot>& target,
		const std::vector<MenuSlot>& logical, MenuDirection direction)
{
	target = logical;
	if (direction == MenuDirection::RightToLeft)
		std::reverse(target.begin(), target.end());
}

/* resolve_windowed_bands:
 * @out: composition whose row visibility and horizontal edge are resolved.
 * @input: current Docked or Centered inputs.
 *
 * Produces the complete vertical order, including the bottommost shared row
 * used when Session is present without a sidebar.
 */
void resolve_windowed_bands(MenuComposition& out,
		const MenuCompositionInput& input)
{
	if (input.primary_edge == PrimaryEdge::Top)
	{
		out.vertical_bands.push_back(MenuBand::Primary);
		out.vertical_bands.push_back(MenuBand::Results);
		if (out.horizontal_sidebar_visible)
			out.vertical_bands.push_back(MenuBand::HorizontalSidebar);
		if (out.secondary_visible)
			out.vertical_bands.push_back(MenuBand::Secondary);
		return;
	}

	const bool bottommost_session = input.sidebar == CompositionSidebar::Hidden
			&& out.effective_session;
	if (bottommost_session)
	{
		out.vertical_bands.push_back(MenuBand::Results);
		out.vertical_bands.push_back(MenuBand::Primary);
		out.vertical_bands.push_back(MenuBand::Secondary);
		return;
	}

	if (out.secondary_visible)
		out.vertical_bands.push_back(MenuBand::Secondary);
	if (out.horizontal_sidebar_visible)
		out.vertical_bands.push_back(MenuBand::HorizontalSidebar);
	out.vertical_bands.push_back(MenuBand::Results);
	out.vertical_bands.push_back(MenuBand::Primary);
}

/* resolve_primary_slots:
 * @out: composition with control locations already resolved.
 * @input: current layout and direction inputs.
 *
 * Resolves physical row order. A vertical sidebar is an explicit physical
 * column; all other arrangements use logical order and mirror for RTL.
 */
void resolve_primary_slots(MenuComposition& out,
		const MenuCompositionInput& input)
{
	std::vector<MenuSlot> logical;
	if (input.layout_mode == LayoutMode::FullScreen)
	{
		if (out.effective_profile)
			logical.push_back(MenuSlot::Profile);
		if (out.apps_places_location == MenuControlLocation::PrimaryRow)
			logical.push_back(MenuSlot::AppsPlaces);
		logical.push_back(MenuSlot::Search);
		if (out.effective_session)
			logical.push_back(MenuSlot::Session);
		append_logical_slots(out.primary_slots, logical, input.direction);
		return;
	}

	const bool vertical = input.sidebar == CompositionSidebar::Left
			|| input.sidebar == CompositionSidebar::Right;
	if (vertical && out.effective_profile)
	{
		if (input.sidebar == CompositionSidebar::Left)
		{
			out.primary_slots.push_back(MenuSlot::Profile);
			out.primary_slots.push_back(MenuSlot::Search);
		}
		else
		{
			out.primary_slots.push_back(MenuSlot::Search);
			out.primary_slots.push_back(MenuSlot::Profile);
		}
		return;
	}

	if (out.effective_profile)
		logical.push_back(MenuSlot::Profile);
	logical.push_back(MenuSlot::Search);
	if (out.apps_places_location == MenuControlLocation::PrimaryRow)
		logical.push_back(MenuSlot::AppsPlaces);
	append_logical_slots(out.primary_slots, logical, input.direction);
}

/* resolve_secondary_slots:
 * @out: composition with secondary locations already resolved.
 * @input: current sidebar and direction inputs.
 *
 * Keeps explicit vertical sidebar columns physical while mirroring the
 * leading/trailing shared-row arrangement in RTL.
 */
void resolve_secondary_slots(MenuComposition& out,
		const MenuCompositionInput& input)
{
	if (!out.secondary_visible)
		return;

	const bool apps = out.apps_places_location
			== MenuControlLocation::SecondaryRow;
	const bool vertical = input.sidebar == CompositionSidebar::Left
			|| input.sidebar == CompositionSidebar::Right;
	if (vertical)
	{
		if (input.sidebar == CompositionSidebar::Left && apps)
			out.secondary_slots.push_back(MenuSlot::AppsPlaces);
		if (out.effective_session)
			out.secondary_slots.push_back(MenuSlot::Session);
		if (input.sidebar == CompositionSidebar::Right && apps)
			out.secondary_slots.push_back(MenuSlot::AppsPlaces);
		return;
	}

	std::vector<MenuSlot> logical;
	if (apps)
		logical.push_back(MenuSlot::AppsPlaces);
	if (out.effective_session)
		logical.push_back(MenuSlot::Session);
	append_logical_slots(out.secondary_slots, logical, input.direction);
}

}

MenuComposition meow_resolve_menu_composition(const MenuCompositionInput& input)
{
	MenuComposition out = {};
	out.sidebar = input.sidebar;
	out.primary_edge = input.layout_mode == LayoutMode::FullScreen
			? PrimaryEdge::Top : input.primary_edge;
	out.horizontal_sidebar_visible = input.sidebar
			== CompositionSidebar::Horizontal;
	out.horizontal_sidebar_edge = input.layout_mode == LayoutMode::FullScreen
			|| input.primary_edge == PrimaryEdge::Top
			? PrimaryEdge::Bottom : PrimaryEdge::Top;
	out.effective_profile = input.show_profile;
	out.effective_session = input.show_session
			&& input.available_session_actions > 0;

	out.profile_location = out.effective_profile
			? MenuControlLocation::PrimaryRow : MenuControlLocation::Hidden;
	out.search_location = MenuControlLocation::PrimaryRow;
	out.session_location = out.effective_session
			? (input.layout_mode == LayoutMode::FullScreen
				? MenuControlLocation::PrimaryRow
				: MenuControlLocation::SecondaryRow)
			: MenuControlLocation::Hidden;
	const bool no_vertical_sidebar = input.sidebar == CompositionSidebar::Hidden
				|| input.sidebar == CompositionSidebar::Horizontal;

	if (!input.places_enabled)
		out.apps_places_location = MenuControlLocation::Hidden;
	else if (input.layout_mode == LayoutMode::FullScreen)
		out.apps_places_location = no_vertical_sidebar
				? MenuControlLocation::PrimaryRow
				: MenuControlLocation::Sidebar;
	else if (no_vertical_sidebar
				&& !out.effective_session && !out.effective_profile)
		out.apps_places_location = MenuControlLocation::PrimaryRow;
	else
		out.apps_places_location = MenuControlLocation::SecondaryRow;

	out.secondary_visible = input.layout_mode != LayoutMode::FullScreen
			&& (out.session_location == MenuControlLocation::SecondaryRow
				|| out.apps_places_location == MenuControlLocation::SecondaryRow);

	if (input.layout_mode == LayoutMode::FullScreen)
	{
		out.vertical_bands.push_back(MenuBand::Primary);
		out.vertical_bands.push_back(MenuBand::Results);
		if (out.horizontal_sidebar_visible)
			out.vertical_bands.push_back(MenuBand::HorizontalSidebar);
	}
	else
	{
		resolve_windowed_bands(out, input);
	}

	resolve_primary_slots(out, input);
	resolve_secondary_slots(out, input);

	const bool vertical = input.sidebar == CompositionSidebar::Left
			|| input.sidebar == CompositionSidebar::Right;
	out.profile_column = !out.effective_profile ? MenuColumnRole::None
			: ((vertical && input.layout_mode != LayoutMode::FullScreen)
				? MenuColumnRole::Sidebar : MenuColumnRole::Outer);
	out.search_column = input.layout_mode == LayoutMode::FullScreen
			? MenuColumnRole::MiddleResults
			: ((!out.effective_profile
					&& out.apps_places_location != MenuControlLocation::PrimaryRow)
				? MenuColumnRole::FullWidth : MenuColumnRole::Results);
	out.apps_places_column = out.apps_places_location == MenuControlLocation::Hidden
			? MenuColumnRole::None
			: (out.apps_places_location == MenuControlLocation::Sidebar
				? MenuColumnRole::Sidebar
				: (input.layout_mode == LayoutMode::FullScreen
					? MenuColumnRole::MiddleResults : MenuColumnRole::Outer));
	out.session_column = !out.effective_session ? MenuColumnRole::None
			: (input.layout_mode == LayoutMode::FullScreen
				? MenuColumnRole::Outer
				: (vertical ? MenuColumnRole::Results : MenuColumnRole::Outer));

	out.profile_alignment = out.effective_profile
			? (vertical ? MenuAlignment::Fill : MenuAlignment::LogicalLeading)
			: MenuAlignment::None;
	out.search_alignment = MenuAlignment::Fill;
	out.apps_places_alignment = out.apps_places_location
			== MenuControlLocation::Hidden ? MenuAlignment::None
			: ((vertical && out.apps_places_location != MenuControlLocation::PrimaryRow)
				? MenuAlignment::Fill : MenuAlignment::LogicalLeading);
	out.session_alignment = out.effective_session
			? MenuAlignment::LogicalTrailing
			: MenuAlignment::None;

	const bool fullscreen = input.layout_mode == LayoutMode::FullScreen;
	out.baseline_surface = fullscreen
			? MenuSurfaceRole::FullScreen : MenuSurfaceRole::Content;
	out.primary_surface = fullscreen
			? MenuSurfaceRole::FullScreen : MenuSurfaceRole::Content;
	out.profile_surface = !out.effective_profile
			? MenuSurfaceRole::None
			: (fullscreen ? MenuSurfaceRole::FullScreen
					: (vertical ? MenuSurfaceRole::Chrome
							: MenuSurfaceRole::Content));
	out.search_surface = fullscreen
			? MenuSurfaceRole::FullScreen : MenuSurfaceRole::Content;
	out.results_surface = fullscreen
			? MenuSurfaceRole::FullScreen : MenuSurfaceRole::Content;
	out.sidebar_surface = !vertical
			? MenuSurfaceRole::None
			: (fullscreen ? MenuSurfaceRole::FullScreen
					: MenuSurfaceRole::Chrome);
	out.horizontal_sidebar_surface = !out.horizontal_sidebar_visible
			? MenuSurfaceRole::None
			: (fullscreen ? MenuSurfaceRole::FullScreen
					: MenuSurfaceRole::Chrome);
	out.secondary_surface = out.secondary_visible
			? MenuSurfaceRole::Chrome : MenuSurfaceRole::None;

	return out;
}

MenuChromeGeometry meow_resolve_chrome_geometry(
		const MenuComposition& composition, int window_width, int window_height,
		int region_gap,
		const MenuSurfaceRectangle& profile,
		const MenuSurfaceRectangle& sidebar,
		const MenuSurfaceRectangle& horizontal,
		const MenuSurfaceRectangle& secondary)
{
	MenuChromeGeometry out = {};
	if (window_width <= 0 || window_height <= 0)
		return out;

	int column_left = window_width;
	int column_right = 0;
	auto include_column = [&](const MenuSurfaceRectangle& rectangle)
	{
		if (!usable_rectangle(rectangle))
			return;
		column_left = std::min(column_left, rectangle.x);
		column_right = std::max(column_right,
				rectangle.x + rectangle.width);
	};
	if (composition.profile_surface == MenuSurfaceRole::Chrome)
		include_column(profile);
	if (composition.sidebar_surface == MenuSurfaceRole::Chrome)
		include_column(sidebar);
	column_left = std::max(0, std::min(window_width, column_left));
	column_right = std::max(0, std::min(window_width, column_right));
	if (column_right > column_left)
	{
		if (composition.sidebar == CompositionSidebar::Left)
			out.vertical = { 0, 0, column_right, window_height, true };
		else if (composition.sidebar == CompositionSidebar::Right)
			out.vertical = { column_left, 0,
					window_width - column_left, window_height, true };
	}

	int band_top = window_height;
	int band_bottom = 0;
	int first_band = static_cast<int>(composition.vertical_bands.size());
	auto include_band = [&](const MenuSurfaceRectangle& rectangle,
			MenuBand band)
	{
		if (!usable_rectangle(rectangle))
			return;
		band_top = std::min(band_top, rectangle.y);
		band_bottom = std::max(band_bottom,
				rectangle.y + rectangle.height);
		const int index = band_index(composition, band);
		if (index >= 0)
			first_band = std::min(first_band, index);
	};
	if (composition.horizontal_sidebar_surface == MenuSurfaceRole::Chrome)
		include_band(horizontal, MenuBand::HorizontalSidebar);
	if (composition.secondary_surface == MenuSurfaceRole::Chrome)
		include_band(secondary, MenuBand::Secondary);
	band_top = std::max(0, std::min(window_height, band_top));
	band_bottom = std::max(0, std::min(window_height, band_bottom));
	if (band_bottom > band_top)
	{
		const int results = band_index(composition, MenuBand::Results);
		const bool follows_results = results >= 0 && first_band > results;
		const int outer_gap = follows_results
				? window_height - band_bottom : band_top;
		const int gap = outer_gap > 0
				? outer_gap : std::max(0, region_gap);
		if (composition.secondary_surface == MenuSurfaceRole::Chrome)
		{
			if (follows_results)
				band_top = std::max(0, band_top - gap);
			else
				band_bottom = std::min(window_height, band_bottom + gap);
		}
		out.band = follows_results
				? MenuSurfaceRectangle{ 0, band_top, window_width,
						window_height - band_top, true }
				: MenuSurfaceRectangle{ 0, 0, window_width,
						band_bottom, true };
		if (composition.secondary_surface == MenuSurfaceRole::Chrome)
		{
			out.separator = follows_results
					? MenuSurfaceRectangle{ 0, band_top,
							window_width, 1, true }
					: MenuSurfaceRectangle{ 0, band_bottom - 1,
							window_width, 1, true };
		}
	}

	return out;
}

MenuLayoutSnapshot meow_resolve_layout_snapshot(
		const MenuLayoutSnapshotInput& input)
{
	MenuLayoutSnapshot snapshot = {};
	snapshot.input = input;
	snapshot.composition = meow_resolve_menu_composition(input.composition);
	return snapshot;
}

bool meow_layout_snapshot_equal(const MenuLayoutSnapshot& first,
		const MenuLayoutSnapshot& second)
{
	const MenuCompositionInput& a = first.input.composition;
	const MenuCompositionInput& b = second.input.composition;
	if (a.layout_mode != b.layout_mode || a.primary_edge != b.primary_edge
			|| a.sidebar != b.sidebar || a.show_profile != b.show_profile
			|| a.show_session != b.show_session
			|| a.available_session_actions != b.available_session_actions
			|| a.places_enabled != b.places_enabled
			|| a.direction != b.direction
			|| first.input.category_names_visible
					!= second.input.category_names_visible
			|| first.input.selector_icons_requested
					!= second.input.selector_icons_requested
			|| first.input.category_icon_px != second.input.category_icon_px
			|| first.input.search_icon_px != second.input.search_icon_px
			|| first.input.session_icon_px != second.input.session_icon_px)
		return false;

	const MenuComposition& x = first.composition;
	const MenuComposition& y = second.composition;
	return x.sidebar == y.sidebar
			&& x.primary_edge == y.primary_edge
			&& x.horizontal_sidebar_edge == y.horizontal_sidebar_edge
			&& x.horizontal_sidebar_visible == y.horizontal_sidebar_visible
			&& x.secondary_visible == y.secondary_visible
			&& x.effective_profile == y.effective_profile
			&& x.effective_session == y.effective_session
			&& x.vertical_bands == y.vertical_bands
			&& x.primary_slots == y.primary_slots
			&& x.secondary_slots == y.secondary_slots
			&& x.profile_location == y.profile_location
			&& x.search_location == y.search_location
			&& x.apps_places_location == y.apps_places_location
			&& x.session_location == y.session_location
			&& x.profile_column == y.profile_column
			&& x.search_column == y.search_column
			&& x.apps_places_column == y.apps_places_column
			&& x.session_column == y.session_column
			&& x.profile_alignment == y.profile_alignment
			&& x.search_alignment == y.search_alignment
			&& x.apps_places_alignment == y.apps_places_alignment
			&& x.session_alignment == y.session_alignment
			&& x.baseline_surface == y.baseline_surface
			&& x.primary_surface == y.primary_surface
			&& x.profile_surface == y.profile_surface
			&& x.search_surface == y.search_surface
			&& x.results_surface == y.results_surface
			&& x.sidebar_surface == y.sidebar_surface
			&& x.horizontal_sidebar_surface == y.horizontal_sidebar_surface
			&& x.secondary_surface == y.secondary_surface;
}

}
