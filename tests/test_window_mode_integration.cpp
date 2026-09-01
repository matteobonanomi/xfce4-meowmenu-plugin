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
#include "core/window-geometry.h"
#include "core/window-pages.h"
#include "core/user-session-relayout.h"
#include "launcher/command.h"
#include "launcher/page.h"
#include "settings.h"

#include <gtk/gtk.h>

#include <algorithm>
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

struct ComposedWindowDraw
{
	GdkRGBA background = { 1.0, 1.0, 1.0, 1.0 };
	int separator_y = -1;
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

void check_contents_frame_margin_follows_adjacent_secondary_band()
{
	auto resolve = [](PrimaryEdge edge, CompositionSidebar sidebar,
			bool profile, bool session) -> MenuContentMargins
	{
		const MenuComposition composition = meow_resolve_menu_composition({
				LayoutMode::Centered, edge, sidebar, profile, session,
				session ? 2U : 0U, true, MenuDirection::LeftToRight });
		return meow_resolve_contents_frame_margins(composition, 6);
	};
	MenuContentMargins margins = resolve(PrimaryEdge::Top,
			CompositionSidebar::Left, true, true);
	CHECK(margins.top == 0);
	CHECK(margins.bottom == 6);
	margins = resolve(PrimaryEdge::Bottom,
			CompositionSidebar::Left, true, true);
	CHECK(margins.top == 6);
	CHECK(margins.bottom == 0);
	margins = resolve(PrimaryEdge::Top,
			CompositionSidebar::Horizontal, true, true);
	CHECK(margins.top == 0);
	CHECK(margins.bottom == 6);
	margins = resolve(PrimaryEdge::Bottom,
			CompositionSidebar::Horizontal, true, true);
	CHECK(margins.top == 6);
	CHECK(margins.bottom == 0);
	// Here Primary separates Results from the lower secondary band.
	margins = resolve(PrimaryEdge::Bottom,
			CompositionSidebar::Hidden, false, true);
	CHECK(margins.top == 0);
	CHECK(margins.bottom == 0);
	const MenuComposition no_secondary = meow_resolve_menu_composition({
			LayoutMode::Centered, PrimaryEdge::Top,
			CompositionSidebar::Left, true, false, 0, false,
			MenuDirection::LeftToRight });
	margins = meow_resolve_contents_frame_margins(no_secondary, 6);
	CHECK(margins.top == 0);
	CHECK(margins.bottom == 0);
}

/* draw_composed_window:
 * @widget: test toplevel with the same single root-child shape as Window.
 * @cr: current complete-window draw transaction.
 * @data: background colour for the semantic surface pass.
 *
 * Mirrors the launcher's manual root-child propagation so the regression
 * exercises native GtkIconView scrolling under the production draw topology.
 *
 * Returns: GDK_EVENT_STOP because the root child is propagated exactly once.
 */
gboolean draw_composed_window(GtkWidget* widget, cairo_t* cr, gpointer data)
{
	auto* draw = static_cast<ComposedWindowDraw*>(data);
	gdk_cairo_set_source_rgba(cr, &draw->background);
	cairo_paint(cr);
	if (draw->separator_y >= 0)
	{
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_rectangle(cr, 0, draw->separator_y, 420, 1);
		cairo_fill(cr);
	}
	GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
	if (child)
		gtk_container_propagate_draw(GTK_CONTAINER(widget), child, cr);
	return GDK_EVENT_STOP;
}

bool surface_has_red_pixel(cairo_surface_t* surface, int top, int bottom)
{
	cairo_surface_flush(surface);
	const int width = cairo_image_surface_get_width(surface);
	const int height = cairo_image_surface_get_height(surface);
	const int stride = cairo_image_surface_get_stride(surface);
	const unsigned char* pixels = cairo_image_surface_get_data(surface);
	for (int y = std::max(0, top); y < std::min(height, bottom); ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const unsigned char* pixel = pixels + y * stride + x * 4;
			if (pixel[2] > 200 && pixel[1] < 60 && pixel[0] < 60)
				return true;
		}
	}
	return false;
}

