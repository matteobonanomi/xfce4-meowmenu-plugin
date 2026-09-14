/*
 * Shared behavior checks for the two runtime result-page implementations.
 */

#include <cassert>
#include <cstdio>

#include "core/plugin.h"
#include "core/window.h"
#include "launcher/favorites-page.h"
#include "places/places-page.h"
#include "private-xfconf-fixture.h"
#include "settings.h"
#include "ui/launcher-view.h"

using namespace WhiskerMenu;

namespace
{

template<typename Surface>
void exercise_result_surface(Surface* surface)
{
	GtkListStore* store = gtk_list_store_new(LauncherView::N_COLUMNS,
			G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	surface->get_view()->set_model(GTK_TREE_MODEL(store));
	assert(!surface->focus_first_result());

	GtkAllocation allocation = { 0, 0, 320, 240 };
	gtk_widget_size_allocate(surface->get_widget(), &allocation);
	surface->present();
	assert(surface->get_viewport_width() > 0);
	assert(surface->get_minimum_viewport_width() >= 0);

	GtkTreeIter iter;
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			LauncherView::COLUMN_TEXT, "Result",
			LauncherView::COLUMN_TOOLTIP, "Result tooltip",
			LauncherView::COLUMN_LAUNCHER, nullptr,
			-1);
	surface->select_first();
	GtkTreePath* selected = surface->get_view()->get_selected_path();
	assert(selected);
	gtk_tree_path_free(selected);
	assert(surface->focus_first_result());
	selected = surface->get_view()->get_selected_path();
	assert(selected);
	assert(gtk_tree_path_get_indices(selected)[0] == 0);
	gtk_tree_path_free(selected);

	surface->get_view()->unset_model();
	g_object_unref(store);
}

/* exercise_view_replacement:
 * @settings: live configuration used by both production pages.
 * @favorites: Applications result page whose view is replaced by update_view.
 * @places: Places result page whose view is replaced by reload_view.
 *
 * Starts presentation from hidden icon views and immediately replaces each
 * concrete view. The old widgets must be released without a later frame using
 * their page-owned preparation context.
 */
void exercise_view_replacement(Settings* settings, FavoritesPage* favorites,
		PlacesPage* places)
{
	settings->view_mode = Settings::ViewAsIcons;
	favorites->update_view();
	places->reload_view();

	GtkWidget* old_favorites = favorites->get_view()->get_widget();
	g_object_add_weak_pointer(G_OBJECT(old_favorites),
			reinterpret_cast<gpointer*>(&old_favorites));
	gtk_widget_hide(old_favorites);
	favorites->present();
	settings->view_mode = Settings::ViewAsList;
	favorites->update_view();
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	assert(old_favorites == nullptr);

	GtkWidget* old_places = places->get_view()->get_widget();
	g_object_add_weak_pointer(G_OBJECT(old_places),
			reinterpret_cast<gpointer*>(&old_places));
	gtk_widget_hide(old_places);
	places->present();
	places->reload_view();
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
	assert(old_places == nullptr);
}

}

static int run_test(int argc, char** argv)
{
	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	XfcePanelPlugin* host = XFCE_PANEL_PLUGIN(g_object_new(
			XFCE_TYPE_PANEL_PLUGIN,
			"name", "meowmenu",
			"display-name", "MeowMenu",
			"comment", "Result surface test host",
			"unique-id", 9002,
			nullptr));
	assert(host);
	Plugin* plugin = new Plugin(host);
	Window* window = plugin->get_window();
	exercise_result_surface(window->get_favorites());
	exercise_result_surface(window->get_places());
	exercise_view_replacement(plugin->get_settings(),
			window->get_favorites(), window->get_places());

	std::printf("test_result_surfaces: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
