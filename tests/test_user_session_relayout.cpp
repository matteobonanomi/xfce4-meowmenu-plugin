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
#include <cmath>
#include <cstdio>
#include <vector>

using namespace WhiskerMenu;

namespace
{

void drain_events()
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
}

int translated_x(GtkWidget* widget, GtkWidget* ancestor)
{
	int x = 0;
	int y = 0;
	assert(gtk_widget_translate_coordinates(widget, ancestor, 0, 0, &x, &y));
	return x;
}

void check_session_allocated_edge(bool right_sidebar, bool left_to_right)
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* commands = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* spacer = gtk_label_new(nullptr);
	GtkWidget* selector = gtk_button_new_with_label("Apps / Places");
	std::vector<GtkWidget*> buttons;
	gtk_box_pack_start(GTK_BOX(commands), spacer, true, true, 0);
	for (int i = 0; i < 9; ++i)
	{
		GtkWidget* button = gtk_button_new();
		gtk_widget_set_size_request(button, 24, 24);
		gtk_box_pack_start(GTK_BOX(commands), button, false, false, 0);
		buttons.push_back(button);
	}
	gtk_widget_set_halign(commands, GTK_ALIGN_FILL);
	const GtkTextDirection direction = left_to_right
			? GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;
	gtk_widget_set_direction(row, direction);
	gtk_widget_set_direction(commands, direction);
	const bool selector_first = left_to_right
			? !right_sidebar : right_sidebar;
	if (!selector_first)
	{
		gtk_box_pack_start(GTK_BOX(row), commands, true, true, 0);
		gtk_box_pack_start(GTK_BOX(row), selector, false, false, 0);
	}
	else
	{
		gtk_box_pack_start(GTK_BOX(row), selector, false, false, 0);
		gtk_box_pack_start(GTK_BOX(row), commands, true, true, 0);
	}
	gtk_box_reorder_child(GTK_BOX(commands), spacer,
			meow_session_spacer_position(right_sidebar, left_to_right));
	gtk_container_add(GTK_CONTAINER(window), row);
	gtk_window_set_default_size(GTK_WINDOW(window), 560, 70);
	gtk_widget_show_all(window);
	drain_events();

	const int row_width = gtk_widget_get_allocated_width(row);
	int session_left = row_width;
	int session_right = 0;
	for (GtkWidget* button : buttons)
	{
		const int x = translated_x(button, row);
		session_left = MIN(session_left, x);
		session_right = MAX(session_right,
				x + gtk_widget_get_allocated_width(button));
	}
	const int selector_left = translated_x(selector, row);
	const int selector_right = selector_left
			+ gtk_widget_get_allocated_width(selector);
	if (right_sidebar)
	{
		assert(session_left <= 1);
		assert(std::abs(selector_right - row_width) <= 1);
	}
	else
	{
		assert(selector_left <= 1);
		assert(std::abs(session_right - row_width) <= 1);
	}

	gtk_widget_destroy(window);
}

