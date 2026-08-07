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

	return out;
}

}
