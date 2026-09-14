/*
 * Direct normal-path checks for preset synchronization and compositor updates.
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/plugin.h"
#include "private-xfconf-fixture.h"
#include "settings-dialog.h"
#include "settings.h"

using namespace WhiskerMenu;

namespace
{

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

GtkComboBox* find_preset_combo(const std::vector<GtkWidget*>& widgets)
{
	for (GtkWidget* widget : widgets)
	{
		if (!GTK_IS_COMBO_BOX(widget))
			continue;
		GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
		if (model && gtk_tree_model_get_n_columns(model)
				== SettingsDialog::PRESET_N_COLS
				&& gtk_combo_box_get_id_column(GTK_COMBO_BOX(widget))
				== SettingsDialog::PRESET_COL_ID)
		{
			return GTK_COMBO_BOX(widget);
		}
	}
	return nullptr;
}

GtkWidget* find_opacity_scale(const std::vector<GtkWidget*>& widgets)
{
	for (GtkWidget* widget : widgets)
	{
		if (!GTK_IS_SCALE(widget))
			continue;
		GtkAdjustment* adjustment = gtk_range_get_adjustment(GTK_RANGE(widget));
		if (gtk_adjustment_get_lower(adjustment) == 0.0
				&& gtk_adjustment_get_upper(adjustment) == 100.0)
		{
			return widget;
		}
	}
	return nullptr;
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
			"comment", "Settings dialog test host",
			"unique-id", 9006,
			nullptr));
	assert(host);
	Plugin* plugin = new Plugin(host);
	Settings* settings = plugin->get_settings();
	SettingsDialog* dialog = new SettingsDialog(settings, plugin);
	std::vector<GtkWidget*> widgets;
	collect_widgets(dialog->get_widget(), &widgets);

	GtkComboBox* presets = find_preset_combo(widgets);
	assert(presets);
	const gchar* initial_id = gtk_combo_box_get_active_id(presets);
	assert(initial_id);
	assert(settings->current_preset_id == initial_id);
	assert(gtk_combo_box_set_active_id(presets, "classic"));
	assert(settings->current_preset_id == "classic");
	assert(std::strcmp(gtk_combo_box_get_active_id(presets), "classic") == 0);

	GtkWidget* opacity = find_opacity_scale(widgets);
	assert(opacity);
	GdkScreen* screen = gtk_widget_get_screen(opacity);
	assert(screen);
	const gboolean composited = gdk_screen_is_composited(screen);
	gtk_widget_set_sensitive(opacity, !composited);
	g_signal_emit_by_name(screen, "composited-changed");
	assert(gtk_widget_get_sensitive(opacity) == composited);

	delete dialog;
	gtk_widget_set_sensitive(opacity, !composited);
	g_signal_emit_by_name(screen, "composited-changed");
	assert(gtk_widget_get_sensitive(opacity) != composited);

	std::printf("test_settings_dialog_lifecycle: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
