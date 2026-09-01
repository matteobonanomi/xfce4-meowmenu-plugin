/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ui/launcher-icon-view.h"
#include "ui/grid-cell-metrics.h"
#include "ui/icon-renderer.h"
#include "core/window-frame.h"

#include <gtk/gtk.h>

#include <cstdio>
#include <cstdlib>

namespace
{

int failures = 0;
int activations = 0;

struct PresentationState
{
	GtkIconView* view;
	GtkCellRenderer* renderer;
	guint64 generation;
	WhiskerMenu::LauncherIconPresentationState* state;
};

#define CHECK(condition) do { \
		if (!(condition)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", \
					__FILE__, __LINE__, #condition); \
			++failures; \
		} \
	} while (0)

void drain_events()
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
}

/* check_hidden_populated_layout:
 *
 * Reproduces the production ordering used by Favourites: configure the real
 * cell renderer and automatic grid layout, attach a complete model while its
 * page is hidden, then reveal it without any row mutation. Presentation is
 * complete only when existing paths have usable cell geometry; draw counts and
 * model row counts cannot establish that boundary.
 */
void check_hidden_populated_layout()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	gtk_window_set_default_size(GTK_WINDOW(window), 420, 260);
	GtkWidget* stack = gtk_stack_new();
	GtkWidget* loading = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* page = gtk_scrolled_window_new(nullptr, nullptr);
	GtkWidget* view_widget = gtk_icon_view_new();
	GtkIconView* view = GTK_ICON_VIEW(view_widget);
	gtk_container_add(GTK_CONTAINER(page), view_widget);
	gtk_stack_add_named(GTK_STACK(stack), loading, "loading");
	gtk_stack_add_named(GTK_STACK(stack), page, "page");
	gtk_container_add(GTK_CONTAINER(window), stack);