void check_results_clip_tracks_real_grid_boundary()
{
	if (!gtk_init_check(nullptr, nullptr))
		return;
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* grid = gtk_grid_new();
	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_hexpand(vbox, TRUE);
	gtk_widget_set_vexpand(vbox, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	GtkWidget* primary = gtk_drawing_area_new();
	gtk_widget_set_size_request(primary, -1, 40);
	GtkWidget* contents = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* secondary = gtk_drawing_area_new();
	gtk_widget_set_size_request(secondary, -1, 40);
	GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
	GtkListStore* model = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 48, 48);
	gdk_pixbuf_fill(pixbuf, 0xff0000ff);
	for (int i = 0; i < 30; ++i)
	{
		GtkTreeIter iter;
		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter, 0, pixbuf, 1, "Application", -1);
	}
	GtkWidget* result = gtk_icon_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(result), 0);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(result), 1);
	gtk_icon_view_set_columns(GTK_ICON_VIEW(result), 3);
	gtk_icon_view_set_item_width(GTK_ICON_VIEW(result), 88);
	gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(result), 12);
	gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(result), 12);
	DrawClipCapture capture;
	const MenuContentMargins margins = meow_resolve_contents_frame_margins(
			meow_resolve_menu_composition({ LayoutMode::Centered,
					PrimaryEdge::Top, CompositionSidebar::Left, true, true,
					2, true, MenuDirection::LeftToRight }), 6);
	gtk_widget_set_margin_top(contents, margins.top);
	gtk_widget_set_margin_bottom(contents, margins.bottom);
	gtk_widget_set_size_request(scroller, 320, -1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scroller), result);
	gtk_box_pack_start(GTK_BOX(contents), scroller, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), primary, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), contents, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), secondary, FALSE, FALSE, 0);
	GtkWidget* top_handle = gtk_drawing_area_new();
	GtkWidget* bottom_handle = gtk_drawing_area_new();
	GtkWidget* left_handle = gtk_drawing_area_new();
	GtkWidget* right_handle = gtk_drawing_area_new();
	gtk_widget_set_size_request(top_handle, 6, 6);
	gtk_widget_set_size_request(bottom_handle, 6, 6);
	gtk_widget_set_size_request(left_handle, 6, 6);
	gtk_widget_set_size_request(right_handle, 6, 6);
	gtk_grid_attach(GTK_GRID(grid), top_handle, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), left_handle, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), vbox, 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), right_handle, 2, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), bottom_handle, 1, 2, 1, 1);
	g_signal_connect(result, "draw",
			G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer data) -> gboolean
			{
				auto* clip = static_cast<DrawClipCapture*>(data);
				clip->seen = true;
				cairo_clip_extents(cr, &clip->x1, &clip->y1,
						&clip->x2, &clip->y2);
				return GDK_EVENT_PROPAGATE;
			}), &capture);
	gtk_container_add(GTK_CONTAINER(window), grid);
	ComposedWindowDraw composed_draw;
	g_signal_connect(window, "draw", G_CALLBACK(draw_composed_window),
			&composed_draw);
	gtk_window_set_default_size(GTK_WINDOW(window), 420, 280);
	gtk_widget_set_size_request(window, 420, 280);
	gtk_widget_show_all(window);
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	GdkWindow* native_window = gtk_widget_get_window(window);
	if (cairo_region_t* stale = gdk_window_get_update_area(native_window))
		cairo_region_destroy(stale);
	CHECK(meow::meowmenu_queue_complete_window_frame(window));
	CHECK(!meow::meowmenu_queue_complete_window_frame(nullptr));
	cairo_region_t* queued = gdk_window_get_update_area(native_window);
	CHECK(queued != nullptr);
	if (queued)
		cairo_region_destroy(queued);
	GtkAllocation window_allocation = { 0, 0, 420, 280 };
	gtk_widget_size_allocate(window, &window_allocation);
	int secondary_x = 0;
	int secondary_y = 0;
	CHECK(gtk_widget_translate_coordinates(secondary, window, 0, 0,
			&secondary_x, &secondary_y));
	const MenuChromeGeometry geometry = meow_resolve_chrome_geometry(
			meow_resolve_menu_composition({ LayoutMode::Centered,
					PrimaryEdge::Top, CompositionSidebar::Left, true, true,
					2, true, MenuDirection::LeftToRight }), 420, 280, 6,
			{ 0, 0, 0, 0, false }, { 0, 0, 0, 0, false },
			{ 0, 0, 0, 0, false },
			{ secondary_x, secondary_y,
					gtk_widget_get_allocated_width(secondary),
					gtk_widget_get_allocated_height(secondary), true });
	CHECK(geometry.separator.visible);
	composed_draw.separator_y = geometry.separator.y;
	auto draw_window = [&]() -> cairo_surface_t*
	{
		cairo_surface_t* surface = cairo_image_surface_create(
				CAIRO_FORMAT_ARGB32, 420, 280);
		cairo_t* cr = cairo_create(surface);
		gtk_widget_draw(window, cr);
		cairo_destroy(cr);
		return surface;
	};
	cairo_surface_t* surface = draw_window();
	GtkAllocation allocation = {};
	gtk_widget_get_allocation(scroller, &allocation);
	int results_x = 0;
	int results_y = 0;
	CHECK(gtk_widget_translate_coordinates(scroller, window, 0, 0,
			&results_x, &results_y));
	const int results_boundary = results_y + allocation.height;
	CHECK(results_boundary == geometry.separator.y);
	CHECK(surface_has_red_pixel(surface, results_y, results_boundary));
	CHECK(!surface_has_red_pixel(surface, results_boundary, 280));
	cairo_surface_destroy(surface);
	CHECK(allocation.width >= 320);
	CHECK(allocation.height > 0);
	GtkAllocation clip = {};
	gtk_widget_get_clip(scroller, &clip);
	CHECK(clip.x == allocation.x);
	CHECK(clip.y == allocation.y);
	CHECK(clip.width == allocation.width);
	CHECK(clip.height == allocation.height);
	CHECK(capture.seen);
	CHECK(capture.x2 - capture.x1 <= allocation.width);
	CHECK(capture.y2 - capture.y1 <= allocation.height);
	GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(scroller));
	capture.seen = false;
	gtk_adjustment_set_value(adjustment, 45.0);
	gtk_widget_queue_draw(result);
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	gtk_widget_size_allocate(window, &window_allocation);
	surface = draw_window();
	CHECK(surface_has_red_pixel(surface, results_y, results_boundary));
	CHECK(!surface_has_red_pixel(surface, results_boundary, 280));
	cairo_surface_destroy(surface);
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
	CHECK(capture.x2 - capture.x1 <= allocation.width);
	CHECK(capture.y2 - capture.y1 <= allocation.height);
	gtk_widget_destroy(window);
	g_object_unref(window);
	g_object_unref(pixbuf);
	g_object_unref(model);
}

