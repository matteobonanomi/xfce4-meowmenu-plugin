/*
 * Direct normal-path checks for panel title presentation and Properties open.
 */

#include <cassert>
#include <cstdio>
#include <cstring>

#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include "core/plugin.h"
#include "private-xfconf-fixture.h"
#include "settings.h"

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

GtkImage* find_image(GtkWidget* widget)
{
	if (GTK_IS_IMAGE(widget))
		return GTK_IMAGE(widget);
	if (!GTK_IS_CONTAINER(widget))
		return nullptr;
	GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
	GtkImage* found = nullptr;
	for (GList* child = children; child && !found; child = child->next)
		found = find_image(GTK_WIDGET(child->data));
	g_list_free(children);
	return found;
}

/* assert_title_presentation:
 * @plugin: live plugin whose title is updated.
 * @markup: stored panel title, including any supported Pango markup.
 * @visible_text: text expected after Pango markup is rendered.
 *
 * Confirms that one live title edit reaches both the panel label and tooltip.
 */
void assert_title_presentation(Plugin* plugin,
		const char* markup,
		const char* visible_text)
{
	plugin->set_button_title(markup);
	GtkLabel* label = find_label(plugin->get_button());
	assert(label);
	assert(std::strcmp(gtk_label_get_text(label), visible_text) == 0);
	gchar* tooltip = gtk_widget_get_tooltip_markup(plugin->get_button());
	assert(tooltip);
	assert(std::strcmp(tooltip, markup) == 0);
	g_free(tooltip);
	gchar* tooltip_text = gtk_widget_get_tooltip_text(plugin->get_button());
	assert(tooltip_text);
	assert(std::strcmp(tooltip_text, visible_text) == 0);
	g_free(tooltip_text);
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
	assert(xfconf_init(nullptr));
	XfconfChannel* channel = xfconf_channel_new_with_property_base(
			xfce_panel_get_channel_name(),
			xfce_panel_plugin_get_property_base(host));
	assert(channel);
	assert(xfconf_channel_set_bool(channel, "/initialized", true));
	assert(xfconf_channel_set_int(channel, "/schema-version", 13));
	assert(xfconf_channel_set_string(channel, "/button-title",
			"<b>Malformed initial title"));
	g_object_unref(channel);

	Plugin* plugin = new Plugin(host);
	GtkLabel* label = find_label(plugin->get_button());
	GtkImage* icon = find_image(plugin->get_button());
	assert(label);
	assert(icon);
	gchar* initial_tooltip =
			gtk_widget_get_tooltip_text(plugin->get_button());
	assert(std::strcmp(gtk_label_get_text(label),
			"<b>Malformed initial title") == 0);
	assert(initial_tooltip);
	assert(std::strcmp(initial_tooltip, "<b>Malformed initial title") == 0);
	g_free(initial_tooltip);

	plugin->set_button_style(Plugin::ShowIcon);
	assert(!gtk_widget_get_visible(GTK_WIDGET(label)));
	assert(gtk_widget_get_visible(GTK_WIDGET(icon)));
	assert(gtk_widget_get_has_tooltip(plugin->get_button()));

	plugin->set_button_style(Plugin::ShowText);
	assert(gtk_widget_get_visible(GTK_WIDGET(label)));
	assert(!gtk_widget_get_visible(GTK_WIDGET(icon)));
	assert(!gtk_widget_get_has_tooltip(plugin->get_button()));

	plugin->set_button_style(Plugin::ShowIconAndText);
	assert(gtk_widget_get_visible(GTK_WIDGET(label)));
	assert(gtk_widget_get_visible(GTK_WIDGET(icon)));
	assert(!gtk_widget_get_has_tooltip(plugin->get_button()));

	assert_title_presentation(plugin, "<b>Meow &amp; Menu</b>",
			"Meow & Menu");
	assert_title_presentation(plugin, "Meow Menu", "Meow Menu");
	assert_title_presentation(plugin,
			"Cats &amp; Kittens &lt;3 &gt; 0 — 100%",
			"Cats & Kittens <3 > 0 — 100%");
	assert_title_presentation(plugin, "<i>Live edit</i>", "Live edit");

	plugin->set_button_title("<b>Malformed title");
	gchar* malformed_tooltip =
			gtk_widget_get_tooltip_text(plugin->get_button());
	assert(std::strcmp(gtk_label_get_text(label), "<b>Malformed title") == 0);
	assert(malformed_tooltip);
	assert(std::strcmp(malformed_tooltip, "<b>Malformed title") == 0);
	g_free(malformed_tooltip);

	const int before = visible_properties_count();
	g_signal_emit_by_name(host, "configure-plugin");
	assert(visible_properties_count() == before + 1);
	g_signal_emit_by_name(host, "configure-plugin");
	assert(visible_properties_count() == before + 1);

	Settings* settings = plugin->get_settings();
	const int action_count = settings->search_actions.size();
	settings->search_actions.set_modified();
	XfconfChannel* retained_channel = XFCONF_CHANNEL(
			g_object_ref(settings->channel));
	int search_action_writes = 0;
	g_signal_connect(retained_channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property,
					const GValue*, gpointer data)
			{
				if (g_strcmp0(property, "/search-actions") == 0)
					++*static_cast<int*>(data);
			}), &search_action_writes);

	g_signal_emit_by_name(host, "free-data");
	while (g_main_context_iteration(nullptr, false))
	{
	}
	assert(visible_properties_count() == before);
	assert(xfconf_channel_get_int(retained_channel,
			"/search-actions", -1) == action_count);
	assert(search_action_writes == 1);
	g_object_unref(retained_channel);

	std::printf("test_plugin_lifecycle: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