void check_profile_allocated_geometry()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget* column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* profile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* profile_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* profile_leading = gtk_label_new(nullptr);
	GtkWidget* profile_trailing = gtk_label_new(nullptr);
	GtkWidget* avatar = gtk_drawing_area_new();
	GtkWidget* username = gtk_label_new("A deliberately wide profile");
	gtk_widget_set_size_request(avatar, 32, 32);
	gtk_box_pack_start(GTK_BOX(profile_content), avatar, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile_content), username, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile), profile_leading, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile), profile_content, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile), profile_trailing, true, true, 0);

	GtkWidget* category = gtk_radio_button_new(nullptr);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(category), false);
	gtk_button_set_relief(GTK_BUTTON(category), GTK_RELIEF_NONE);
	GtkWidget* category_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	GtkWidget* category_icon = gtk_drawing_area_new();
	GtkWidget* category_label = gtk_label_new("All Applications");
	gtk_widget_set_size_request(category_icon, 24, 24);
	gtk_box_pack_start(GTK_BOX(category_content), category_icon,
			false, false, 0);
	gtk_box_pack_start(GTK_BOX(category_content), category_label,
			false, false, 0);
	gtk_container_add(GTK_CONTAINER(category), category_content);
	gtk_style_context_add_class(gtk_widget_get_style_context(category),
			"category-button");

	gtk_widget_set_size_request(profile, 300, -1);
	gtk_widget_set_size_request(category, 300, -1);
	gtk_box_pack_start(GTK_BOX(column), profile, false, false, 0);
	gtk_box_pack_start(GTK_BOX(column), category, false, false, 0);
	gtk_container_add(GTK_CONTAINER(window), column);
	gtk_widget_show_all(window);
	gtk_window_set_default_size(GTK_WINDOW(window), 300, 100);
	drain_events();
	gtk_widget_hide(window);
	drain_events();
	const int inset = meow_configure_profile_sidebar_alignment(profile,
			profile_content, profile_leading, profile_trailing,
			category, true, true);
	assert(inset >= 0);
	int leading_request = -1;
	int trailing_request = -1;
	gtk_widget_get_size_request(profile_leading, &leading_request, nullptr);
	gtk_widget_get_size_request(profile_trailing, &trailing_request, nullptr);
	assert(leading_request == (inset > 0 ? inset : -1));
	assert(trailing_request <= 0);
	gtk_widget_show(window);
	drain_events();
	assert(std::abs(translated_x(avatar, column)
			- translated_x(category_icon, column)) <= 1);

	gtk_widget_hide(window);
	drain_events();
	gtk_widget_hide(category_label);
	gtk_box_set_child_packing(GTK_BOX(category_content), category_icon,
			true, true, 0, GTK_PACK_START);
	assert(meow_configure_profile_sidebar_alignment(profile, profile_content,
			profile_leading, profile_trailing, category, true, false) == inset);
	gtk_widget_get_size_request(profile_leading, &leading_request, nullptr);
	gtk_widget_get_size_request(profile_trailing, &trailing_request, nullptr);
	assert(leading_request == (inset > 0 ? inset : -1));
	assert(trailing_request == (inset > 0 ? inset : -1));
	gtk_widget_show(window);
	drain_events();
	const int profile_x = translated_x(avatar, column);
	const int profile_right = translated_x(username, column)
			+ gtk_widget_get_allocated_width(username);
	const int profile_width = profile_right - profile_x;
	const int outer_width = gtk_widget_get_allocated_width(profile);
	assert(std::abs(profile_x - (outer_width - profile_x - profile_width)) <= 1);
	const int profile_center = 2 * profile_x + profile_width;
	const int icon_x = translated_x(category_icon, column);
	const int icon_center = 2 * icon_x
			+ gtk_widget_get_allocated_width(category_icon);
	assert(std::abs(profile_center - icon_center) <= 1);
	gtk_widget_set_size_request(profile, -1, -1);
	int content_natural = 0;
	int profile_natural = 0;
	gtk_widget_get_preferred_width(profile_content, nullptr, &content_natural);
	gtk_widget_get_preferred_width(profile, nullptr, &profile_natural);
	assert(profile_natural >= content_natural + 2 * inset);

	assert(meow_configure_profile_sidebar_alignment(profile, profile_content,
			profile_leading, profile_trailing, category, false, false) == 0);
	gtk_widget_get_size_request(profile_leading, &leading_request, nullptr);
	gtk_widget_get_size_request(profile_trailing, &trailing_request, nullptr);
	assert(leading_request <= 0);
	assert(trailing_request <= 0);
	gtk_widget_destroy(window);
}