	GtkCellRenderer* renderer = whiskermenu_icon_renderer_new();
	g_object_set(renderer,
			"stretch", true,
			"size", 48,
			"spacing", 4,
			"label-lines", 2,
			nullptr);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(view), renderer, false);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(view), renderer,
			"gicon", WhiskerMenu::LauncherView::COLUMN_ICON,
			"launcher", WhiskerMenu::LauncherView::COLUMN_LAUNCHER,
			nullptr);
	gtk_icon_view_set_markup_column(view,
			WhiskerMenu::LauncherView::COLUMN_TEXT);
	gtk_icon_view_set_item_padding(view, 4);
	gtk_icon_view_set_row_spacing(view, 4);
	gtk_icon_view_set_column_spacing(view, 4);
	gtk_icon_view_set_margin(view, 6);
	gtk_icon_view_set_selection_mode(view, GTK_SELECTION_SINGLE);
	WhiskerMenu::LauncherIconPresentationState layout_state;

	GtkListStore* model = gtk_list_store_new(
			WhiskerMenu::LauncherView::N_COLUMNS,
			G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	GIcon* icon = g_themed_icon_new("application-x-executable");
	for (int row = 0; row < 12; ++row)
	{
		gchar* text = g_strdup_printf("Application %d", row + 1);
		gtk_list_store_insert_with_values(model, nullptr, -1,
				WhiskerMenu::LauncherView::COLUMN_ICON, icon,
				WhiskerMenu::LauncherView::COLUMN_TEXT, text,
				WhiskerMenu::LauncherView::COLUMN_TOOLTIP, text,
				WhiskerMenu::LauncherView::COLUMN_LAUNCHER, nullptr,
				-1);
		g_free(text);
	}
	g_object_unref(icon);
	PresentationState presentation_state = {
			view, renderer, 1, &layout_state };
	g_signal_connect_after(view_widget, "draw",
			G_CALLBACK(+[](GtkWidget*, cairo_t*, gpointer data) -> gboolean
			{
				auto* presentation = static_cast<PresentationState*>(data);
				WhiskerMenu::launcher_icon_view_complete_layout(
						presentation->view, presentation->renderer,
						presentation->generation, presentation->state);
				return GDK_EVENT_PROPAGATE;
			}), &presentation_state);
	gtk_widget_show_all(window);
	gtk_stack_set_visible_child_name(GTK_STACK(stack), "loading");
	drain_events();
	gtk_icon_view_set_model(view, GTK_TREE_MODEL(model));
	WhiskerMenu::launcher_icon_view_apply_automatic_layout(view, 48, 420);
	CHECK(gtk_icon_view_get_columns(view) == -1);
	CHECK(gtk_icon_view_get_item_width(view) == -1);
	CHECK(!WhiskerMenu::launcher_icon_view_prepare_layout(
			view, 1, &layout_state));
	CHECK(layout_state.prepared_generation == 0);
	CHECK(layout_state.layout_requests == 1);
	guint mapped_frame_id = 0;
	CHECK(meow::meowmenu_schedule_mapped_result_frame(window, window,
			view_widget, &mapped_frame_id,
			+[](void* data) -> bool
			{
				auto* state = static_cast<PresentationState*>(data);
				return WhiskerMenu::launcher_icon_view_prepare_layout(state->view,
						state->generation, state->state);
			}, &presentation_state));
	gtk_stack_set_visible_child_name(GTK_STACK(stack), "page");
	gtk_test_widget_wait_for_draw(window);
	gtk_test_widget_wait_for_draw(view_widget);
	drain_events();
	for (int frame = 0; mapped_frame_id != 0 && frame < 8; ++frame)
	{
		gtk_test_widget_wait_for_draw(window);
		drain_events();
	}
	CHECK(gtk_widget_get_mapped(view_widget));
	CHECK(mapped_frame_id == 0);
	CHECK(layout_state.prepared_generation == 1);
	CHECK(layout_state.layout_requests == 2);
	CHECK(layout_state.mapped_layout_requests == 1);
	CHECK(layout_state.completions == 1);
	gtk_test_widget_wait_for_draw(view_widget);
	drain_events();

	GtkTreePath* first = gtk_tree_path_new_from_indices(0, -1);
	GtkTreePath* last = gtk_tree_path_new_from_indices(11, -1);
	GdkRectangle first_rect = {};
	GdkRectangle last_rect = {};
	CHECK(gtk_icon_view_get_cell_rect(view, first, renderer, &first_rect));
	CHECK(first_rect.width > 0 && first_rect.height > 0);
	CHECK(gtk_icon_view_get_cell_rect(view, last, renderer, &last_rect));
	CHECK(last_rect.width > 0 && last_rect.height > 0);

	const int columns = gtk_icon_view_get_columns(view);
	const int item_width = gtk_icon_view_get_item_width(view);
	int minimum_width = 0;
	int natural_width = 0;
	gtk_widget_get_preferred_width(view_widget, &minimum_width, &natural_width);
	GtkTreePath* selected_path = gtk_tree_path_new_from_indices(1, -1);
	gtk_icon_view_select_path(view, selected_path);
	GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(page));
	gtk_adjustment_set_value(adjustment, 24.0);
	const double scroll_value = gtk_adjustment_get_value(adjustment);
	const unsigned int layout_requests_before_warm =
			layout_state.layout_requests;
	const unsigned int mapped_requests_before_warm =
			layout_state.mapped_layout_requests;
	const unsigned int geometry_checks_before_warm =
			layout_state.geometry_checks;
	const unsigned int completions_before_warm = layout_state.completions;
	const unsigned int ready_reuses_before_warm = layout_state.ready_reuses;
	int warm_followup_frames = 0;
	for (int presentation = 0; presentation < 100; ++presentation)
	{
		const bool ready = WhiskerMenu::launcher_icon_view_prepare_layout(
				view, 1, &layout_state);
		CHECK(ready);
		if (!ready)
			++warm_followup_frames;
	}
	CHECK(layout_state.ready_reuses == ready_reuses_before_warm + 100);
	CHECK(warm_followup_frames == 0);
	CHECK(layout_state.layout_requests == layout_requests_before_warm);
	CHECK(layout_state.mapped_layout_requests == mapped_requests_before_warm);
	CHECK(layout_state.geometry_checks == geometry_checks_before_warm);
	CHECK(layout_state.completions == completions_before_warm);
	CHECK(gtk_icon_view_get_model(view) == GTK_TREE_MODEL(model));
	presentation_state.generation = 2;
	CHECK(!WhiskerMenu::launcher_icon_view_prepare_layout(
			view, 2, &layout_state));
	gtk_test_widget_wait_for_draw(view_widget);
	drain_events();
	CHECK(layout_state.prepared_generation == 2);
	CHECK(layout_state.completions == 2);
	CHECK(gtk_icon_view_path_is_selected(view, selected_path));
	CHECK(gtk_icon_view_get_columns(view) == columns);
	CHECK(gtk_icon_view_get_item_width(view) == item_width);
	int repeated_minimum_width = 0;
	int repeated_natural_width = 0;
	gtk_widget_get_preferred_width(view_widget,
			&repeated_minimum_width, &repeated_natural_width);
	CHECK(repeated_minimum_width == minimum_width);
	CHECK(repeated_natural_width == natural_width);
	CHECK(gtk_adjustment_get_value(adjustment) == scroll_value);

	gtk_tree_path_free(selected_path);
	gtk_tree_path_free(last);
	gtk_tree_path_free(first);
	gtk_widget_destroy(window);
	g_object_unref(window);
	g_object_unref(model);
}

