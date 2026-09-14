/*
 * Exact in-process drag target checks for application and Places results.
 */

#include <cassert>
#include <cstdio>
#include <cstring>

#include "core/plugin.h"
#include "core/window.h"
#include "launcher/favorites-page.h"
#include "launcher/page.h"
#include "places/places-page.h"
#include "private-xfconf-fixture.h"
#include "ui/launcher-view.h"

using namespace WhiskerMenu;

namespace
{

void assert_drag_target(GtkWidget* widget, const char* mime, guint expected_info)
{
	GtkTargetList* targets = gtk_drag_source_get_target_list(widget);
	assert(targets);
	guint info = 0;
	assert(gtk_target_list_find(targets, gdk_atom_intern(mime, FALSE), &info));
	assert(info == expected_info);
}

}

static int run_test(int argc, char** argv)
{
	assert(std::strcmp(WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET,
			"application/x-meowmenu-application-favourite") == 0);
	assert(WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO == 2);
	assert(std::strcmp(WHISKERMENU_PLACES_FAVOURITE_DND_TARGET,
			"application/x-meowmenu-places-favourite") == 0);
	assert(WHISKERMENU_PLACES_FAVOURITE_DND_INFO == 3);

	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	XfcePanelPlugin* host = XFCE_PANEL_PLUGIN(g_object_new(
			XFCE_TYPE_PANEL_PLUGIN,
			"name", "meowmenu",
			"display-name", "MeowMenu",
			"comment", "Drag contract test host",
			"unique-id", 9003,
			nullptr));
	assert(host);
	Plugin* plugin = new Plugin(host);
	Window* window = plugin->get_window();
	assert_drag_target(window->get_favorites()->get_view()->get_widget(),
			WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET,
			WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO);
	assert_drag_target(window->get_places()->get_view()->get_widget(),
			WHISKERMENU_PLACES_FAVOURITE_DND_TARGET,
			WHISKERMENU_PLACES_FAVOURITE_DND_INFO);

	std::printf("test_result_drag_contract: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