void check_sidebar_scrollbar_reservation()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* sidebar = gtk_scrolled_window_new(nullptr, nullptr);
	GtkWidget* categories = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* profile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* profile_content = gtk_label_new("Profile");
	GtkWidget* results = gtk_label_new("Results");
	gtk_box_pack_start(GTK_BOX(profile), profile_content, false, false, 0);
	GtkWidget* first_button = gtk_radio_button_new(nullptr);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(first_button), false);
	GtkWidget* first_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* first_icon = gtk_image_new_from_icon_name(
			"applications-other", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(first_content), first_icon, true, true, 0);
	gtk_container_add(GTK_CONTAINER(first_button), first_content);
	gtk_box_pack_start(GTK_BOX(categories), first_button, false, false, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(sidebar), TRUE);
	gtk_container_add(GTK_CONTAINER(sidebar), categories);
	assert(meow_configure_vertical_sidebar_content(categories, true));
	gtk_box_pack_start(GTK_BOX(row), sidebar, false, false, 0);
	gtk_box_pack_start(GTK_BOX(row), results, true, true, 0);
	gtk_container_add(GTK_CONTAINER(window), row);
	gtk_widget_show_all(row);

	const int reserved_width = meow_configure_vertical_sidebar_width(
			sidebar, profile, true, true);
	GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(sidebar));
	GtkWidget* scrollbar = gtk_scrolled_window_get_vscrollbar(
			GTK_SCROLLED_WINDOW(sidebar));
	int viewport_natural = 0;
	int scrollbar_natural = 0;
	gtk_widget_get_preferred_width(viewport, nullptr, &viewport_natural);
	gtk_widget_get_preferred_width(scrollbar, nullptr, &scrollbar_natural);
	assert(reserved_width >= viewport_natural + scrollbar_natural);

	gtk_window_set_default_size(GTK_WINDOW(window), reserved_width + 220, 120);
	gtk_widget_show(window);
	drain_events();
	const int initial_sidebar_width = gtk_widget_get_allocated_width(sidebar);
	const int sidebar_center = 2 * translated_x(sidebar, row)
			+ initial_sidebar_width;
	const int icon_center = 2 * translated_x(first_icon, row)
			+ gtk_widget_get_allocated_width(first_icon);
	assert(std::abs(sidebar_center - icon_center) <= 1);
	for (int i = 0; i < 20; ++i)
	{
		GtkWidget* button = gtk_radio_button_new(nullptr);
		gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), false);
		gtk_widget_set_size_request(button, -1, 32);
		GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(content), gtk_image_new_from_icon_name(
				"applications-other", GTK_ICON_SIZE_BUTTON), true, true, 0);
		gtk_container_add(GTK_CONTAINER(button), content);
		gtk_box_pack_start(GTK_BOX(categories), button, false, false, 0);
		gtk_widget_show_all(button);
	}
	// Wait for the resize and draw cycle that updates automatic scrollbar
	// visibility and its adjustment after the category list grows.
	gtk_test_widget_wait_for_draw(window);
	drain_events();
	assert(gtk_widget_get_child_visible(scrollbar));
	assert(gtk_widget_get_allocated_width(sidebar) == initial_sidebar_width);
	assert(meow_configure_vertical_sidebar_width(sidebar, profile, true, true)
			== reserved_width);

	GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(
			GTK_SCROLLED_WINDOW(sidebar));
	gtk_adjustment_set_value(adjustment,
			gtk_adjustment_get_upper(adjustment)
			- gtk_adjustment_get_page_size(adjustment));
	assert(gtk_adjustment_get_value(adjustment)
			> gtk_adjustment_get_lower(adjustment));
	assert(meow_reset_vertical_sidebar_scroll(GTK_SCROLLED_WINDOW(sidebar)));
	assert(gtk_adjustment_get_value(adjustment)
			== gtk_adjustment_get_lower(adjustment));
	assert(!meow_reset_vertical_sidebar_scroll(nullptr));
	assert(meow_configure_vertical_sidebar_content(categories, false));
	assert(!gtk_widget_get_hexpand(first_button));
	assert(meow_configure_vertical_sidebar_width(sidebar, profile,
			false, false) == -1);

	gtk_widget_destroy(window);
	gtk_widget_destroy(profile);
}

void check_presented_navigation_reveal()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget* host = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* vertical = gtk_scrolled_window_new(nullptr, nullptr);
	GtkWidget* horizontal = gtk_scrolled_window_new(nullptr, nullptr);
	GtkWidget* categories = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(vertical),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(horizontal),
			GTK_POLICY_ALWAYS, GTK_POLICY_NEVER);
	gtk_widget_set_size_request(vertical, 120, 80);
	gtk_widget_set_size_request(horizontal, 120, 80);
	gtk_box_pack_start(GTK_BOX(host), vertical, true, true, 0);
	gtk_box_pack_start(GTK_BOX(host), horizontal, true, true, 0);
	gtk_container_add(GTK_CONTAINER(vertical), categories);
	GtkWidget* first = nullptr;
	GtkWidget* last = nullptr;
	for (int i = 0; i < 20; ++i)
	{
		last = gtk_button_new_with_label("Oversized category");
		if (!first)
			first = last;
		gtk_widget_set_size_request(last, 150, 28);
		gtk_box_pack_start(GTK_BOX(categories), last, false, false, 0);
	}
	gtk_container_add(GTK_CONTAINER(window), host);
	gtk_widget_show_all(window);
	drain_events();

	assert(meow_bind_navigation_scroller(GTK_CONTAINER(categories),
			GTK_SCROLLED_WINDOW(vertical), GTK_ORIENTATION_VERTICAL));
	GtkAdjustment* vertical_adjustment =
			gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(vertical));
	assert(meow_reveal_navigation_widget(GTK_SCROLLED_WINDOW(vertical), last));
	assert(gtk_adjustment_get_value(vertical_adjustment) > 0.0);

	const double inactive_value = gtk_adjustment_get_value(vertical_adjustment);
	g_object_ref(categories);
	gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(categories)),
			categories);
	gtk_orientable_set_orientation(GTK_ORIENTABLE(categories),
			GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add(GTK_CONTAINER(horizontal), categories);
	g_object_unref(categories);
	gtk_widget_hide(vertical);
	gtk_widget_show_all(horizontal);
	gtk_widget_queue_resize(categories);
	drain_events();
	assert(meow_bind_navigation_scroller(GTK_CONTAINER(categories),
			GTK_SCROLLED_WINDOW(horizontal), GTK_ORIENTATION_HORIZONTAL));
	GtkAdjustment* horizontal_adjustment =
			gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(horizontal));
	gtk_adjustment_configure(horizontal_adjustment, 300.0,
			0.0, 600.0, 1.0, 20.0, 120.0);
	assert(meow_reveal_navigation_widget(GTK_SCROLLED_WINDOW(horizontal), first));
	assert(gtk_adjustment_get_value(horizontal_adjustment) < 300.0);
	assert(gtk_adjustment_get_value(vertical_adjustment) == inactive_value);
	assert(!meow_reveal_navigation_widget(GTK_SCROLLED_WINDOW(vertical), last));

	gtk_widget_destroy(window);
}

} // namespace

