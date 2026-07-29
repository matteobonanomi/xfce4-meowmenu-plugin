/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_MENU_MODE_STATE_H
#define WHISKERMENU_MENU_MODE_STATE_H

namespace WhiskerMenu
{

enum class MenuMode
{
	Applications,
	Places
};

enum class MenuModeTransition
{
	Enter,
	Reevaluate
};

enum class MenuContentTarget
{
	ApplicationsDefault,
	RetainCurrent,
	PlacesHome,
	PlacesHistory,
	PlacesFavourites
};

struct MenuModeInputs
{
	MenuMode requested_mode = MenuMode::Applications;
	MenuModeTransition transition = MenuModeTransition::Enter;
	MenuContentTarget current_content = MenuContentTarget::ApplicationsDefault;
	bool places_enabled = false;
	bool recent_applications_enabled = false;
	bool places_history_enabled = false;
	bool places_favourites_enabled = false;
};

struct MenuModeResolution
{
	MenuMode mode = MenuMode::Applications;
	MenuContentTarget content = MenuContentTarget::ApplicationsDefault;
	bool applications_favourites_visible = true;
	bool applications_recent_visible = false;
	bool applications_all_visible = true;
	bool application_categories_visible = true;
	bool places_home_visible = false;
	bool places_history_visible = false;
	bool places_favourites_visible = false;
};

MenuMode resolve_opening_mode(bool places_enabled, bool remember_enabled,
		const char* stored_mode);
const char* mode_to_persist(bool places_enabled, bool remember_enabled,
		MenuMode mode);
MenuModeResolution resolve_menu_mode(const MenuModeInputs& inputs);

}

#endif // WHISKERMENU_MENU_MODE_STATE_H
