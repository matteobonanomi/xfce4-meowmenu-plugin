/*
 * Direct normal-path checks for panel title presentation and Properties open.
 */

#include <cassert>
#include <cstdio>
#include <cstring>

#include <libxfce4ui/libxfce4ui.h>

#include "core/plugin.h"
#include "private-xfconf-fixture.h"

using namespace WhiskerMenu;

namespace
{

GtkLabel* find_label(GtkWidget* widget)
{
	if (GTK_IS_LABEL(widget))
		return GTK_LABEL(widget);
	if (!GTK_IS_CONTAINER(widget))
		return nullptr;
	GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
	GtkLabel* found = nullptr;
	for (GList* child = children; child && !found; child = child->next)
		found = find_label(GTK_WIDGET(child->data));
	g_list_free(children);
	return found;
}

int visible_properties_count()
{
	int count = 0;
	GList* windows = gtk_window_list_toplevels();
	for (GList* item = windows; item; item = item->next)
	{
		GtkWidget* widget = GTK_WIDGET(item->data);
		if (XFCE_IS_TITLED_DIALOG(widget) && gtk_widget_get_visible(widget)
				&& g_strcmp0(gtk_window_get_title(GTK_WINDOW(widget)),
						"Meow Menu") == 0)
		{
			++count;
		}
	}
	g_list_free(windows);
	return count;
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
			"comment", "Plugin lifecycle test host",
			"unique-id", 9005,
			nullptr));
	assert(host);
	Plugin* plugin = new Plugin(host);
	plugin->set_button_style(Plugin::ShowIconAndText);
	plugin->set_button_title("<b>Meow &amp; Menu</b>");
	GtkLabel* label = find_label(plugin->get_button());
	assert(label);
	assert(std::strcmp(gtk_label_get_text(label), "Meow & Menu") == 0);
	gchar* tooltip = gtk_widget_get_tooltip_markup(plugin->get_button());
	assert(tooltip);
	assert(std::strcmp(tooltip, "<b>Meow &amp; Menu</b>") == 0);
	g_free(tooltip);

	const int before = visible_properties_count();
	g_signal_emit_by_name(host, "configure-plugin");
	assert(visible_properties_count() == before + 1);
	g_signal_emit_by_name(host, "configure-plugin");
	assert(visible_properties_count() == before + 1);

	std::printf("test_plugin_lifecycle: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
