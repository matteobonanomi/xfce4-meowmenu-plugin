/*
 * Settings signal ownership checks against a retained Xfconf sender.
 */

#include <cassert>
#include <cstdio>

#include <xfconf/xfconf.h>

#include "core/plugin.h"
#include "private-xfconf-fixture.h"
#include "settings.h"

using namespace WhiskerMenu;

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
			"comment", "Settings lifecycle test host",
			"unique-id", 9007,
			nullptr));
	assert(host);
	Plugin* plugin = new Plugin(host);
	XfconfChannel* channel = XFCONF_CHANNEL(
			g_object_ref(plugin->get_settings()->channel));

	g_signal_emit_by_name(host, "free-data");

	GValue value = G_VALUE_INIT;
	g_value_init(&value, G_TYPE_INT);
	g_value_set_int(&value, 73);
	g_signal_emit_by_name(channel, "property-changed", "/menu-opacity", &value);
	g_value_unset(&value);
	g_object_unref(channel);

	std::printf("test_settings_lifecycle: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
