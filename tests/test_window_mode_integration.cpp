/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "core/menu-mode-state.h"
#include "core/window-keyboard.h"

#include <gtk/gtk.h>

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

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

struct ModeWidgets
{
	GtkWidget* app_favourites = gtk_button_new();
	GtkWidget* app_recent = gtk_button_new();
	GtkWidget* app_all = gtk_button_new();
	GtkWidget* app_category = gtk_button_new();
	GtkWidget* places_home = gtk_button_new();
	GtkWidget* places_history = gtk_button_new();
	GtkWidget* places_favourites = gtk_button_new();

	ModeWidgets()
	{
		for (GtkWidget* widget : all())
		{
			g_object_ref_sink(widget);
		}
	}

	~ModeWidgets()
	{
		for (GtkWidget* widget : all())
		{
			gtk_widget_destroy(widget);
			g_object_unref(widget);
		}
	}

	void apply(const MenuModeResolution& resolution)
	{
		gtk_widget_set_visible(app_favourites,
				resolution.applications_favourites_visible);
		gtk_widget_set_visible(app_recent,
				resolution.applications_recent_visible);
		gtk_widget_set_visible(app_all,
				resolution.applications_all_visible);
		gtk_widget_set_visible(app_category,
				resolution.application_categories_visible);
		gtk_widget_set_visible(places_home,
				resolution.places_home_visible);
		gtk_widget_set_visible(places_history,
				resolution.places_history_visible);
		gtk_widget_set_visible(places_favourites,
				resolution.places_favourites_visible);
	}

	std::vector<GtkWidget*> all() const
	{
		return {
			app_favourites,
			app_recent,
			app_all,
			app_category,
			places_home,
			places_history,
			places_favourites
		};
	}
};

}

int main()
{
	if (!gtk_init_check(nullptr, nullptr))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	std::string stored = "places";
	MenuMode active = resolve_opening_mode(true, false, stored.c_str());
	CHECK(active == MenuMode::Applications);
	CHECK(mode_to_persist(true, false, MenuMode::Places) == nullptr);
	CHECK(stored == "places");

	const char* saved = mode_to_persist(true, true, active);
	CHECK(saved != nullptr);
	stored = saved;
	CHECK(stored == "apps");
	for (int cycle = 0; cycle < 20; ++cycle)
	{
		active = resolve_opening_mode(true, true, stored.c_str());
		CHECK(active == MenuMode::Applications);
		saved = mode_to_persist(true, true, active);
		CHECK(saved != nullptr);
		stored = saved;
	}

	CHECK(resolve_opening_mode(true, true, nullptr)
			== MenuMode::Applications);
	CHECK(resolve_opening_mode(true, true, "")
			== MenuMode::Applications);
	CHECK(resolve_opening_mode(true, true, "unknown")
			== MenuMode::Applications);
	CHECK(resolve_opening_mode(true, true, "places")
			== MenuMode::Places);

	ModeWidgets widgets;
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
					const MenuModeResolution resolution =
							resolve_menu_mode(inputs);
					widgets.apply(resolution);

					const bool places = mode == MenuMode::Places;
					CHECK(gtk_widget_get_visible(widgets.app_favourites)
							== !places);
					CHECK(gtk_widget_get_visible(widgets.app_recent)
							== (!places && recent));
					CHECK(gtk_widget_get_visible(widgets.app_all) == !places);
					CHECK(gtk_widget_get_visible(widgets.app_category)
							== !places);
					CHECK(gtk_widget_get_visible(widgets.places_home)
							== places);
					CHECK(gtk_widget_get_visible(widgets.places_history)
							== (places && history));
					CHECK(gtk_widget_get_visible(widgets.places_favourites)
							== (places && favourites));
				}
			}
		}
	}

	MenuModeInputs live;
	live.requested_mode = MenuMode::Places;
	live.transition = MenuModeTransition::Reevaluate;
	live.current_content = MenuContentTarget::PlacesHistory;
	live.places_enabled = true;
	live.places_history_enabled = true;
	CHECK(resolve_menu_mode(live).content
			== MenuContentTarget::PlacesHistory);
	live.places_history_enabled = false;
	CHECK(resolve_menu_mode(live).content == MenuContentTarget::PlacesHome);
	live.places_enabled = false;
	const MenuModeResolution forced_apps = resolve_menu_mode(live);
	CHECK(forced_apps.mode == MenuMode::Applications);
	CHECK(!forced_apps.places_home_visible);
	CHECK(Keyboard::tab_action(true) == Keyboard::TabAction::ToggleMode);
	CHECK(Keyboard::tab_action(false) == Keyboard::TabAction::Inert);

	if (failures != 0)
	{
		std::fprintf(stderr,
				"test_window_mode_integration: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	std::printf("test_window_mode_integration: ok\n");
	return EXIT_SUCCESS;
}