/* check_mapped_result_frame_orders_preparation_and_damage:
 *
 * Reproduces hidden model publication through the nested production stack and
 * verifies that mapped preparation precedes concrete and toplevel damage.
 */
void check_mapped_result_frame_orders_preparation_and_damage()
{
	if (!gtk_init_check(nullptr, nullptr))
		return;

	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* outer = gtk_stack_new();
	GtkWidget* loading = gtk_spinner_new();
	GtkWidget* contents = gtk_stack_new();
	GtkWidget* favorites_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* applications_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkListStore* model = gtk_list_store_new(1, G_TYPE_STRING);
	GtkListStore* applications_model = gtk_list_store_new(1, G_TYPE_STRING);
	for (int row = 0; row < 3; ++row)
	{
		GtkTreeIter iter;
		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter, 0, "Application", -1);
		gtk_list_store_append(applications_model, &iter);
		gtk_list_store_set(applications_model, &iter, 0, "Application", -1);
	}
	GtkWidget* result = gtk_icon_view_new();
	GtkWidget* applications_result = gtk_icon_view_new();
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(result), 0);
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(applications_result), 0);
	gtk_box_pack_start(GTK_BOX(favorites_page), result, true, true, 0);
	gtk_box_pack_start(GTK_BOX(applications_page), applications_result,
			true, true, 0);
	GtkWidget* favorites_heading = nullptr;
	GtkWidget* favorites_outer =
			meow::meowmenu_create_default_heading_page(favorites_page,
					"FAVORITES", &favorites_heading);
	gtk_stack_add_named(GTK_STACK(contents), favorites_outer, "favorites");
	gtk_stack_add_named(GTK_STACK(contents), applications_page, "applications");
	gtk_stack_add_named(GTK_STACK(outer), loading, "loading");
	gtk_stack_add_named(GTK_STACK(outer), contents, "contents");
	gtk_container_add(GTK_CONTAINER(window), outer);
	gtk_widget_show_all(window);
	gtk_stack_set_visible_child_name(GTK_STACK(outer), "loading");
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);

	// Populate while both the contents stack and concrete result are hidden.
	gtk_icon_view_set_model(GTK_ICON_VIEW(result), GTK_TREE_MODEL(model));
	gtk_icon_view_set_model(GTK_ICON_VIEW(applications_result),
			GTK_TREE_MODEL(applications_model));
	int toplevel_draws = 0;
	int result_draws = 0;
	int applications_draws = 0;
	g_signal_connect(window, "draw",
			G_CALLBACK(+[](GtkWidget*, cairo_t*, gpointer data) -> gboolean
			{
				++*static_cast<int*>(data);
				return GDK_EVENT_PROPAGATE;
			}), &toplevel_draws);
	g_signal_connect(result, "draw",
			G_CALLBACK(+[](GtkWidget*, cairo_t*, gpointer data) -> gboolean
			{
				++*static_cast<int*>(data);
				return GDK_EVENT_PROPAGATE;
			}), &result_draws);
	g_signal_connect(applications_result, "draw",
			G_CALLBACK(+[](GtkWidget*, cairo_t*, gpointer data) -> gboolean
			{
				++*static_cast<int*>(data);
				return GDK_EVENT_PROPAGATE;
			}), &applications_draws);
	guint favorites_frame_id = 0;
	int favorites_preparations = 0;
	CHECK(meow::meowmenu_queue_complete_result_frame(window, result));
	CHECK(meow::meowmenu_schedule_mapped_result_frame(window, window,
			result, &favorites_frame_id,
			+[](void* data) -> bool
			{
				++*static_cast<int*>(data);
				return true;
			}, &favorites_preparations));
	CHECK(favorites_frame_id != 0);
	CHECK(!meow::meowmenu_queue_complete_result_frame(nullptr, nullptr));
	gtk_widget_set_visible(favorites_heading, TRUE);
	gtk_stack_set_visible_child_name(GTK_STACK(contents), "favorites");
	gtk_stack_set_visible_child_name(GTK_STACK(outer), "contents");
	gtk_test_widget_wait_for_draw(window);
	gtk_test_widget_wait_for_draw(result);
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	CHECK(gtk_widget_get_mapped(result));
	CHECK(favorites_frame_id == 0);
	CHECK(favorites_preparations == 1);
	CHECK(gtk_widget_get_visible(favorites_heading));
	CHECK(std::string(gtk_label_get_text(GTK_LABEL(favorites_heading)))
			== "FAVORITES");
	CHECK(toplevel_draws > 0);
	CHECK(result_draws > 0);
	CHECK(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), nullptr) == 3);
	guint cancelled_frame_id = 0;
	int cancelled_preparations = 0;
	CHECK(meow::meowmenu_schedule_mapped_result_frame(window,
			window, applications_result, &cancelled_frame_id,
			+[](void* data) -> bool
			{
				++*static_cast<int*>(data);
				return true;
			}, &cancelled_preparations));
	CHECK(cancelled_frame_id != 0);
	meow::meowmenu_cancel_mapped_result_frame(window,
			&cancelled_frame_id);
	CHECK(cancelled_frame_id == 0);
	CHECK(cancelled_preparations == 0);
	guint applications_frame_id = 0;
	struct HiddenPreparation
	{
		GtkWidget* result;
		int calls;
	};
	HiddenPreparation applications_preparation = {
			applications_result, 0 };
	CHECK(meow::meowmenu_queue_complete_result_frame(window,
			applications_result));
	CHECK(meow::meowmenu_schedule_mapped_result_frame(window,
			window, applications_result, &applications_frame_id,
			+[](void* data) -> bool
			{
				auto* preparation = static_cast<HiddenPreparation*>(data);
				++preparation->calls;
				return gtk_widget_get_mapped(preparation->result);
			}, &applications_preparation));
	gtk_test_widget_wait_for_draw(window);
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	CHECK(applications_preparation.calls > 0);
	CHECK(applications_frame_id != 0);
	gtk_stack_set_visible_child_name(GTK_STACK(contents), "applications");
	gtk_test_widget_wait_for_draw(window);
	gtk_test_widget_wait_for_draw(applications_result);
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	for (int frame = 0; applications_frame_id != 0 && frame < 8; ++frame)
	{
		gtk_test_widget_wait_for_draw(window);
		while (g_main_context_pending(nullptr))
			g_main_context_iteration(nullptr, FALSE);
	}
	CHECK(gtk_widget_get_mapped(applications_result));
	CHECK(applications_frame_id == 0);
	CHECK(applications_preparation.calls > 1);
	CHECK(applications_draws > 0);
	CHECK(gtk_tree_model_iter_n_children(
			GTK_TREE_MODEL(applications_model), nullptr) == 3);

	GtkTreePath* favorites_selection = gtk_tree_path_new_from_indices(1, -1);
	GtkTreePath* applications_selection = gtk_tree_path_new_from_indices(2, -1);
	gtk_icon_view_select_path(GTK_ICON_VIEW(result), favorites_selection);
	gtk_icon_view_select_path(GTK_ICON_VIEW(applications_result),
			applications_selection);
	const int initial_width = gtk_widget_get_allocated_width(window);
	const int initial_height = gtk_widget_get_allocated_height(window);
	for (int transition = 0; transition < 40; ++transition)
	{
		gtk_stack_set_visible_child_name(GTK_STACK(contents),
				(transition % 2) == 0 ? "favorites" : "applications");
		gtk_test_widget_wait_for_draw(window);
		while (g_main_context_pending(nullptr))
			g_main_context_iteration(nullptr, FALSE);
		CHECK(gtk_icon_view_get_model(GTK_ICON_VIEW(result))
				== GTK_TREE_MODEL(model));
		CHECK(gtk_icon_view_get_model(GTK_ICON_VIEW(applications_result))
				== GTK_TREE_MODEL(applications_model));
		CHECK(gtk_icon_view_path_is_selected(GTK_ICON_VIEW(result),
				favorites_selection));
		CHECK(gtk_icon_view_path_is_selected(GTK_ICON_VIEW(applications_result),
				applications_selection));
		CHECK(gtk_widget_get_allocated_width(window) == initial_width);
		CHECK(gtk_widget_get_allocated_height(window) == initial_height);
	}
	gtk_tree_path_free(applications_selection);
	gtk_tree_path_free(favorites_selection);
	gtk_widget_destroy(window);
	g_object_unref(window);
	g_object_unref(model);
	g_object_unref(applications_model);
}

