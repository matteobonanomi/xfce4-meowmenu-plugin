/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "core/menu-mode-state.h"
#include "core/menu-composition.h"
#include "core/sidebar-layout.h"
#include "core/window-keyboard.h"
#include "core/window-frame.h"
#include "launcher/command.h"
#include "launcher/page.h"

#include <gtk/gtk.h>

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

using namespace WhiskerMenu;

namespace
{

struct DrawClipCapture
{
	bool seen = false;
	double x1 = 0.0;
	double y1 = 0.0;
	double x2 = 0.0;
	double y2 = 0.0;
};

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

void check_horizontal_selector_home()
{
	for (LayoutMode mode : { LayoutMode::Docked, LayoutMode::Centered,
			LayoutMode::FullScreen })
	{
		MenuCompositionInput hidden = { mode, PrimaryEdge::Bottom,
				CompositionSidebar::Hidden, false, false, 0, true,
				MenuDirection::LeftToRight };
		MenuCompositionInput horizontal = hidden;
		horizontal.sidebar = CompositionSidebar::Horizontal;
		const MenuComposition hidden_out =
				meow_resolve_menu_composition(hidden);
		const MenuComposition horizontal_out =
				meow_resolve_menu_composition(horizontal);
		CHECK(horizontal_out.apps_places_location
				== hidden_out.apps_places_location);
		CHECK(horizontal_out.primary_slots == hidden_out.primary_slots);
		CHECK(horizontal_out.apps_places_location
				== MenuControlLocation::PrimaryRow);
	}
}

void check_horizontal_secondary_boundary_keeps_layout_geometry()
{
	MenuCompositionInput input = {
		LayoutMode::Centered, PrimaryEdge::Top,
		CompositionSidebar::Horizontal, true, true, 2, true,
		MenuDirection::LeftToRight
	};
	const MenuChromeGeometry geometry = meow_resolve_chrome_geometry(
			meow_resolve_menu_composition(input), 450, 500, 6,
			{ 0, 0, 0, 0, false },
			{ 0, 0, 0, 0, false },
			{ 12, 420, 426, 28, true },
			{ 12, 464, 426, 24, true });
	CHECK(geometry.secondary_separator.visible);
	CHECK(geometry.secondary_separator.y == 452);
	CHECK(geometry.secondary_separator.height == 1);
	CHECK(geometry.separator.y == 416);
	CHECK(2 * 420 + 28
			== geometry.separator.y + geometry.secondary_separator.y);
	CHECK(2 * 464 + 24
			== geometry.secondary_separator.y + 500);
	CHECK(geometry.band.y == 408);
	CHECK(geometry.band.height == 92);
}

void check_results_clip_tracks_viewport_allocation()
{
	if (!gtk_init_check(nullptr, nullptr))
		return;
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* fixed = gtk_fixed_new();
	GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
	GtkWidget* result = gtk_drawing_area_new();
	DrawClipCapture capture;
	gtk_widget_set_size_request(result, 320, 640);
	gtk_widget_set_size_request(scroller, 320, 180);
	gtk_container_add(GTK_CONTAINER(scroller), result);
	g_signal_connect(result, "draw",
			G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer data) -> gboolean
			{
				auto* clip = static_cast<DrawClipCapture*>(data);
				clip->seen = true;
				cairo_clip_extents(cr, &clip->x1, &clip->y1,
						&clip->x2, &clip->y2);
				return GDK_EVENT_PROPAGATE;
			}), &capture);
	gtk_fixed_put(GTK_FIXED(fixed), scroller, 40, 30);
	gtk_container_add(GTK_CONTAINER(window), fixed);
	gtk_window_set_default_size(GTK_WINDOW(window), 420, 280);
	gtk_widget_show_all(window);
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	GtkAllocation window_allocation = { 0, 0, 420, 280 };
	gtk_widget_size_allocate(window, &window_allocation);
	auto draw_window = [&]()
	{
		cairo_surface_t* surface = cairo_image_surface_create(
				CAIRO_FORMAT_ARGB32, 420, 280);
		cairo_t* cr = cairo_create(surface);
		gtk_widget_draw(window, cr);
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
	};
	draw_window();
	GtkAllocation allocation = {};
	gtk_widget_get_allocation(scroller, &allocation);
	CHECK(allocation.x == 40);
	CHECK(allocation.y == 30);
	CHECK(allocation.width == 320);
	CHECK(allocation.height == 180);
	GtkAllocation clip = {};
	gtk_widget_get_clip(scroller, &clip);
	CHECK(clip.x == allocation.x);
	CHECK(clip.y == allocation.y);
	CHECK(clip.width == 320);
	CHECK(clip.height == 180);
	CHECK(capture.seen);
	CHECK(capture.x2 - capture.x1 <= 320.0);
	CHECK(capture.y2 - capture.y1 <= 180.0);
	GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(scroller));
	capture.seen = false;
	gtk_adjustment_set_value(adjustment, 120.0);
	gtk_widget_queue_draw(result);
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	gtk_widget_size_allocate(window, &window_allocation);
	draw_window();
	GtkAllocation after_allocation = {};
	gtk_widget_get_allocation(scroller, &after_allocation);
	GtkAllocation after_scroll = {};
	gtk_widget_get_clip(scroller, &after_scroll);
	CHECK(after_allocation.x == allocation.x);
	CHECK(after_allocation.y == allocation.y);
	CHECK(after_allocation.width == allocation.width);
	CHECK(after_allocation.height == allocation.height);
	CHECK(after_scroll.x == clip.x);
	CHECK(after_scroll.y == clip.y);
	CHECK(after_scroll.width == clip.width);
	CHECK(after_scroll.height == clip.height);
	CHECK(capture.seen);
	CHECK(capture.x2 - capture.x1 <= 320.0);
	CHECK(capture.y2 - capture.y1 <= 180.0);
	gtk_widget_destroy(window);
	g_object_unref(window);
}

}

