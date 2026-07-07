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
	meow_widget_set_halign_if_valid(child, GTK_ALIGN_END);
	assert(gtk_widget_get_halign(child) == GTK_ALIGN_END);

	gtk_widget_destroy(leading);
	gtk_widget_destroy(trailing);
	gtk_widget_destroy(outsider);

	return 0;
}
