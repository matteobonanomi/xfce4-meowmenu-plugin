/*
 * GTK coverage for dynamic category widget detach semantics.
 */

#include "core/category-lifetime.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace WhiskerMenu;

int main(int argc, char** argv)
{
	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	g_object_ref_sink(box);
	GtkSizeGroup* width_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	std::vector<GtkWidget*> widgets;
	std::vector<GtkWidget*> owned_widgets;
	for (int i = 0; i < 3; ++i)
	{
		GtkWidget* button = gtk_button_new();
		g_object_ref_sink(button);
		owned_widgets.push_back(button);
		gtk_box_pack_start(GTK_BOX(box), button, false, false, 0);
		gtk_size_group_add_widget(width_group, button);
		widgets.push_back(button);
	}

	detach_category_widgets(width_group, widgets);
	assert(widgets.empty());

	GList* children = gtk_container_get_children(GTK_CONTAINER(box));
	assert(children == nullptr);
	g_list_free(children);

	detach_category_widgets(width_group, widgets);
	assert(widgets.empty());

	for (GtkWidget* button : owned_widgets)
	{
		assert(gtk_widget_get_parent(button) == nullptr);
		gtk_widget_destroy(button);
		g_object_unref(button);
	}

	gtk_widget_destroy(box);
	g_object_unref(box);
	g_object_unref(width_group);

	return 0;
}