int main()
{
	check_horizontal_selector_home();
	check_horizontal_secondary_boundary_keeps_layout_geometry();
	check_results_clip_tracks_viewport_allocation();
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

	GtkWidget* navigation = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* lead_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* home = gtk_button_new_with_label("Home");
	GtkWidget* favourites = gtk_button_new_with_label("Favourites");
	GtkWidget* group_separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	GtkWidget* trail_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_vexpand(lead_spacer, TRUE);
	gtk_widget_set_vexpand(trail_spacer, TRUE);
	gtk_box_pack_start(GTK_BOX(navigation), lead_spacer, true, true, 0);
	gtk_box_pack_start(GTK_BOX(navigation), home, false, false, 0);
	gtk_box_pack_start(GTK_BOX(navigation), favourites, false, false, 0);
	gtk_box_pack_start(GTK_BOX(navigation), group_separator, false, false, 4);
	gtk_box_pack_start(GTK_BOX(navigation), trail_spacer, true, true, 0);
	gtk_widget_show_all(navigation);

	const bool vertical_spacers = meow_strip_spacers_visible(false);
	gtk_widget_set_visible(lead_spacer, vertical_spacers);
	gtk_widget_set_visible(trail_spacer, vertical_spacers);
	gtk_widget_set_visible(group_separator,
			meow_sidebar_group_separator_visible(true, false));
	CHECK(!gtk_widget_get_visible(lead_spacer));
	CHECK(!gtk_widget_get_visible(trail_spacer));
	CHECK(!gtk_widget_get_visible(group_separator));
	CHECK(gtk_widget_get_visible(home));
	CHECK(gtk_widget_get_visible(favourites));

	const bool horizontal_spacers = meow_strip_spacers_visible(true);
	gtk_widget_set_visible(lead_spacer, horizontal_spacers);
	gtk_widget_set_visible(trail_spacer, horizontal_spacers);
	gtk_widget_set_visible(group_separator,
			meow_sidebar_group_separator_visible(true, true));
	CHECK(gtk_widget_get_visible(lead_spacer));
	CHECK(gtk_widget_get_visible(trail_spacer));
	CHECK(gtk_widget_get_visible(group_separator));
	gtk_widget_destroy(navigation);

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
	CHECK(!command_effectively_available(false, false));
	CHECK(!command_effectively_available(false, true));
	CHECK(!command_effectively_available(true, false));
	CHECK(command_effectively_available(true, true));

	MenuCompositionInput windowed_composition = {
			LayoutMode::Docked, PrimaryEdge::Top, CompositionSidebar::Left,
			true, true, 3, true, MenuDirection::LeftToRight,
	};
	MenuComposition composition =
			meow_resolve_menu_composition(windowed_composition);
	CHECK(composition.session_alignment == MenuAlignment::LogicalTrailing);
	CHECK(composition.apps_places_location
			== MenuControlLocation::SecondaryRow);
	CHECK(composition.baseline_surface == MenuSurfaceRole::Content);
	CHECK(composition.profile_surface == MenuSurfaceRole::Chrome);
	CHECK(composition.sidebar_surface == MenuSurfaceRole::Chrome);
	CHECK(composition.search_surface == MenuSurfaceRole::Content);
	CHECK(composition.results_surface == MenuSurfaceRole::Content);
	CHECK(composition.secondary_surface == MenuSurfaceRole::Chrome);
	windowed_composition.direction = MenuDirection::RightToLeft;
	composition = meow_resolve_menu_composition(windowed_composition);
	CHECK(composition.session_alignment == MenuAlignment::LogicalTrailing);

	windowed_composition.direction = MenuDirection::LeftToRight;
	windowed_composition.sidebar = CompositionSidebar::Left;
	windowed_composition.show_profile = true;
	windowed_composition.show_session = true;
	windowed_composition.available_session_actions = 0;
	composition = meow_resolve_menu_composition(windowed_composition);
	CHECK(composition.apps_places_location
			== MenuControlLocation::SecondaryRow);
	CHECK(composition.secondary_visible);

	windowed_composition.sidebar = CompositionSidebar::Hidden;
	composition = meow_resolve_menu_composition(windowed_composition);
	CHECK(composition.apps_places_location
			== MenuControlLocation::SecondaryRow);
	CHECK(composition.secondary_visible);

	windowed_composition.show_profile = false;
	composition = meow_resolve_menu_composition(windowed_composition);
	CHECK(composition.apps_places_location
			== MenuControlLocation::PrimaryRow);
	CHECK(!composition.secondary_visible);

	const FullscreenMainColumn fullscreen_column =
			meow_fullscreen_main_column(1920);
	CHECK(fullscreen_column.width == 1280);
	CHECK(fullscreen_column.margin == 320);
	GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* middle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* apps = gtk_button_new();
	GtkWidget* search = gtk_search_entry_new();
	gtk_widget_set_size_request(middle, fullscreen_column.width, -1);
	gtk_box_pack_start(GTK_BOX(middle), apps, false, false, 0);
	gtk_box_pack_start(GTK_BOX(middle), search, true, true, 0);
	gtk_box_set_center_widget(GTK_BOX(row), middle);
	CHECK(gtk_box_get_center_widget(GTK_BOX(row)) == middle);
	CHECK(gtk_widget_get_parent(apps) == middle);
	CHECK(gtk_widget_get_parent(search) == middle);
	CHECK(gtk_box_get_spacing(GTK_BOX(middle)) > 0);
	CHECK(MEOWMENU_LAUNCHER_SHADOW_TYPE == GTK_SHADOW_NONE);

	GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* secondary = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* selector = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkToggleButton* apps_mode = GTK_TOGGLE_BUTTON(
			gtk_toggle_button_new_with_label("Applications"));
	GtkToggleButton* places_mode = GTK_TOGGLE_BUTTON(
			gtk_toggle_button_new_with_label("Places"));
	gtk_box_pack_start(GTK_BOX(selector), GTK_WIDGET(apps_mode), true, true, 0);
	gtk_box_pack_start(GTK_BOX(selector), GTK_WIDGET(places_mode), true, true, 0);
	gtk_toggle_button_set_active(places_mode, TRUE);
	gtk_entry_set_text(GTK_ENTRY(search), "persistent query");
	atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(apps_mode)),
			"Applications");
	atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(places_mode)),
			"Places");
	g_object_ref_sink(selector);
	gtk_box_pack_start(GTK_BOX(sidebar), selector, false, false, 0);
	for (int cycle = 0; cycle < 20; ++cycle)
	{
		GtkWidget* target = (cycle % 2) == 0 ? secondary : sidebar;
		g_object_ref(selector);
		GtkWidget* parent = gtk_widget_get_parent(selector);
		if (parent)
			gtk_container_remove(GTK_CONTAINER(parent), selector);
		gtk_box_pack_start(GTK_BOX(target), selector, false, false, 0);
		gtk_box_reorder_child(GTK_BOX(target), selector, 0);
		g_object_unref(selector);
		CHECK(gtk_widget_get_parent(selector) == target);
		CHECK(!gtk_toggle_button_get_active(apps_mode));
		CHECK(gtk_toggle_button_get_active(places_mode));
		CHECK(g_strcmp0(gtk_entry_get_text(GTK_ENTRY(search)),
				"persistent query") == 0);
		CHECK(g_strcmp0(atk_object_get_name(
				gtk_widget_get_accessible(GTK_WIDGET(places_mode))),
				"Places") == 0);
		GList* children = gtk_container_get_children(GTK_CONTAINER(target));
		CHECK(children && children->data == selector);
		g_list_free(children);
	}
	GtkWidget* parent = gtk_widget_get_parent(selector);
	gtk_container_remove(GTK_CONTAINER(parent), selector);
	CHECK(gtk_widget_get_parent(selector) == nullptr);
	gtk_widget_set_can_focus(GTK_WIDGET(apps_mode), FALSE);
	gtk_widget_set_can_focus(GTK_WIDGET(places_mode), FALSE);
	CHECK(!gtk_widget_get_can_focus(GTK_WIDGET(apps_mode)));
	CHECK(!gtk_widget_get_can_focus(GTK_WIDGET(places_mode)));
	g_object_unref(selector);
	gtk_widget_destroy(sidebar);
	gtk_widget_destroy(secondary);
	gtk_widget_destroy(row);

	if (failures != 0)
	{
		std::fprintf(stderr,
				"test_window_mode_integration: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	std::printf("test_window_mode_integration: ok\n");
	return EXIT_SUCCESS;
}