/* check_applications_and_places_share_automatic_policy:
 *
 * Seeds two result grids with rejected explicit geometry, then exercises the
 * selected policy through 40 representative viewport changes. Applications
 * and Places must both return to GTK ownership without retaining stale values.
 */
void check_applications_and_places_share_automatic_policy()
{
	GtkIconView* applications = GTK_ICON_VIEW(gtk_icon_view_new());
	GtkIconView* places = GTK_ICON_VIEW(gtk_icon_view_new());
	g_object_ref_sink(applications);
	g_object_ref_sink(places);
	gtk_icon_view_set_columns(applications, 4);
	gtk_icon_view_set_item_width(applications, 128);
	gtk_icon_view_set_columns(places, 3);
	gtk_icon_view_set_item_width(places, 144);

	for (int transition = 0; transition < 40; ++transition)
	{
		const int viewport_width = 320 + (transition * 17);
		WhiskerMenu::launcher_icon_view_apply_automatic_layout(
				applications, 48, viewport_width);
		WhiskerMenu::launcher_icon_view_apply_automatic_layout(
				places, 48, viewport_width);
		CHECK(gtk_icon_view_get_columns(applications) == -1);
		CHECK(gtk_icon_view_get_item_width(applications) == -1);
		CHECK(gtk_icon_view_get_columns(places) == -1);
		CHECK(gtk_icon_view_get_item_width(places) == -1);
	}

	gtk_widget_destroy(GTK_WIDGET(places));
	gtk_widget_destroy(GTK_WIDGET(applications));
	g_object_unref(places);
	g_object_unref(applications);
}

}