void check_fullscreen_sidebar_selector_keeps_canonical_prefix()
{
	MenuCompositionInput input = { LayoutMode::FullScreen, PrimaryEdge::Top,
			CompositionSidebar::Left, true, true, 4, true,
			MenuDirection::LeftToRight };
	const MenuComposition composition = meow_resolve_menu_composition(input);
	CHECK(composition.apps_places_location == MenuControlLocation::Sidebar);

	GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* selector = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	GtkWidget* category = gtk_button_new_with_label("All Applications");
	g_object_ref_sink(sidebar);
	gtk_box_pack_start(GTK_BOX(sidebar), selector, false, false, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), separator, false, false, 4);
	gtk_box_pack_start(GTK_BOX(sidebar), category, false, false, 0);

	for (int pass = 0; pass < 40; ++pass)
	{
		CHECK(meow_box_repack_child(GTK_BOX(sidebar), selector,
				false, false, false, 0));
		CHECK(meow_restore_vertical_selector_prefix(GTK_BOX(sidebar),
				selector, separator));
		GList* children = gtk_container_get_children(GTK_CONTAINER(sidebar));
		CHECK(g_list_nth_data(children, 0) == selector);
		CHECK(g_list_nth_data(children, 1) == separator);
		CHECK(g_list_nth_data(children, 2) == category);
		g_list_free(children);
	}

	gtk_widget_destroy(sidebar);
	g_object_unref(sidebar);
}

