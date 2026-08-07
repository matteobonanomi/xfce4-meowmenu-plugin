/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_CORE_MENU_COMPOSITION_H
#define MEOWMENU_CORE_MENU_COMPOSITION_H

#include "layout-mode.h"

#include <vector>

namespace WhiskerMenu
{

enum class PrimaryEdge
{
	Top,
	Bottom
};

enum class CompositionSidebar
{
	Hidden,
	Left,
	Right,
	Horizontal
};

enum class MenuDirection
{
	LeftToRight,
	RightToLeft
};

enum class MenuBand
{
	Primary,
	Results,
	HorizontalSidebar,
	Secondary
};

enum class MenuSlot
{
	Profile,
	AppsPlaces,
	Search,
	Session
};

enum class MenuControlLocation
{
	Hidden,
	PrimaryRow,
	SecondaryRow,
	Sidebar // Only a vertical Left/Right sidebar owns Apps/Places.
};

enum class MenuColumnRole
{
	None,
	FullWidth,
	Sidebar,
	Results,
	MiddleResults,
	Outer
};

enum class MenuAlignment
{
	None,
	Fill,
	LogicalLeading,
	LogicalTrailing
};

/* MenuCompositionInput:
 *
 * One immutable snapshot of stored layout intent and current action
 * availability. The resolver clamps action availability to a present/absent
 * decision and does not mutate persistent settings.
 */
struct MenuCompositionInput
{
	LayoutMode layout_mode;
	PrimaryEdge primary_edge;
	CompositionSidebar sidebar;
	bool show_profile;
	bool show_session;
	unsigned int available_session_actions;
	bool places_enabled;
	MenuDirection direction;
};

/* MenuComposition:
 *
 * Complete GTK-independent instructions for one menu layout pass. Slot lists
 * are stored in physical left-to-right order; logical leading/trailing roles
 * are mirrored by the resolver for right-to-left interfaces. Explicit Left
 * and Right sidebars remain on their selected physical sides.
 */
struct MenuComposition
{
	PrimaryEdge primary_edge;
	PrimaryEdge horizontal_sidebar_edge;
	bool horizontal_sidebar_visible;
	bool secondary_visible;
	bool effective_profile;
	bool effective_session;

	std::vector<MenuBand> vertical_bands;
	std::vector<MenuSlot> primary_slots;
	std::vector<MenuSlot> secondary_slots;

	MenuControlLocation profile_location;
	MenuControlLocation search_location;
	MenuControlLocation apps_places_location;
	MenuControlLocation session_location;

	MenuColumnRole profile_column;
	MenuColumnRole search_column;
	MenuColumnRole apps_places_column;
	MenuColumnRole session_column;

	MenuAlignment profile_alignment;
	MenuAlignment search_alignment;
	MenuAlignment apps_places_alignment;
	MenuAlignment session_alignment;
};

/* meow_resolve_menu_composition:
 * @input: supported layout intent and current Session action availability.
 *
 * Resolves every Docked, Centered, and Full Screen composition from one total
 * value function. The result owns no widgets and is safe to compare or test
 * before a live GTK relayout.
 *
 * Returns: a complete composition by value.
 */
MenuComposition meow_resolve_menu_composition(const MenuCompositionInput& input);

}

#endif // MEOWMENU_CORE_MENU_COMPOSITION_H