int main()
{
	if (!gtk_init_check(nullptr, nullptr))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}
	check_hidden_populated_layout();
	check_applications_and_places_share_automatic_policy();

	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget* view_widget = gtk_icon_view_new();
	g_object_ref_sink(window);
	GtkIconView* view = GTK_ICON_VIEW(view_widget);
	GtkListStore* model = gtk_list_store_new(1, G_TYPE_STRING);
	for (const char* text : {"One", "Two", "Three", "Four", "Five"})
		gtk_list_store_insert_with_values(model, nullptr, -1, 0, text, -1);
	gtk_icon_view_set_model(view, GTK_TREE_MODEL(model));
	gtk_icon_view_set_text_column(view, 0);
	gtk_icon_view_set_item_width(view, 80);
	gtk_icon_view_set_columns(view, 2);
	gtk_icon_view_set_selection_mode(view, GTK_SELECTION_SINGLE);
	gtk_container_add(GTK_CONTAINER(window), view_widget);

	g_signal_connect(view, "item-activated",
			G_CALLBACK(+[](GtkIconView*, GtkTreePath*, gpointer)
			{
				++activations;
			}), nullptr);
	gtk_widget_show_all(window);
	while (gtk_events_pending())
		gtk_main_iteration();

	WhiskerMenu::launcher_icon_view_set_transparent_grid_style(
			view_widget, true);
	CHECK(gtk_style_context_has_class(
			gtk_widget_get_style_context(view_widget), "transparent-grid"));

	GtkTreePath* first = gtk_tree_path_new_from_indices(0, -1);
	gtk_icon_view_select_path(view, first);
	gtk_icon_view_set_cursor(view, first, nullptr, false);
	gtk_container_set_focus_child(GTK_CONTAINER(window), view_widget);
	CHECK(gtk_container_get_focus_child(GTK_CONTAINER(window)) == view_widget);

	GtkTreePath* origin = gtk_tree_path_new_from_indices(0, -1);
	GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(view));
	GtkCellRenderer* renderer = cells
			? GTK_CELL_RENDERER(cells->data) : nullptr;
	GtkTreePath* right = WhiskerMenu::launcher_icon_view_find_directional_path(
			view, renderer, origin,
			WhiskerMenu::Keyboard::PhysicalDirection::Right, false);
	GtkTreePath* expected_right = gtk_tree_path_new_from_indices(1, -1);
	CHECK(right && gtk_tree_path_compare(right, expected_right) == 0);
	gtk_tree_path_free(right);
	gtk_tree_path_free(expected_right);

	GtkTreePath* down = WhiskerMenu::launcher_icon_view_find_directional_path(
			view, renderer, origin,
			WhiskerMenu::Keyboard::PhysicalDirection::Down, false);
	GtkTreePath* expected_down = gtk_tree_path_new_from_indices(2, -1);
	CHECK(down && gtk_tree_path_compare(down, expected_down) == 0);
	gtk_tree_path_free(down);
	gtk_tree_path_free(expected_down);

	GtkTreePath* second = gtk_tree_path_new_from_indices(1, -1);
	GtkTreePath* left = WhiskerMenu::launcher_icon_view_find_directional_path(
			view, renderer, second,
			WhiskerMenu::Keyboard::PhysicalDirection::Left, false);
	GtkTreePath* expected_left = gtk_tree_path_new_from_indices(0, -1);
	CHECK(left && gtk_tree_path_compare(left, expected_left) == 0);
	gtk_tree_path_free(left);
	gtk_tree_path_free(expected_left);

	GtkTreePath* third = gtk_tree_path_new_from_indices(2, -1);
	GtkTreePath* up = WhiskerMenu::launcher_icon_view_find_directional_path(
			view, renderer, third,
			WhiskerMenu::Keyboard::PhysicalDirection::Up, false);
	CHECK(up && gtk_tree_path_compare(up, first) == 0);
	gtk_tree_path_free(up);
	gtk_tree_path_free(third);

	GtkTreePath* wrapped = WhiskerMenu::launcher_icon_view_find_directional_path(
			view, renderer, second,
			WhiskerMenu::Keyboard::PhysicalDirection::Right, false);
	CHECK(!wrapped);
	gtk_tree_path_free(wrapped);
	gtk_tree_path_free(second);

	CHECK(WhiskerMenu::launcher_icon_view_apply_keyboard_target(
			view, GTK_TREE_MODEL(model), origin));
	CHECK(gtk_icon_view_path_is_selected(view, origin));
	g_list_free(cells);
	gtk_tree_path_free(origin);

	GdkEventButton event = {};
	event.type = GDK_BUTTON_RELEASE;
	event.button = 1;
	event.x = -1;
	event.y = -1;

	CHECK(WhiskerMenu::launcher_icon_view_complete_empty_click(
			view, true, &event));
	GList* selected = gtk_icon_view_get_selected_items(view);
	CHECK(selected == nullptr);
	g_list_free_full(selected,
			reinterpret_cast<GDestroyNotify>(&gtk_tree_path_free));
	CHECK(gtk_container_get_focus_child(GTK_CONTAINER(window)) == view_widget);
	CHECK(activations == 0);

	CHECK(WhiskerMenu::launcher_icon_view_complete_empty_click(
			view, true, &event));
	CHECK(!WhiskerMenu::launcher_icon_view_complete_empty_click(
			view, false, &event));
	event.button = 3;
	CHECK(!WhiskerMenu::launcher_icon_view_complete_empty_click(
			view, true, &event));

	WhiskerMenu::launcher_icon_view_set_transparent_grid_style(
			view_widget, false);
	CHECK(!gtk_style_context_has_class(
			gtk_widget_get_style_context(view_widget), "transparent-grid"));

	gtk_icon_view_set_item_padding(view, 4);
	gtk_icon_view_set_row_spacing(view, 4);
	gtk_icon_view_set_column_spacing(view, 4);
	gtk_icon_view_set_margin(view, 6);
	WhiskerMenu::launcher_icon_view_apply_automatic_layout(view, 48, 420);
	CHECK(gtk_widget_get_hexpand(view_widget));
	CHECK(gtk_icon_view_get_columns(view) == -1);
	CHECK(gtk_icon_view_get_item_width(view) == -1);
	int minimum_width = 0;
	int natural_width = 0;
	gtk_widget_get_preferred_width(view_widget,
			&minimum_width, &natural_width);

	// A large category model must retain GTK-owned columns and cell width across
	// every accepted viewport. No rejected explicit policy may remain active.
	for (int i = 0; i < 2000; ++i)
		gtk_list_store_insert_with_values(model, nullptr, -1,
				0, "Additional application", -1);
	for (int width : {240, 260, 300, 360, 420, 480, 568, 720, 960, 1200})
	{
		WhiskerMenu::launcher_icon_view_apply_automatic_layout(
				view, 48, width);
		CHECK(gtk_icon_view_get_columns(view) == -1);
		CHECK(gtk_icon_view_get_item_width(view) == -1);
		CHECK(gtk_icon_view_get_model(view) == GTK_TREE_MODEL(model));
	}
	while (gtk_events_pending())
		gtk_main_iteration();
	CHECK(gtk_icon_view_get_columns(view) == -1);
	CHECK(gtk_icon_view_get_item_width(view) == -1);
	CHECK(gtk_icon_view_get_model(view) == GTK_TREE_MODEL(model));
	GtkListStore* transition_model = gtk_list_store_new(1, G_TYPE_STRING);
	for (int row = 0; row < 64; ++row)
	{
		gtk_list_store_insert_with_values(transition_model, nullptr, -1,
				0, "Application", -1);
	}
	gtk_icon_view_set_model(view, GTK_TREE_MODEL(transition_model));
	WhiskerMenu::LauncherIconPresentationState docked_presentation;
	int docked_minimum_width = 0;
	int docked_natural_width = 0;
	for (int switch_index = 1; switch_index <= 40; ++switch_index)
	{
		gtk_icon_view_set_model(view, nullptr);
		while (gtk_events_pending())
			gtk_main_iteration();
		const int overshoot = switch_index * 23;
		const int effective_width =
				WhiskerMenu::meow_grid_effective_viewport_width(
						420 + overshoot, 450 + overshoot, 450);
		WhiskerMenu::launcher_icon_view_apply_automatic_layout(
				view, 48, effective_width);
		gtk_icon_view_set_model(view, GTK_TREE_MODEL(transition_model));
		CHECK(!WhiskerMenu::launcher_icon_view_prepare_layout(view,
				static_cast<guint64>(switch_index),
				&docked_presentation));
		while (gtk_events_pending())
			gtk_main_iteration();
		CHECK(WhiskerMenu::launcher_icon_view_complete_layout(view, nullptr,
				static_cast<guint64>(switch_index), &docked_presentation));
		CHECK(effective_width == 420);
		gtk_widget_get_preferred_width(view_widget,
				&minimum_width, &natural_width);
		CHECK(gtk_icon_view_get_columns(view) == -1);
		CHECK(gtk_icon_view_get_item_width(view) == -1);
		if (switch_index == 1)
		{
			docked_minimum_width = minimum_width;
			docked_natural_width = natural_width;
		}
		else
		{
			CHECK(std::abs(minimum_width - docked_minimum_width) <= 1);
			CHECK(std::abs(natural_width - docked_natural_width) <= 1);
		}
	}
	const int fullscreen_toplevel_width = 1920;
	const int fullscreen_result_cap = 1280;
	int fullscreen_columns = 0;
	int fullscreen_item_width = 0;
	int fullscreen_minimum_width = 0;
	int fullscreen_natural_width = 0;
	WhiskerMenu::LauncherIconPresentationState fullscreen_presentation;
	for (int switch_index = 1; switch_index <= 40; ++switch_index)
	{
		gtk_icon_view_set_model(view, nullptr);
		const int overshoot = switch_index * 29;
		const int effective_width =
				WhiskerMenu::meow_grid_effective_viewport_width(
						fullscreen_result_cap + overshoot,
						fullscreen_toplevel_width + overshoot,
						fullscreen_toplevel_width,
						fullscreen_result_cap);
		WhiskerMenu::launcher_icon_view_apply_automatic_layout(
				view, 48, effective_width);
		gtk_icon_view_set_model(view, GTK_TREE_MODEL(transition_model));
		CHECK(!WhiskerMenu::launcher_icon_view_prepare_layout(view,
				static_cast<guint64>(switch_index),
				&fullscreen_presentation));
		while (gtk_events_pending())
			gtk_main_iteration();
		CHECK(WhiskerMenu::launcher_icon_view_complete_layout(view, nullptr,
				static_cast<guint64>(switch_index), &fullscreen_presentation));
		CHECK(effective_width == fullscreen_result_cap);
		gtk_widget_get_preferred_width(view_widget,
				&minimum_width, &natural_width);
		CHECK(gtk_icon_view_get_columns(view) == -1);
		CHECK(gtk_icon_view_get_item_width(view) == -1);
		if (switch_index == 1)
		{
			fullscreen_columns = gtk_icon_view_get_columns(view);
			fullscreen_item_width = gtk_icon_view_get_item_width(view);
			fullscreen_minimum_width = minimum_width;
			fullscreen_natural_width = natural_width;
		}
		else
		{
			CHECK(gtk_icon_view_get_columns(view) == fullscreen_columns);
			CHECK(gtk_icon_view_get_item_width(view)
					== fullscreen_item_width);
			CHECK(std::abs(minimum_width - fullscreen_minimum_width) <= 1);
			CHECK(std::abs(natural_width - fullscreen_natural_width) <= 1);
		}
	}
	gtk_icon_view_set_model(view, GTK_TREE_MODEL(model));
	g_object_unref(transition_model);

	gtk_tree_path_free(first);
	gtk_widget_destroy(window);
	g_object_unref(window);
	g_object_unref(model);

	if (failures != 0)
	{
		std::fprintf(stderr, "test_launcher_icon_view: %d failure(s)\n",
				failures);
		return EXIT_FAILURE;
	}
	std::printf("test_launcher_icon_view: ok\n");
	return EXIT_SUCCESS;
}