void check_sidebar_disabled_builtin_headings_identify_empty_and_populated_pages()
{
	if (!gtk_init_check(nullptr, nullptr))
		return;

	GtkWidget* stack = gtk_stack_new();
	g_object_ref_sink(stack);
	const char* names[] = { "favorites", "recent", "applications" };
	const char* labels[] = { "FAVORITES", "RECENTLY USED", "ALL APPLICATIONS" };
	const int rows[] = { 2, 0, 3 };
	GtkWidget* headings[3] = { nullptr, nullptr, nullptr };
	GtkListStore* models[3] = { nullptr, nullptr, nullptr };

	for (int page = 0; page < 3; ++page)
	{
		models[page] = gtk_list_store_new(1, G_TYPE_STRING);
		for (int row = 0; row < rows[page]; ++row)
		{
			GtkTreeIter iter;
			gtk_list_store_append(models[page], &iter);
			gtk_list_store_set(models[page], &iter, 0, "Application", -1);
		}
		GtkWidget* result = gtk_tree_view_new_with_model(
				GTK_TREE_MODEL(models[page]));
		GtkWidget* outer = meow::meowmenu_create_default_heading_page(
				result, labels[page], &headings[page]);
		gtk_stack_add_named(GTK_STACK(stack), outer, names[page]);
	}
	gtk_widget_show_all(stack);
	for (GtkWidget* heading : headings)
		gtk_widget_set_visible(heading, TRUE);

	for (int page = 0; page < 3; ++page)
	{
		gtk_stack_set_visible_child_name(GTK_STACK(stack), names[page]);
		CHECK(gtk_widget_get_visible(headings[page]));
		CHECK(std::string(gtk_label_get_text(GTK_LABEL(headings[page])))
				== labels[page]);
		CHECK(gtk_tree_model_iter_n_children(
				GTK_TREE_MODEL(models[page]), nullptr) == rows[page]);
	}

	gtk_widget_destroy(stack);
	g_object_unref(stack);
	for (GtkListStore* model : models)
		g_object_unref(model);
}

