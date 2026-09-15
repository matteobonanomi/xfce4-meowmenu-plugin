/*
 * Direct normal-path transactions on the real launcher Window.
 */

#include <cassert>
#include <cstdio>
#include <cstring>

#include "core/plugin.h"
#include "core/window.h"
#include "launcher/applications-page.h"
#include "launcher/favorites-page.h"
#include "launcher/recent-page.h"
#include "private-xfconf-fixture.h"
#include "settings.h"

using namespace WhiskerMenu;

namespace
{

/* assert_dismissal_family_inventory:
 *
 * Keeps the external call-family audit beside the production transaction test.
 * The first ten entries carry a user action; the Plugin entries are explicit
 * toggle/reload/remote dismissals. Focus loss is exercised separately below.
 */
void assert_dismissal_family_inventory()
{
	static const char* const families[] = {
		"places-row-open",
		"places-menu-open",
		"places-open-containing",
		"places-open-terminal",
		"launcher-run",
		"launcher-desktop-action",
		"launcher-hide",
		"launcher-edit",
		"search-calculator-copy",
		"profile-command",
		"plugin-button-toggle",
		"plugin-reload",
		"plugin-remote-toggle",
	};
	static_assert(G_N_ELEMENTS(families) == 13,
			"external dismissal family inventory changed");
	for (const char* family : families)
		assert(family && std::strlen(family) > 0);
}

bool wait_for_publication(ApplicationsPage* applications)
{
	const gint64 deadline = g_get_monotonic_time() + (5 * G_USEC_PER_SEC);
	while (!applications->has_publication()
			&& g_get_monotonic_time() < deadline)
	{
		g_main_context_iteration(nullptr, TRUE);
	}
	return applications->has_publication();
}

void drain_ready_sources()
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
}

Page* expected_default_page(Settings* settings, Window* window)
{
	switch (static_cast<Settings::DefaultCategory>(
			static_cast<int>(settings->default_category)))
	{
	case Settings::CategoryRecent:
		return window->get_recent();
	case Settings::CategoryAll:
		return window->get_applications();
	default:
		return window->get_favorites();
	}
}

}

static int run_test(int argc, char** argv)
{
	assert_dismissal_family_inventory();

	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	XfcePanelPlugin* host = XFCE_PANEL_PLUGIN(g_object_new(
			XFCE_TYPE_PANEL_PLUGIN,
			"name", "meowmenu",
			"display-name", "MeowMenu",
			"comment", "Window transaction test host",
			"unique-id", 9004,
			nullptr));
	assert(host);
	Plugin* plugin = new Plugin(host);
	Settings* settings = plugin->get_settings();
	Window* window = plugin->get_window();
	assert(wait_for_publication(window->get_applications()));
	assert(window->get_active_page() == expected_default_page(settings, window));

	settings->menu_width = 480;
	settings->menu_height = 520;
	settings->menu_opacity = 83;
	settings->layout_mode = "docked";
	settings->sidebar_enabled = true;
	settings->sidebar_position = "horizontal";
	settings->places_enabled = true;
	window->show(Window::PositionAtCenter);
	assert(gtk_widget_get_visible(window->get_widget()));
	assert(window->get_result_toplevel_width_authority() == 480);
	assert(window->get_result_viewport_width_cap() == -1);
	settings->layout_mode = "centered";
	assert(window->get_result_toplevel_width_authority() == 480);
	assert(window->get_result_viewport_width_cap() == -1);
	settings->layout_mode = "unsupported";
	assert(window->get_result_toplevel_width_authority() == 480);
	assert(window->get_result_viewport_width_cap() == -1);
	settings->layout_mode = "fullscreen";
	const int fullscreen_width = window->get_result_toplevel_width_authority();
	assert(fullscreen_width > 0);
	assert(window->get_result_viewport_width_cap()
			== meow_fullscreen_main_column(fullscreen_width).width);
	settings->layout_mode = "docked";
	window->refresh_layout();
	window->refresh_layout();
	drain_ready_sources();
	assert(gtk_widget_get_visible(window->get_widget()));

	window->switch_mode(true);
	assert(window->is_places_active());
	window->switch_mode(false);
	assert(!window->is_places_active());

	window->set_child_has_focus();
	GdkEventFocus focus_out = {};
	focus_out.type = GDK_FOCUS_CHANGE;
	focus_out.in = FALSE;
	gboolean handled = FALSE;
	g_signal_emit_by_name(window->get_widget(), "focus-out-event",
			&focus_out, &handled);
	drain_ready_sources();
	assert(gtk_widget_get_visible(window->get_widget()));

	GdkEventFocus focus_in = {};
	focus_in.type = GDK_FOCUS_CHANGE;
	focus_in.in = TRUE;
	g_signal_emit_by_name(window->get_widget(), "focus-in-event",
			&focus_in, &handled);
	g_signal_emit_by_name(window->get_widget(), "focus-out-event",
			&focus_out, &handled);
	assert(gtk_widget_get_visible(window->get_widget()));
	drain_ready_sources();
	assert(!gtk_widget_get_visible(window->get_widget()));
	gboolean remote_handled = FALSE;
	g_signal_emit_by_name(host, "remote-event", "popup", nullptr,
			&remote_handled);
	assert(remote_handled);
	assert(!gtk_widget_get_visible(window->get_widget()));

	settings->default_category = Settings::CategoryFavorites;
	window->show(Window::PositionAtCenter);
	window->dismiss(Window::DismissReason::Explicit);
	assert(!gtk_widget_get_visible(window->get_widget()));
	assert(window->get_active_page() == window->get_favorites());

	window->show(Window::PositionAtCenter);
	window->switch_mode(false);
	gtk_entry_set_text(window->get_search_entry(), "dismissal-order");
	Page* action_page = window->get_active_page();
	bool action_ran = false;
	window->perform_then_dismiss(
			[&]()
			{
				assert(gtk_widget_get_visible(window->get_widget()));
				assert(window->get_active_page() == action_page);
				assert(std::strcmp(gtk_entry_get_text(window->get_search_entry()),
						"dismissal-order") == 0);
				action_ran = true;
			});
	assert(action_ran);
	assert(!gtk_widget_get_visible(window->get_widget()));
	assert(window->get_active_page() == window->get_favorites());

	window->show(Window::PositionAtCenter);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugin->get_button()), true);
	GdkEventButton button_press = {};
	button_press.type = GDK_BUTTON_PRESS;
	button_press.button = 1;
	g_signal_emit_by_name(window->get_widget(), "focus-out-event",
			&focus_out, &handled);
	assert(gtk_widget_get_visible(window->get_widget()));
	gboolean button_handled = FALSE;
	g_signal_emit_by_name(plugin->get_button(), "button-press-event",
			&button_press, &button_handled);
	assert(button_handled);
	assert(!gtk_widget_get_visible(window->get_widget()));
	drain_ready_sources();
	assert(!gtk_widget_get_visible(window->get_widget()));

	window->show(Window::PositionAtCenter);
	assert(gtk_widget_get_visible(window->get_widget()));
	g_signal_emit_by_name(window->get_widget(), "focus-out-event",
			&focus_out, &handled);
	g_signal_emit_by_name(host, "free-data");
	drain_ready_sources();

	std::printf("test_window_transactions: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
