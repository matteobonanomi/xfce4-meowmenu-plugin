/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "menu-mode-state.h"

#include <cstring>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* resolve_opening_mode:
 * @places_enabled: whether Places is available.
 * @remember_enabled: whether the saved top-level mode should be consulted.
 * @stored_mode: persisted mode text; may be NULL, empty, or obsolete.
 *
 * Resolves every opening to a closed, valid top-level mode. Only the exact
 * persisted value "places" opts into Places; all other input fails safely to
 * Applications without requiring a settings rewrite.
 *
 * Returns: the valid top-level mode to present.
 */
MenuMode WhiskerMenu::resolve_opening_mode(bool places_enabled,
		bool remember_enabled, const char* stored_mode)
{
	return places_enabled
			&& remember_enabled
			&& stored_mode
			&& (std::strcmp(stored_mode, "places") == 0)
		? MenuMode::Places
		: MenuMode::Applications;
}

//-----------------------------------------------------------------------------

/* mode_to_persist:
 * @places_enabled: whether Places is available.
 * @remember_enabled: whether mode persistence is enabled.
 * @mode: current valid top-level mode.
 *
 * Keeps persistence conditional so disabling remembering leaves the stored
 * value untouched. The returned constants have static storage duration.
 *
 * Returns: "apps" or "places" when the caller should save, NULL otherwise.
 */
const char* WhiskerMenu::mode_to_persist(bool places_enabled,
		bool remember_enabled, MenuMode mode)
{
	if (!places_enabled || !remember_enabled)
	{
		return nullptr;
	}
	return mode == MenuMode::Places ? "places" : "apps";
}

//-----------------------------------------------------------------------------

/* resolve_menu_mode:
 * @inputs: requested mode, transition context, current section, and live
 *          enablement values.
 *
 * Computes the complete control-visibility and content decision in one pass.
 * Mode entry uses a deterministic default; live reevaluation retains a valid
 * Places section and otherwise falls back to Home.
 *
 * Returns: a self-consistent presentation containing no inactive-mode control.
 */
MenuModeResolution WhiskerMenu::resolve_menu_mode(const MenuModeInputs& inputs)
{
	MenuModeResolution result;
	result.mode = inputs.places_enabled
			? inputs.requested_mode
			: MenuMode::Applications;

	const bool places = result.mode == MenuMode::Places;
	result.applications_favourites_visible = !places;
	result.applications_recent_visible =
			!places && inputs.recent_applications_enabled;
	result.applications_all_visible = !places;
	result.application_categories_visible = !places;
	result.places_home_visible = places;
	result.places_history_visible =
			places && inputs.places_history_enabled;
	result.places_favourites_visible =
			places && inputs.places_favourites_enabled;

	if (!places)
	{
		result.content = inputs.transition == MenuModeTransition::Enter
				? MenuContentTarget::ApplicationsDefault
				: MenuContentTarget::RetainCurrent;
		return result;
	}

	if (inputs.transition == MenuModeTransition::Enter)
	{
		result.content = MenuContentTarget::PlacesHome;
		return result;
	}

	switch (inputs.current_content)
	{
	case MenuContentTarget::PlacesHistory:
		result.content = inputs.places_history_enabled
				? MenuContentTarget::PlacesHistory
				: MenuContentTarget::PlacesHome;
		break;

	case MenuContentTarget::PlacesFavourites:
		result.content = inputs.places_favourites_enabled
				? MenuContentTarget::PlacesFavourites
				: MenuContentTarget::PlacesHome;
		break;

	case MenuContentTarget::PlacesHome:
		result.content = MenuContentTarget::PlacesHome;
		break;

	default:
		result.content = MenuContentTarget::PlacesHome;
		break;
	}
	return result;
}