void check_radio_group_performs_one_effective_transition()
{
	if (!gtk_init_check(nullptr, nullptr))
		return;

	struct TransitionCounts
	{
		int signals = 0;
		int effective = 0;
		GtkToggleButton* active = nullptr;
	};
	TransitionCounts counts;
	GtkWidget* first = gtk_radio_button_new(nullptr);
	GSList* group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(first));
	GtkWidget* second = gtk_radio_button_new(group);
	g_object_ref_sink(first);
	g_object_ref_sink(second);
	auto callback = +[](GtkToggleButton* button, gpointer data)
	{
		TransitionCounts* state = static_cast<TransitionCounts*>(data);
		++state->signals;
		if (!category_toggle_transition_is_active(button))
			return;
		++state->effective;
		state->active = button;
	};
	g_signal_connect(first, "toggled", G_CALLBACK(callback), &counts);
	g_signal_connect(second, "toggled", G_CALLBACK(callback), &counts);

	// Programmatic selection emits inactive-old and active-new signals.
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(second), true);
	CHECK(counts.signals == 2);
	CHECK(counts.effective == 1);
	CHECK(counts.active == GTK_TOGGLE_BUTTON(second));

	// GtkButton activation is shared by pointer and keyboard activation paths.
	gtk_button_clicked(GTK_BUTTON(first));
	CHECK(counts.signals == 4);
	CHECK(counts.effective == 2);
	CHECK(counts.active == GTK_TOGGLE_BUTTON(first));
	gtk_widget_grab_focus(second);
	gtk_button_clicked(GTK_BUTTON(second));
	CHECK(counts.signals == 6);
	CHECK(counts.effective == 3);
	CHECK(counts.active == GTK_TOGGLE_BUTTON(second));

	g_object_unref(first);
	g_object_unref(second);
}

