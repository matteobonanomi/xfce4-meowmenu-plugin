/*
 * Production-linked characterization of Settings notification fan-out.
 */

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include <xfconf/xfconf.h>

#include "core/plugin.h"
#include "core/window.h"
#include "private-xfconf-fixture.h"
#include "settings-dialog.h"
#include "settings.h"

using namespace WhiskerMenu;

namespace
{

struct PropertyCount
{
	const char* property;
	unsigned int count;
};

void collect_widgets(GtkWidget* widget, std::vector<GtkWidget*>* widgets)
{
	widgets->push_back(widget);
	if (!GTK_IS_CONTAINER(widget))
		return;
	GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
	for (GList* child = children; child; child = child->next)
		collect_widgets(GTK_WIDGET(child->data), widgets);
	g_list_free(children);
}

GtkSpinButton* find_spin_with_value(const std::vector<GtkWidget*>& widgets,
		double value)
{
	for (GtkWidget* widget : widgets)
	{
		if (GTK_IS_SPIN_BUTTON(widget)
				&& std::fabs(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget))
						- value) < 0.1)
		{
			return GTK_SPIN_BUTTON(widget);
		}
	}
	return nullptr;
}

void drain_ready_sources()
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
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
			"comment", "Settings dispatch test host",
			"unique-id", 9009,
			nullptr));
	assert(host);
	assert(xfconf_init(nullptr));
	XfconfChannel* seed = xfconf_channel_new_with_property_base(
			xfce_panel_get_channel_name(),
			xfce_panel_plugin_get_property_base(host));
	assert(seed);
	assert(xfconf_channel_set_bool(seed, "/initialized", true));
	assert(xfconf_channel_set_int(seed, "/schema-version", 13));
	g_object_unref(seed);

	Plugin* plugin = new Plugin(host);
	Settings* settings = plugin->get_settings();
	Window* window = plugin->get_window();
	SettingsDialog* dialog = new SettingsDialog(settings, plugin);
	std::vector<GtkWidget*> widgets;
	collect_widgets(dialog->get_widget(), &widgets);

	PropertyCount observed = { nullptr, 0 };
	gulong observer = g_signal_connect(settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property,
					const GValue*, gpointer data)
			{
				auto* state = static_cast<PropertyCount*>(data);
				if (g_strcmp0(property, state->property) == 0)
					++state->count;
			}), &observed);

	// External writes reach Settings before later observers and select the
	// existing Button refresh intent once. Live title editing intentionally uses
	// Plugin::set_button_title(), so this path does not assert that transaction.
	observed = { "/button-title", 0 };
	assert(xfconf_channel_set_string(settings->channel, observed.property,
			"Dispatch title"));
	assert(observed.count == 1);
	assert(settings->button_title == "Dispatch title");

	// The dialog's independent size subscription still observes local writes
	// while Settings blocks its own property handler.
	GtkSpinButton* width = find_spin_with_value(widgets, settings->menu_width);
	assert(width);
	observed = { "/menu-width", 0 };
	gtk_spin_button_set_value(width, 612.0);
	assert(observed.count == 1);
	assert(static_cast<int>(settings->menu_width) == 612);
	assert(gtk_spin_button_get_value_as_int(width) == 612);

	// Both coupled-setting repairs remain part of every external notification.
	assert(xfconf_channel_set_bool(settings->channel,
			"/category-show-name", false));
	assert(xfconf_channel_set_int(settings->channel,
			"/category-icon-size", IconSize::NONE));
	assert(settings->category_show_name);
	assert(xfconf_channel_set_int(settings->channel,
			"/recent-items-max", 0));
	assert(xfconf_channel_set_int(settings->channel,
			"/default-category", Settings::CategoryRecent));
	assert(static_cast<int>(settings->default_category)
			== Settings::CategoryFavorites);

	// Window-owned Places and live-presentation adapters remain responsive to
	// the same notification stream without becoming persistence authorities.
	assert(xfconf_channel_set_bool(settings->channel, "/places/enabled", true));
	assert(settings->places_enabled);
	assert(xfconf_channel_set_int(settings->channel, "/menu-opacity", 77));
	assert(static_cast<int>(settings->menu_opacity) == 77);
	assert(xfconf_channel_set_int(settings->channel, "/corner-radius", 9));
	assert(static_cast<int>(settings->corner_radius) == 9);

	// Repeated style-affecting requests coalesce through the real Window and a
	// later request remains schedulable after the idle transaction completes.
	window->refresh_layout();
	window->refresh_layout();
	drain_ready_sources();
	window->refresh_layout();
	drain_ready_sources();

	g_signal_handler_disconnect(settings->channel, observer);
	delete dialog;
	g_signal_emit_by_name(host, "free-data");
	std::printf("test_settings_dispatch: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
