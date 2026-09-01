/* GTK-backed coverage for displayed tree-row directional movement. */

#include "ui/launcher-tree-view.h"

#include <gtk/gtk.h>

#include <cstdio>

using WhiskerMenu::Keyboard::NavigationRect;
using WhiskerMenu::Keyboard::PhysicalDirection;
using WhiskerMenu::launcher_tree_view_find_directional_path;
using WhiskerMenu::launcher_tree_view_get_path_rectangle;

namespace
{

GtkTreePath* path_at(int index)
{
	return gtk_tree_path_new_from_indices(index, -1);
}

} // namespace

int main()
{
	if (!gtk_init_check(nullptr, nullptr))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* widget = gtk_tree_view_new();
	GtkTreeView* view = GTK_TREE_VIEW(widget);
	GtkTreeStore* model = gtk_tree_store_new(
			WhiskerMenu::LauncherView::N_COLUMNS,
			G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	GtkTreeViewColumn* column = gtk_tree_view_column_new();
	GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, true);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
			WhiskerMenu::LauncherView::COLUMN_TEXT);
	gtk_tree_view_append_column(view, column);
	gtk_tree_view_set_model(view, GTK_TREE_MODEL(model));
	gtk_container_add(GTK_CONTAINER(window), widget);

	for (const char* text : {"One", "Two", "Three"})
	{
		GtkTreeIter iter;
		gtk_tree_store_append(model, &iter, nullptr);
		gtk_tree_store_set(model, &iter,
				WhiskerMenu::LauncherView::COLUMN_TEXT, text, -1);
	}
	gtk_widget_show_all(window);
	while (gtk_events_pending())
		gtk_main_iteration();

	int failures = 0;
	GtkTreePath* middle = path_at(1);
	GtkTreePath* down = launcher_tree_view_find_directional_path(view,
			middle, PhysicalDirection::Down);
	GtkTreePath* expected_last = path_at(2);
	if (!down || gtk_tree_path_compare(down, expected_last) != 0)
		++failures;
	gtk_tree_path_free(down);
	gtk_tree_path_free(expected_last);

	GtkTreePath* first = path_at(0);
	GtkTreePath* up = launcher_tree_view_find_directional_path(view,
			first, PhysicalDirection::Up);
	if (up)
		++failures;
	gtk_tree_path_free(up);

	GtkTreePath* horizontal = launcher_tree_view_find_directional_path(view,
			middle, PhysicalDirection::Right);
	if (horizontal)
		++failures;
	gtk_tree_path_free(horizontal);

	NavigationRect rectangle;
	if (!launcher_tree_view_get_path_rectangle(view, middle, &rectangle)
			|| !rectangle.is_valid())
		++failures;

	gtk_tree_path_free(first);
	gtk_tree_path_free(middle);
	gtk_widget_destroy(window);
	g_object_unref(window);
	g_object_unref(model);
	if (failures != 0)
	{
		std::fprintf(stderr, "test_launcher_tree_view: %d failure(s)\n",
				failures);
		return 1;
	}
	std::printf("test_launcher_tree_view: ok\n");
	return 0;
}