void check_fullscreen_search_column_does_not_ratchet()
{
	if (!gtk_init_check(nullptr, nullptr))
		return;

	const int workarea_width = 960;
	const FullscreenMainColumn column =
			meow_fullscreen_main_column(workarea_width);
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* middle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* mode = gtk_button_new_with_label("Applications");
	GtkWidget* search = gtk_search_entry_new();
	GtkWidget* result_band = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
	GtkWidget* result = gtk_icon_view_new();
	GtkListStore* model = gtk_list_store_new(1, G_TYPE_STRING);
	for (int row_index = 0; row_index < 64; ++row_index)
	{
		GtkTreeIter iter;
		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter, 0, "Application", -1);
	}
	gtk_icon_view_set_text_column(GTK_ICON_VIEW(result), 0);
	gtk_widget_set_size_request(middle, column.width, -1);
	gtk_box_pack_start(GTK_BOX(middle), mode, false, false, 0);
	gtk_box_pack_start(GTK_BOX(middle), search, true, true, 0);
	gtk_box_set_center_widget(GTK_BOX(row), middle);
	gtk_container_add(GTK_CONTAINER(scroller), result);
	gtk_widget_set_margin_start(result_band, column.margin);
	gtk_widget_set_margin_end(result_band, column.margin);
	gtk_box_pack_start(GTK_BOX(result_band), scroller, true, true, 0);
	gtk_box_pack_start(GTK_BOX(layout), row, false, false, 0);
	gtk_box_pack_start(GTK_BOX(layout), result_band, true, true, 0);
	gtk_container_add(GTK_CONTAINER(window), layout);
	gtk_widget_set_size_request(window, workarea_width, 600);
	gtk_widget_show_all(window);
	gtk_icon_view_set_model(GTK_ICON_VIEW(result), GTK_TREE_MODEL(model));
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);

	int initial_x = 0;
	int ignored_y = 0;
	CHECK(gtk_widget_translate_coordinates(search, window, 0, 0,
			&initial_x, &ignored_y));
	const int initial_search_width = gtk_widget_get_allocated_width(search);
	const int initial_window_width = gtk_widget_get_allocated_width(window);
	for (int transition = 1; transition <= 40; ++transition)
	{
		gtk_icon_view_set_model(GTK_ICON_VIEW(result), nullptr);
		const int overshoot = transition * 19;
		const int apparent_result_width = column.width + overshoot;
		const int toplevel_overshoot = overshoot;
		const int effective = std::min(column.width,
				apparent_result_width - toplevel_overshoot);
		CHECK(effective == column.width);
		gtk_icon_view_set_columns(GTK_ICON_VIEW(result), -1);
		gtk_icon_view_set_item_width(GTK_ICON_VIEW(result), -1);
		gtk_icon_view_set_model(GTK_ICON_VIEW(result), GTK_TREE_MODEL(model));
		meow::meowmenu_queue_complete_result_frame(window, result);
		while (g_main_context_pending(nullptr))
			g_main_context_iteration(nullptr, FALSE);
		int current_x = 0;
		CHECK(gtk_widget_translate_coordinates(search, window, 0, 0,
				&current_x, &ignored_y));
		CHECK(current_x == initial_x);
		CHECK(gtk_widget_get_allocated_width(search) == initial_search_width);
		CHECK(gtk_widget_get_allocated_width(window) == initial_window_width);
		CHECK(gtk_icon_view_get_columns(GTK_ICON_VIEW(result)) == -1);
		CHECK(gtk_icon_view_get_item_width(GTK_ICON_VIEW(result)) == -1);
	}

	gtk_widget_destroy(window);
	g_object_unref(window);
	g_object_unref(model);
}

/* check_resize_frame_plan_keeps_one_axis_specific_transaction:
 *
 * Exercises the immutable plan consumed by Window's sole X11 frame callback.
 * The accepted toplevel widths must reach the result-width update unchanged,
 * while height-only, origin-only, and no-op frames skip unrelated work.
 */
