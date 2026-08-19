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

#include <gtk/gtk.h>

#include <cstdio>
#include <cstdlib>

namespace
{

int failures = 0;
int activations = 0;

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
	if (!gtk_init_check(nullptr, nullptr))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

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
	WhiskerMenu::launcher_icon_view_apply_grid_width(view, 48, 420);
	CHECK(gtk_widget_get_hexpand(view_widget));
	CHECK(gtk_icon_view_get_columns(view) == 3);
	CHECK(gtk_icon_view_get_item_width(view) == 125);
	int minimum_width = 0;
	int natural_width = 0;
	gtk_widget_get_preferred_width(view_widget,
			&minimum_width, &natural_width);
	CHECK(minimum_width <= 420);
	CHECK(natural_width <= 420);
	WhiskerMenu::launcher_icon_view_apply_grid_width(view, 48, 568);
	CHECK(gtk_icon_view_get_columns(view) == 4);
	CHECK(gtk_icon_view_get_item_width(view) == 128);

	// A large category model must not become a width authority. Reapplying the
	// same external Results width stays idempotent regardless of row count.
	for (int i = 0; i < 2000; ++i)
		gtk_list_store_insert_with_values(model, nullptr, -1,
				0, "Additional application", -1);
	for (int i = 0; i < 8; ++i)
	{
		WhiskerMenu::launcher_icon_view_apply_grid_width(view, 48, 420);
		CHECK(gtk_icon_view_get_columns(view) == 3);
		CHECK(gtk_icon_view_get_item_width(view) == 125);
	}
	while (gtk_events_pending())
		gtk_main_iteration();
	CHECK(gtk_icon_view_get_columns(view) == 3);
	CHECK(gtk_icon_view_get_item_width(view) == 125);
	gtk_widget_get_preferred_width(view_widget,
			&minimum_width, &natural_width);
	CHECK(minimum_width <= 420);
	CHECK(natural_width <= 420);
	int live_width = WhiskerMenu::meow_grid_resized_viewport_width(
			420, 450, 598);
	WhiskerMenu::launcher_icon_view_apply_grid_width(view, 48, live_width);
	gtk_widget_get_preferred_width(view_widget,
			&minimum_width, &natural_width);
	CHECK(live_width == 568);
	CHECK(minimum_width <= 568);
	CHECK(natural_width <= 568);
	live_width = WhiskerMenu::meow_grid_resized_viewport_width(
			live_width, 598, 450);
	WhiskerMenu::launcher_icon_view_apply_grid_width(view, 48, live_width);
	gtk_widget_get_preferred_width(view_widget,
			&minimum_width, &natural_width);
	CHECK(live_width == 420);
	CHECK(minimum_width <= 420);
	CHECK(natural_width <= 420);
	CHECK(gtk_icon_view_get_model(view) == GTK_TREE_MODEL(model));
	for (int width : {240, 260, 300, 360, 420, 480, 568, 720, 960, 1200})
	{
		WhiskerMenu::launcher_icon_view_apply_grid_width(view, 48, width);
		const WhiskerMenu::GridCellMetrics cell =
				WhiskerMenu::meow_grid_cell_metrics(4, 48, 4, true, 2);
		const WhiskerMenu::GridColumnLayout expected =
				WhiskerMenu::meow_grid_column_layout(
						width, 6, 4, 4, cell.minimum_width);
		CHECK(gtk_icon_view_get_columns(view) == expected.columns);
		CHECK(gtk_icon_view_get_item_width(view) == expected.item_width);
		CHECK(gtk_icon_view_get_model(view) == GTK_TREE_MODEL(model));
	}
	WhiskerMenu::launcher_icon_view_apply_grid_width(view, 48, 420);
	CHECK(gtk_icon_view_get_columns(view) == 3);
	CHECK(gtk_icon_view_get_item_width(view) == 125);
	for (int switch_index = 1; switch_index <= 40; ++switch_index)
	{
		gtk_icon_view_set_model(view, nullptr);
		while (gtk_events_pending())
			gtk_main_iteration();
		const int overshoot = switch_index * 23;
		const int effective_width =
				WhiskerMenu::meow_grid_effective_viewport_width(
						420 + overshoot, 450 + overshoot, 450);
		WhiskerMenu::launcher_icon_view_apply_grid_width(
				view, 48, effective_width);
		gtk_icon_view_set_model(view, GTK_TREE_MODEL(model));
		while (gtk_events_pending())
			gtk_main_iteration();
		CHECK(effective_width == 420);
		gtk_widget_get_preferred_width(view_widget,
				&minimum_width, &natural_width);
		CHECK(minimum_width <= 420);
		CHECK(natural_width <= 420);
	}

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
