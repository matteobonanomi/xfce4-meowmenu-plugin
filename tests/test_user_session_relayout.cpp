/*
 * Regression coverage for the guarded GTK row-relayout helpers.
 *
 * The helpers are small, but they protect a crash-sensitive path: moving the
 * session command box between rows and reordering children while Properties is
 * open. This test creates real GtkBox parents when a display is available and
 * verifies missing children are treated as no-ops instead of GTK assertions.
 */

#include "core/user-session-relayout.h"

#include <cassert>
#include <cstdio>

using namespace WhiskerMenu;

int main(int argc, char** argv)
{
	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	GtkWidget* leading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* trailing = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* child = gtk_button_new();
	GtkWidget* outsider = gtk_button_new();
	GtkWidget* grid = gtk_grid_new();
	GtkSizeGroup* widths = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkWidget* primary = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* secondary = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* secondary_spacer = gtk_label_new(nullptr);
	GtkWidget* secondary_session = gtk_button_new();
	gtk_box_pack_start(GTK_BOX(secondary), secondary_spacer, true, true, 0);
	gtk_box_pack_start(GTK_BOX(secondary), secondary_session, false, false, 0);
	gtk_widget_set_halign(secondary_session, GTK_ALIGN_END);
	assert(gtk_widget_get_halign(secondary_session) == GTK_ALIGN_END);
	gtk_box_reorder_child(GTK_BOX(secondary), secondary_spacer, 0);
	gtk_widget_set_direction(secondary, GTK_TEXT_DIR_LTR);
	gtk_box_reorder_child(GTK_BOX(secondary), secondary_spacer, 0);
	gtk_widget_set_direction(secondary, GTK_TEXT_DIR_RTL);
	gtk_box_reorder_child(GTK_BOX(secondary), secondary_spacer, 1);
	GtkWidget* profile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* avatar = gtk_image_new();
	GtkWidget* username = gtk_label_new("User");
	gtk_box_pack_start(GTK_BOX(profile), avatar, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile), username, true, true, 0);
	assert(meow_box_repack_child(GTK_BOX(primary), profile,
			false, false, false, 0));
	assert(gtk_widget_get_parent(avatar) == profile);
	assert(gtk_widget_get_parent(username) == profile);
	assert(meow_box_repack_child(GTK_BOX(secondary), profile,
			false, false, false, 0));
	assert(gtk_widget_get_parent(profile) == secondary);
	assert(gtk_widget_get_parent(avatar) == profile);
	assert(gtk_widget_get_parent(username) == profile);
	assert(meow_box_repack_child(GTK_BOX(primary), profile,
			false, false, false, 0));

	assert(meow_box_repack_child(GTK_BOX(leading), child,
			false, false, false, 0));
	assert(meow_box_contains_child(GTK_BOX(leading), child));
	assert(!meow_box_contains_child(GTK_BOX(trailing), child));

	assert(meow_box_repack_child(GTK_BOX(trailing), child,
			true, true, true, 0));
	assert(!meow_box_contains_child(GTK_BOX(leading), child));
	assert(meow_box_contains_child(GTK_BOX(trailing), child));

	assert(meow_box_reorder_child_if_present(GTK_BOX(trailing), child, 0));
	assert(!meow_box_reorder_child_if_present(GTK_BOX(trailing), outsider, 0));
	assert(!meow_box_reorder_child_if_present(nullptr, child, 0));
	assert(meow_container_contains_child(GTK_CONTAINER(trailing), child));
	assert(!meow_container_contains_child(GTK_CONTAINER(grid), child));

	assert(meow_grid_attach_child(GTK_GRID(grid), child, 1, 2, 2, 1));
	assert(!meow_box_contains_child(GTK_BOX(trailing), child));
	assert(meow_container_contains_child(GTK_CONTAINER(grid), child));
	assert(!meow_grid_attach_child(nullptr, child, 0, 0, 1, 1));
	assert(!meow_grid_attach_child(GTK_GRID(grid), child, 0, 0, 0, 1));

	assert(meow_size_group_set_widget(widths, child, true));
	assert(meow_size_group_set_widget(widths, child, true));
	assert(g_slist_find(gtk_size_group_get_widgets(widths), child));
	assert(meow_size_group_set_widget(widths, child, false));
	assert(meow_size_group_set_widget(widths, child, false));
	assert(!g_slist_find(gtk_size_group_get_widgets(widths), child));

	GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* selector = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* wide_category = gtk_label_new("All Applications");
	gtk_widget_set_size_request(wide_category, 180, -1);
	gtk_box_pack_start(GTK_BOX(sidebar), wide_category, false, false, 0);
	gtk_widget_show_all(sidebar);
	const int sidebar_width = meow_configure_vertical_sidebar_width(
			sidebar, profile, true, true);
	int sidebar_request = -1;
	int profile_request = -1;
	gtk_widget_get_size_request(sidebar, &sidebar_request, nullptr);
	gtk_widget_get_size_request(profile, &profile_request, nullptr);
	assert(sidebar_width >= 180);
	assert(sidebar_request == sidebar_width);
	assert(profile_request == sidebar_width);
	assert(!gtk_widget_get_hexpand(sidebar));
	assert(!gtk_widget_get_hexpand(profile));

	// Re-running the policy after a parent becomes much wider must retain the
	// same content-derived column width; only changed contents may widen it.
	GtkAllocation oversized = {0, 0, sidebar_width + 400, 200};
	gtk_widget_size_allocate(sidebar, &oversized);
	assert(meow_configure_vertical_sidebar_width(sidebar, profile, true, true)
			== sidebar_width);
	assert(meow_configure_vertical_sidebar_width(sidebar, profile, true, false)
			== sidebar_width);
	gtk_widget_get_size_request(profile, &profile_request, nullptr);
	assert(profile_request == -1);
	assert(meow_configure_vertical_sidebar_width(sidebar, profile, false, false)
			== -1);
	gtk_widget_get_size_request(sidebar, &sidebar_request, nullptr);
	assert(sidebar_request == -1);

	GtkWidget* allocation_grid = gtk_grid_new();
	GtkWidget* results = gtk_label_new("Results");
	gtk_widget_set_hexpand(results, TRUE);
	gtk_grid_attach(GTK_GRID(allocation_grid), sidebar, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(allocation_grid), results, 1, 0, 1, 1);
	gtk_widget_show_all(allocation_grid);
	assert(meow_configure_vertical_sidebar_width(
			sidebar, profile, true, true) == sidebar_width);
	GtkAllocation compact = {0, 0, sidebar_width + 240, 200};
	gtk_widget_size_allocate(allocation_grid, &compact);
	const int compact_sidebar = gtk_widget_get_allocated_width(sidebar);
	const int compact_results = gtk_widget_get_allocated_width(results);
	GtkAllocation wide = {0, 0, sidebar_width + 640, 200};
	gtk_widget_size_allocate(allocation_grid, &wide);
	assert(gtk_widget_get_allocated_width(sidebar) == compact_sidebar);
	assert(gtk_widget_get_allocated_width(results) == compact_results + 400);

	gtk_widget_show(child);
	meow_widget_set_visible_if_valid(child, false);
	assert(!gtk_widget_get_visible(child));
	meow_widget_set_visible_if_valid(nullptr, false);
	for (int i = 0; i < 20; ++i)
	{
		const bool visible = (i % 2) == 0;
		meow_widget_set_visible_if_valid(child, visible);
		assert(gtk_widget_get_visible(child) == visible);
	}

	gtk_widget_set_can_focus(child, true);
	meow_widget_set_can_focus_if_valid(child, false);
	assert(!gtk_widget_get_can_focus(child));
	for (int i = 0; i < 20; ++i)
	{
		const bool focusable = (i % 2) == 0;
		meow_widget_set_can_focus_if_valid(child, focusable);
		assert(gtk_widget_get_can_focus(child) == focusable);
	}

	meow_widget_set_hexpand_if_valid(child, true);
	assert(gtk_widget_get_hexpand(child));
	meow_widget_set_vexpand_if_valid(child, true);
	assert(gtk_widget_get_vexpand(child));
	meow_widget_set_halign_if_valid(child, GTK_ALIGN_END);
	assert(gtk_widget_get_halign(child) == GTK_ALIGN_END);
	meow_widget_set_valign_if_valid(child, GTK_ALIGN_CENTER);
	assert(gtk_widget_get_valign(child) == GTK_ALIGN_CENTER);
	meow_widget_set_vexpand_if_valid(child, false);
	gtk_widget_set_margin_top(child, 6);
	gtk_widget_set_margin_bottom(child, 6);
	assert(!gtk_widget_get_vexpand(child));
	assert(gtk_widget_get_margin_top(child) == 6);
	assert(gtk_widget_get_margin_bottom(child) == 6);

	for (int i = 0; i < 20; ++i)
	{
		assert(meow_box_repack_child(GTK_BOX(leading), child,
				false, i % 2, true, 0));
		assert(meow_box_contains_child(GTK_BOX(leading), child));
		assert(meow_size_group_set_widget(widths, child, true));
		assert(meow_grid_attach_child(GTK_GRID(grid), child,
				i % 2, 0, 1, 1));
		assert(meow_container_contains_child(GTK_CONTAINER(grid), child));
		assert(meow_size_group_set_widget(widths, child, false));
	}

	gtk_widget_destroy(leading);
	gtk_widget_destroy(trailing);
	gtk_widget_destroy(grid);
	gtk_widget_destroy(primary);
	gtk_widget_destroy(secondary);
	gtk_widget_destroy(allocation_grid);
	gtk_widget_destroy(selector);
	gtk_widget_destroy(outsider);
	g_object_unref(widths);

	return 0;
}