void check_resize_frame_plan_keeps_one_axis_specific_transaction()
{
	const InteractiveResize::Rectangle displayed = {100, 200, 640, 480};
	for (int sample = 1; sample <= 100; ++sample)
	{
		const InteractiveResize::Rectangle requested = {
			100, 200, 640 + sample, 480
		};
		const WindowResizeFramePlan plan = window_resize_frame_plan(
				displayed, requested);
		CHECK(!plan.empty());
		CHECK(plan.changes.width);
		CHECK(!plan.changes.height && !plan.changes.origin);
		CHECK(plan.current_toplevel_width == displayed.width);
		CHECK(plan.requested_toplevel_width == requested.width);
		CHECK(plan.updates_result_width());
		CHECK(!plan.updates_vertical_overflow());
		CHECK(!plan.moves_window());
	}

	WindowResizeFramePlan plan = window_resize_frame_plan(displayed,
			{100, 200, 640, 520});
	CHECK(!plan.updates_result_width());
	CHECK(plan.updates_vertical_overflow());
	CHECK(!plan.moves_window());

	plan = window_resize_frame_plan(displayed, {90, 180, 640, 480});
	CHECK(!plan.updates_result_width());
	CHECK(!plan.updates_vertical_overflow());
	CHECK(plan.moves_window());

	plan = window_resize_frame_plan(displayed, displayed);
	CHECK(plan.empty());
	CHECK(!plan.updates_result_width());
	CHECK(!plan.updates_vertical_overflow());
	CHECK(!plan.moves_window());
}

}

int main()
{
	check_horizontal_selector_home();
	check_horizontal_secondary_boundary_keeps_layout_geometry();
	check_contents_frame_margin_follows_adjacent_secondary_band();
	check_results_clip_tracks_real_grid_boundary();
	check_mapped_result_frame_orders_preparation_and_damage();
	check_fullscreen_sidebar_selector_keeps_canonical_prefix();
	check_sidebar_disabled_builtin_headings_identify_empty_and_populated_pages();
	check_radio_group_performs_one_effective_transition();
	check_fullscreen_search_column_does_not_ratchet();
	check_resize_frame_plan_keeps_one_axis_specific_transaction();
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
	CHECK(resolve_application_opening_target(Settings::CategoryFavorites,
			true, true) == ApplicationOpeningTarget::Favorites);
	CHECK(resolve_application_opening_target(Settings::CategoryRecent,
			true, true) == ApplicationOpeningTarget::Recent);
	CHECK(resolve_application_opening_target(Settings::CategoryRecent,
			true, false) == ApplicationOpeningTarget::All);
	CHECK(resolve_application_opening_target(Settings::CategoryFavorites,
			false, true) == ApplicationOpeningTarget::All);
	CHECK(resolve_application_opening_target(Settings::CategoryAll,
			false, false) == ApplicationOpeningTarget::All);
	const InteractiveResize::Rectangle displayed = {100, 100, 640, 480};
	WindowGeometryChanges changes = window_geometry_changes(displayed,
			{100, 100, 700, 480});
	CHECK(changes.width && !changes.height && !changes.origin);
	changes = window_geometry_changes(displayed, {100, 100, 640, 520});
	CHECK(!changes.width && changes.height && !changes.origin);
	changes = window_geometry_changes(displayed, {90, 100, 640, 480});
	CHECK(!changes.width && !changes.height && changes.origin);
	CHECK(window_geometry_changes(displayed, displayed).empty());
	CHECK(window_resize_frame_should_schedule(0, true));
	CHECK(!window_resize_frame_should_schedule(7, true));
	CHECK(!window_resize_frame_should_schedule(0, false));

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
	MenuModeInputs mapped_apps;
	mapped_apps.requested_mode = MenuMode::Applications;
	mapped_apps.transition = MenuModeTransition::Reevaluate;
	mapped_apps.places_enabled = true;
	mapped_apps.recent_applications_enabled = true;
	for (int cycle = 0; cycle < 20; ++cycle)
	{
		// Simulate stale pre-map visibility before the authoritative mode pass.
		for (GtkWidget* widget : widgets.all())
			gtk_widget_hide(widget);
		widgets.apply(resolve_menu_mode(mapped_apps));
		CHECK(gtk_widget_get_visible(widgets.app_favourites));
		CHECK(gtk_widget_get_visible(widgets.app_recent));
		CHECK(gtk_widget_get_visible(widgets.app_all));
		CHECK(gtk_widget_get_visible(widgets.app_category));
		CHECK(!gtk_widget_get_visible(widgets.places_home));
		CHECK(!gtk_widget_get_visible(widgets.places_history));
		CHECK(!gtk_widget_get_visible(widgets.places_favourites));
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
	CHECK(composition.session_alignment == MenuAlignment::Fill);
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
	CHECK(composition.session_alignment == MenuAlignment::Fill);

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
