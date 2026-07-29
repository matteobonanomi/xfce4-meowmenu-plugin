/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "core/menu-mode-state.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>

using namespace WhiskerMenu;

namespace
{

int failures = 0;

#define CHECK(condition) do { \
		if (!(condition)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", \
					__FILE__, __LINE__, #condition); \
			++failures; \
		} \
	} while (0)

}

int main()
{
	const char* stored_values[] = {
		nullptr, "", "apps", "places", "Places", "obsolete"
	};
	for (bool places_enabled : { false, true })
	{
		for (bool remember_enabled : { false, true })
		{
			for (const char* stored : stored_values)
			{
				const bool expect_places = places_enabled
						&& remember_enabled
						&& stored
						&& std::string(stored) == "places";
				CHECK((resolve_opening_mode(places_enabled,
						remember_enabled, stored) == MenuMode::Places)
						== expect_places);
			}
		}
	}

	CHECK(mode_to_persist(false, true, MenuMode::Places) == nullptr);
	CHECK(mode_to_persist(true, false, MenuMode::Places) == nullptr);
	CHECK(std::string(mode_to_persist(true, true,
			MenuMode::Applications)) == "apps");
	CHECK(std::string(mode_to_persist(true, true,
			MenuMode::Places)) == "places");

	for (MenuMode mode : { MenuMode::Applications, MenuMode::Places })
	{
		for (bool recent : { false, true })
		{
			for (bool history : { false, true })
			{
				for (bool favourites : { false, true })
				{
					MenuModeInputs inputs;
					inputs.requested_mode = mode;
					inputs.transition = MenuModeTransition::Enter;
					inputs.places_enabled = true;
					inputs.recent_applications_enabled = recent;
					inputs.places_history_enabled = history;
					inputs.places_favourites_enabled = favourites;

					const MenuModeResolution result =
							resolve_menu_mode(inputs);
					const bool places = mode == MenuMode::Places;
					CHECK(result.mode == mode);
					CHECK(result.applications_favourites_visible == !places);
					CHECK(result.applications_recent_visible
							== (!places && recent));
					CHECK(result.applications_all_visible == !places);
					CHECK(result.application_categories_visible == !places);
					CHECK(result.places_home_visible == places);
					CHECK(result.places_history_visible
							== (places && history));
					CHECK(result.places_favourites_visible
							== (places && favourites));
					CHECK(result.content == (places
							? MenuContentTarget::PlacesHome
							: MenuContentTarget::ApplicationsDefault));
				}
			}
		}
	}

	MenuModeInputs disabled;
	disabled.requested_mode = MenuMode::Places;
	disabled.transition = MenuModeTransition::Enter;
	disabled.places_enabled = false;
	const MenuModeResolution disabled_result = resolve_menu_mode(disabled);
	CHECK(disabled_result.mode == MenuMode::Applications);
	CHECK(disabled_result.content == MenuContentTarget::ApplicationsDefault);
	CHECK(!disabled_result.places_home_visible);

	for (MenuContentTarget current : {
			MenuContentTarget::PlacesHome,
			MenuContentTarget::PlacesHistory,
			MenuContentTarget::PlacesFavourites })
	{
		for (bool history : { false, true })
		{
			for (bool favourites : { false, true })
			{
				MenuModeInputs live;
				live.requested_mode = MenuMode::Places;
				live.transition = MenuModeTransition::Reevaluate;
				live.current_content = current;
				live.places_enabled = true;
				live.places_history_enabled = history;
				live.places_favourites_enabled = favourites;
				const MenuModeResolution result = resolve_menu_mode(live);

				MenuContentTarget expected = current;
				if ((current == MenuContentTarget::PlacesHistory && !history)
						|| (current == MenuContentTarget::PlacesFavourites
								&& !favourites))
				{
					expected = MenuContentTarget::PlacesHome;
				}
				CHECK(result.content == expected);
			}
		}
	}

	if (failures != 0)
	{
		std::fprintf(stderr, "test_menu_mode_state: %d failure(s)\n",
				failures);
		return EXIT_FAILURE;
	}
	std::printf("test_menu_mode_state: ok\n");
	return EXIT_SUCCESS;
}