int main(int argc, char** argv)
{
	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}
	check_session_allocated_edge(false, true);
	check_session_allocated_edge(true, true);
	check_session_allocated_edge(false, false);
	check_session_allocated_edge(true, false);
	check_profile_allocated_geometry();
	check_sidebar_scrollbar_reservation();
	check_presented_navigation_reveal();

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
	assert(meow_session_spacer_position(false, true) == 0);
	assert(meow_session_spacer_position(false, false) == 9);
	assert(meow_session_spacer_position(true, true) == 9);
	assert(meow_session_spacer_position(true, false) == 0);
	GtkWidget* profile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* profile_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* profile_leading = gtk_label_new(nullptr);
	GtkWidget* profile_trailing = gtk_label_new(nullptr);
	GtkWidget* avatar = gtk_image_new();
	GtkWidget* username = gtk_label_new("User");
	GtkWidget* category_button = gtk_radio_button_new(nullptr);
	gtk_style_context_add_class(
			gtk_widget_get_style_context(category_button), "category-button");
	gtk_widget_set_direction(profile, GTK_TEXT_DIR_LTR);
	gtk_widget_set_direction(category_button, GTK_TEXT_DIR_LTR);
	const int ltr_inset = meow_configure_profile_sidebar_alignment(profile,
			profile_content, profile_leading, profile_trailing,
			category_button, true, true);
	int spacer_request = -1;
	gtk_widget_get_size_request(profile_leading, &spacer_request, nullptr);
	assert(spacer_request == (ltr_inset > 0 ? ltr_inset : -1));
	gtk_widget_set_direction(profile, GTK_TEXT_DIR_RTL);
	gtk_widget_set_direction(profile_content, GTK_TEXT_DIR_RTL);
	gtk_widget_set_direction(category_button, GTK_TEXT_DIR_RTL);
	const int rtl_inset = meow_configure_profile_sidebar_alignment(profile,
			profile_content, profile_leading, profile_trailing,
			category_button, true, true);
	gtk_widget_get_size_request(profile_leading, &spacer_request, nullptr);
	assert(spacer_request == (rtl_inset > 0 ? rtl_inset : -1));
	gtk_box_pack_start(GTK_BOX(profile_content), avatar, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile_content), username, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile), profile_leading, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile), profile_content, false, false, 0);
	gtk_box_pack_start(GTK_BOX(profile), profile_trailing, true, true, 0);
	assert(meow_box_repack_child(GTK_BOX(primary), profile,
			false, false, false, 0));
	assert(gtk_widget_get_parent(profile_content) == profile);
	assert(gtk_widget_get_parent(avatar) == profile_content);
	assert(gtk_widget_get_parent(username) == profile_content);
	assert(meow_box_repack_child(GTK_BOX(secondary), profile,
			false, false, false, 0));
	assert(gtk_widget_get_parent(profile) == secondary);
	assert(gtk_widget_get_parent(profile_content) == profile);
	assert(gtk_widget_get_parent(avatar) == profile_content);
	assert(gtk_widget_get_parent(username) == profile_content);
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
	GtkWidget* selector_separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	GtkWidget* wide_category = gtk_label_new("All Applications");
	gtk_widget_set_size_request(wide_category, 180, -1);
	gtk_box_pack_start(GTK_BOX(sidebar), selector, false, false, 0);
	gtk_box_pack_start(GTK_BOX(sidebar), selector_separator, false, false, 4);
	gtk_box_pack_start(GTK_BOX(sidebar), wide_category, false, false, 0);
	for (int pass = 0; pass < 40; ++pass)
	{
		assert(meow_box_repack_child(GTK_BOX(sidebar), selector,
				false, false, false, 0));
		assert(meow_restore_vertical_selector_prefix(GTK_BOX(sidebar),
				selector, selector_separator));
		GList* children = gtk_container_get_children(GTK_CONTAINER(sidebar));
		assert(g_list_nth_data(children, 0) == selector);
		assert(g_list_nth_data(children, 1) == selector_separator);
		assert(g_list_nth_data(children, 2) == wide_category);
		g_list_free(children);
	}
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
	gtk_widget_destroy(category_button);
	gtk_widget_destroy(outsider);
	g_object_unref(widths);

	return 0;
}
