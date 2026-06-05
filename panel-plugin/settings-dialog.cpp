/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "settings-dialog.h"

#include "launcher/applications-page.h"
#include "launcher/command.h"
#include "ui/command-edit.h"
#include "ui/icon-size.h"
#include "launcher/launcher.h"
#include "core/plugin.h"
#include "search/search-action.h"
#include "settings.h"
#include "ui/slot.h"
#include "core/window.h"
#include "core/window-size-clamp.h"
#include "presets/preset.h"
#include "presets/preset-io.h"
#include "search/unified-bar.h"
#include "ui/properties/common.h"

#include <algorithm>

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4ui/libxfce4ui.h>

#if !LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
#include <exo/exo.h>
#endif

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

namespace
{

enum
{
	COLUMN_NAME,
	COLUMN_PATTERN,
	COLUMN_ACTION,
	N_COLUMNS
};

}

//-----------------------------------------------------------------------------

SettingsDialog::SettingsDialog(Settings* settings, Plugin* plugin) :
	m_settings(settings),
	m_plugin(plugin)
{
	// Create dialog window
	m_window = xfce_titled_dialog_new_with_mixed_buttons(_("Meow Menu"),
			nullptr,
			GtkDialogFlags(0),
			"help-browser", _("_Help"), GTK_RESPONSE_HELP,
			"window-close-symbolic", _("_Close"), GTK_RESPONSE_CLOSE,
			nullptr);
	gtk_window_set_type_hint(GTK_WINDOW(m_window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_icon_name(GTK_WINDOW(m_window), "org.xfce.panel.meowmenu");
	gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);

	connect(m_window, "response",
		[this](GtkDialog*, int response_id)
		{
			response(response_id);
		});

	// Create sidebar navigation
	GtkStack* stack = GTK_STACK(gtk_stack_new());
	gtk_stack_set_transition_type(stack, GTK_STACK_TRANSITION_TYPE_NONE);

	// Load built-in presets from on-disk files before any tab builder calls
	// refresh_preset_combo (T041 / data-model E-1).
	initialize_file_presets();

	// New 5-tab dictionary per data-model.md E-3. Each init_*_tab() already
	// returns its content wrapped by wrap_in_scrolled().
	auto add_page = [stack](GtkWidget* child, const char* id, const char* title)
	{
		gtk_stack_add_titled(stack, child, id, title);
	};

	add_page(init_general_tab(),       "general", _("General"));
	add_page(init_user_session_tab(),  "user",    _("User / Session"));
	add_page(init_search_bar_tab(),    "search",  _("Search Bar"));
	add_page(init_app_grid_tab(),      "app-grid", _("Results View"));
	add_page(init_sidebar_tab(),       "sidebar", _("Sidebar"));
	add_page(init_places_tab(),        "places",  _("Places"));

	GtkStackSidebar* sidebar = GTK_STACK_SIDEBAR(gtk_stack_sidebar_new());
	gtk_stack_sidebar_set_stack(sidebar, stack);
	gtk_widget_set_size_request(GTK_WIDGET(sidebar), 160, -1);

	GtkBox* hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_box_pack_start(hbox, GTK_WIDGET(sidebar), false, false, 0);
	gtk_box_pack_start(hbox, gtk_separator_new(GTK_ORIENTATION_VERTICAL), false, false, 0);
	gtk_box_pack_start(hbox, GTK_WIDGET(stack), true, true, 0);

	GtkBox* contents = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(m_window)));
	gtk_box_pack_start(contents, GTK_WIDGET(hbox), true, true, 0);
	// Clamp the 820x600 logical default to the active monitor's work area so
	// the preferences window can never open off-screen or larger than the
	// screen at very large effective scales. Shrink-only, so at a normal 1x
	// work area the size passes through unchanged.
	//
	// HACK: the window is not realized yet here, so its own GdkWindow is not
	// available; we resolve the monitor under it when possible and otherwise
	// fall back to the primary (then first) monitor. If no monitor can be
	// resolved at all (no display / headless), the work area is left 0x0,
	// which the clamp treats as "no constraint" and returns the size as-is.
	{
		GdkRectangle workarea = { 0, 0, 0, 0 };
		GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(m_window));
		GdkMonitor* monitor = nullptr;
		if (GdkWindow* gdkwin = gtk_widget_get_window(GTK_WIDGET(m_window)))
			monitor = gdk_display_get_monitor_at_window(display, gdkwin);
		if (!monitor)
			monitor = gdk_display_get_primary_monitor(display);
		if (!monitor && gdk_display_get_n_monitors(display) > 0)
			monitor = gdk_display_get_monitor(display, 0);
		if (monitor)
			gdk_monitor_get_workarea(monitor, &workarea);

		int clamped_w = 0, clamped_h = 0;
		meow::clamp_default_size(820, 600, workarea.width, workarea.height,
			&clamped_w, &clamped_h);
		gtk_window_set_default_size(GTK_WINDOW(m_window), clamped_w, clamped_h);
	}

	// Mirror live menu-width / menu-height changes (e.g. drag-resizing the
	// menu window) into the spin buttons. The Settings::property_changed slot
	// is blocked during local writes via begin/end_property_update, but our
	// dedicated handler is independent, so it fires whenever Xfconf updates.
	if (m_settings->channel)
	{
		m_size_change_slot = g_signal_connect(m_settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue* value, gpointer data) -> void
			{
				auto* self = static_cast<SettingsDialog*>(data);
				if (!G_VALUE_HOLDS_INT(value))
					return;
				const int v = g_value_get_int(value);
				if (g_strcmp0(property, "/menu-width") == 0 && self->m_menu_width)
					gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->m_menu_width), v);
				else if (g_strcmp0(property, "/menu-height") == 0 && self->m_menu_height)
					gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->m_menu_height), v);
			}), this);
	}

	install_layout_mode_handler();
	apply_layout_mode_sensitivity();

	// Show GTK window
	gtk_widget_show_all(m_window);
}

//-----------------------------------------------------------------------------

SettingsDialog::~SettingsDialog()
{
	if (m_size_change_slot && m_settings && m_settings->channel)
	{
		g_signal_handler_disconnect(m_settings->channel, m_size_change_slot);
		m_size_change_slot = 0;
	}

	if (m_layout_mode_slot && m_settings && m_settings->channel)
	{
		g_signal_handler_disconnect(m_settings->channel, m_layout_mode_slot);
		m_layout_mode_slot = 0;
	}

	if (m_unified_bar_slot && m_settings && m_settings->channel)
	{
		g_signal_handler_disconnect(m_settings->channel, m_unified_bar_slot);
		m_unified_bar_slot = 0;
	}

	for (auto command : m_commands)
	{
		delete command;
	}

	g_object_unref(m_actions_model);
	g_object_unref(m_aliases_model);
}

//-----------------------------------------------------------------------------

void SettingsDialog::choose_icon()
{
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
	GtkWidget* chooser = xfce_icon_chooser_dialog_new(_("Select an Icon"),
#else
	GtkWidget* chooser = exo_icon_chooser_dialog_new(_("Select an Icon"),
#endif
			GTK_WINDOW(m_window),
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_OK"), GTK_RESPONSE_ACCEPT,
			nullptr);

	gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
	xfce_icon_chooser_dialog_set_icon(XFCE_ICON_CHOOSER_DIALOG(chooser), m_settings->button_icon_name);
#else
	exo_icon_chooser_dialog_set_icon(EXO_ICON_CHOOSER_DIALOG(chooser), m_settings->button_icon_name);
#endif

	if (gtk_dialog_run(GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
	{
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
		gchar* icon = xfce_icon_chooser_dialog_get_icon(XFCE_ICON_CHOOSER_DIALOG(chooser));
#else
		gchar* icon = exo_icon_chooser_dialog_get_icon(EXO_ICON_CHOOSER_DIALOG(chooser));
#endif
		gtk_image_set_from_icon_name(GTK_IMAGE(m_icon), icon, GTK_ICON_SIZE_DIALOG);
		m_plugin->set_button_icon_name(icon);
		g_free(icon);
	}

	gtk_widget_destroy(chooser);
}

//-----------------------------------------------------------------------------

SearchAction* SettingsDialog::get_selected_action(GtkTreeIter* iter) const
{
	GtkTreeIter selected_iter;
	if (!iter)
	{
		iter = &selected_iter;
	}

	SearchAction* action = nullptr;
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_actions_view);
	GtkTreeModel* model = nullptr;
	if (gtk_tree_selection_get_selected(selection, &model, iter))
	{
		gtk_tree_model_get(model, iter, COLUMN_ACTION, &action, -1);
	}
	return action;
}

//-----------------------------------------------------------------------------

void SettingsDialog::add_action()
{
	// Add to action list
	SearchAction* action = new SearchAction(m_settings);
	m_settings->search_actions.push_back(action);

	// Add to model
	GtkTreeIter iter;
	gtk_list_store_insert_with_values(m_actions_model, &iter, G_MAXINT,
			COLUMN_NAME, "",
			COLUMN_PATTERN, "",
			COLUMN_ACTION, action,
			-1);
	GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(m_actions_model), &iter);
	gtk_tree_view_set_cursor(m_actions_view, path, nullptr, false);
	gtk_tree_path_free(path);

	// Make sure editing is allowed.
	gtk_widget_set_sensitive(m_action_remove, true);
	// Legacy inline-detail widgets only exist when init_search_actions_tab()
	// is in use (will be removed in T012). When the new modal-based tab owns
	// the list, these are nullptr and editing happens through the modal.
	if (m_action_name)    gtk_widget_set_sensitive(m_action_name, true);
	if (m_action_pattern) gtk_widget_set_sensitive(m_action_pattern, true);
	if (m_action_command) gtk_widget_set_sensitive(m_action_command, true);
	if (m_action_regex)   gtk_widget_set_sensitive(m_action_regex, true);

	// Immediately open the modal so the user can populate the new action.
	if (!m_action_name)
		edit_search_action_modal(action);
}

//-----------------------------------------------------------------------------

void SettingsDialog::remove_action()
{
	// Fetch action
	GtkTreeIter iter;
	SearchAction* action = get_selected_action(&iter);
	if (!action)
	{
		return;
	}

	// Confirm removal
	if (!xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
			"edit-delete", _("_Delete"),
			_("The action will be deleted permanently."),
			_("Remove action \"%s\"?"),
			action->get_name()))
	{
		return;
	}

	// Fetch path of previous action
	GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(m_actions_model), &iter);
	if (!gtk_tree_path_prev(path))
	{
		gtk_tree_path_free(path);
		path = nullptr;
	}

	// Remove from model
	if (gtk_list_store_remove(m_actions_model, &iter))
	{
		if (path)
		{
			gtk_tree_path_free(path);
		}
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(m_actions_model), &iter);
	}

	// Remove from list
	m_settings->search_actions.erase(action);
	delete action;

	// Select next action
	if (path)
	{
		gtk_tree_view_set_cursor(m_actions_view, path, nullptr, false);
		gtk_tree_path_free(path);
	}
	else
	{
		gtk_widget_set_sensitive(m_action_remove, false);
		// Null-guarded for the modal-based tab (legacy inline detail widgets
		// disappear with T012).
		if (m_action_name)
		{
			gtk_entry_set_text(GTK_ENTRY(m_action_name), "");
			gtk_widget_set_sensitive(m_action_name, false);
		}
		if (m_action_pattern)
		{
			gtk_entry_set_text(GTK_ENTRY(m_action_pattern), "");
			gtk_widget_set_sensitive(m_action_pattern, false);
		}
		if (m_action_command)
		{
			gtk_entry_set_text(GTK_ENTRY(m_action_command), "");
			gtk_widget_set_sensitive(m_action_command, false);
		}
		if (m_action_regex)
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_action_regex), false);
			gtk_widget_set_sensitive(m_action_regex, false);
		}
	}
}

//-----------------------------------------------------------------------------

/* edit_search_action_modal:
 * @action: the SearchAction to edit; must not be NULL.
 *
 * Opens a transient-for modal dialog with fields Name / Pattern / Command /
 * Is-regex (research R-7, data-model E-5). OK persists the values into the
 * action and updates the list-store row; Cancel discards. The on-disk flush
 * continues to happen via the existing save path in plugin.cpp.
 */
void SettingsDialog::edit_search_action_modal(SearchAction* action)
{
	GtkWidget* dlg = gtk_dialog_new_with_buttons(_("Edit search action"),
		GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
		GTK_DIALOG_MODAL,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"),     GTK_RESPONSE_OK,
		nullptr);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	GtkGrid* grid = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(grid, 12);
	gtk_grid_set_row_spacing(grid, 6);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))),
		GTK_WIDGET(grid), true, true, 0);

	GtkWidget* name_label = gtk_label_new_with_mnemonic(_("Nam_e:"));
	gtk_widget_set_halign(name_label, GTK_ALIGN_START);
	gtk_grid_attach(grid, name_label, 0, 0, 1, 1);
	GtkWidget* name_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name_entry), action->get_name());
	gtk_entry_set_activates_default(GTK_ENTRY(name_entry), true);
	gtk_widget_set_hexpand(name_entry, true);
	gtk_grid_attach(grid, name_entry, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(name_label), name_entry);

	GtkWidget* pat_label = gtk_label_new_with_mnemonic(_("_Pattern:"));
	gtk_widget_set_halign(pat_label, GTK_ALIGN_START);
	gtk_grid_attach(grid, pat_label, 0, 1, 1, 1);
	GtkWidget* pat_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(pat_entry), action->get_pattern());
	gtk_entry_set_activates_default(GTK_ENTRY(pat_entry), true);
	gtk_widget_set_hexpand(pat_entry, true);
	gtk_grid_attach(grid, pat_entry, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(pat_label), pat_entry);

	GtkWidget* cmd_label = gtk_label_new_with_mnemonic(_("C_ommand:"));
	gtk_widget_set_halign(cmd_label, GTK_ALIGN_START);
	gtk_grid_attach(grid, cmd_label, 0, 2, 1, 1);
	GtkWidget* cmd_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(cmd_entry), action->get_command());
	gtk_entry_set_activates_default(GTK_ENTRY(cmd_entry), true);
	gtk_widget_set_hexpand(cmd_entry, true);
	gtk_grid_attach(grid, cmd_entry, 1, 2, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(cmd_label), cmd_entry);

	GtkWidget* regex_check = gtk_check_button_new_with_mnemonic(_("Is _regular expression"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(regex_check), action->get_is_regex());
	gtk_grid_attach(grid, regex_check, 1, 3, 1, 1);

	gtk_widget_show_all(dlg);
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
	{
		const gchar* new_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
		const gchar* new_pat  = gtk_entry_get_text(GTK_ENTRY(pat_entry));
		const gchar* new_cmd  = gtk_entry_get_text(GTK_ENTRY(cmd_entry));
		const bool   new_regex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(regex_check));

		action->set_name(new_name);
		action->set_pattern(new_pat);
		action->set_command(new_cmd);
		action->set_is_regex(new_regex);

		// Reflect into the visible list row.
		GtkTreeIter iter;
		if (get_selected_action(&iter))
		{
			gtk_list_store_set(m_actions_model, &iter,
				COLUMN_NAME, new_name,
				COLUMN_PATTERN, new_pat,
				-1);
		}
	}
	gtk_widget_destroy(dlg);
}

//-----------------------------------------------------------------------------

void SettingsDialog::response(int response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
	{
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
		bool result = g_spawn_command_line_async("xfce-open --launch WebBrowser " PLUGIN_WEBSITE, nullptr);
#else
		bool result = g_spawn_command_line_async("exo-open --launch WebBrowser " PLUGIN_WEBSITE, nullptr);
#endif

		if (G_UNLIKELY(!result))
		{
			g_warning(_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
		}
	}
	else
	{
		if ((m_plugin->get_button_style() == Plugin::ShowText) && m_settings->button_title.empty())
		{
			m_plugin->set_button_title(m_plugin->get_button_title_default());
		}

		for (auto command : m_settings->command)
		{
			command->check();
		}

		if (response_id == GTK_RESPONSE_CLOSE)
		{
			m_settings->save_aliases(m_settings->channel);
			gtk_widget_destroy(m_window);
		}
	}
}

//-----------------------------------------------------------------------------

void SettingsDialog::refresh_customized_indicator()
{
	const gchar* pid = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
	const LayoutPreset* preset = find_preset_by_id(pid ? std::string(pid) : std::string());
	if (preset)
	{
		gtk_widget_set_visible(m_preset_customized, compute_preset_diff(*preset, *m_settings));
	}
}

void SettingsDialog::update_grid_controls_state()
{
	if (!m_grid_density_combo)
	{
		return;
	}

	const bool icons_view = (static_cast<int>(m_settings->view_mode) == Settings::ViewAsIcons);
	gtk_widget_set_sensitive(m_grid_density_combo, icons_view);
}

void SettingsDialog::sync_preset_widgets()
{
	// Drive every preset-governed widget across all tabs to match the current
	// settings after a preset is applied, then recompute every dependent
	// control's sensitivity. The whole body runs under m_programmatic_update so
	// the cascade of set_active calls cannot write a divergent value back into
	// Settings (FR-004) — each widget handler early-returns while it is set.
	//
	// The authoritative set of keys driven here is synced_keys(); a unit test
	// asserts it equals governed_keys() so no governed control is left stale
	// (FR-001/003). When adding a governed key, add both its sync call below
	// and its entry in synced_keys().
	//
	// NOTE: each widget is null-guarded because tabs are built independently;
	// a widget owned by an as-yet-unbuilt tab is nullptr.

	m_programmatic_update = true;

	if (m_layout_mode_combo)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_layout_mode_combo),
			static_cast<const gchar*>(m_settings->layout_mode));

	if (m_corner_radius)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_corner_radius),
			static_cast<int>(m_settings->corner_radius));
	if (m_panel_gap)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_panel_gap),
			static_cast<int>(m_settings->panel_gap));
	if (m_menu_width)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_width),
			static_cast<int>(m_settings->menu_width));
	if (m_menu_height)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_height),
			static_cast<int>(m_settings->menu_height));
	if (m_full_screen_opacity)
		gtk_range_set_value(GTK_RANGE(m_full_screen_opacity),
			static_cast<int>(m_settings->full_screen_opacity));
	if (m_categories_opacity)
		gtk_range_set_value(GTK_RANGE(m_categories_opacity),
			static_cast<int>(m_settings->categories_opacity));
	if (m_apps_opacity)
		gtk_range_set_value(GTK_RANGE(m_apps_opacity),
			static_cast<int>(m_settings->apps_opacity));

	if (m_sidebar_position_combo)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_sidebar_position_combo),
			static_cast<const gchar*>(m_settings->sidebar_position));
	if (m_enable_sidebar_switch)
		gtk_switch_set_active(GTK_SWITCH(m_enable_sidebar_switch),
			static_cast<bool>(m_settings->sidebar_enabled));
	if (m_show_category_names)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_category_names),
			static_cast<bool>(m_settings->category_show_name));
	if (m_search_bar_position_combo)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_search_bar_position_combo),
			static_cast<const gchar*>(m_settings->search_bar_position));
	if (m_profile_position_combo)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_profile_position_combo),
			static_cast<const gchar*>(m_settings->profile_position));
	if (m_commands_position_combo)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_commands_position_combo),
			static_cast<const gchar*>(m_settings->commands_position));

	if (m_grid_density_combo)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_grid_density_combo),
			static_cast<const gchar*>(m_settings->grid_density));

	if (m_places_enabled_switch)
		gtk_switch_set_active(GTK_SWITCH(m_places_enabled_switch),
			static_cast<bool>(m_settings->places_enabled));
	if (m_places_switch_show_icons)
		gtk_switch_set_active(GTK_SWITCH(m_places_switch_show_icons),
			static_cast<bool>(m_settings->places_switch_show_icons));

	if (m_hover_switch_category)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_hover_switch_category),
			static_cast<bool>(m_settings->category_hover_activate));
	if (m_unified_bar)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_unified_bar),
			static_cast<bool>(m_settings->unified_bar));
	if (m_item_icon_size)
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_item_icon_size),
			static_cast<int>(m_settings->launcher_icon_size) + 1);
	if (m_category_icon_size)
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_category_icon_size),
			static_cast<int>(m_settings->category_icon_size) + 1);
	if (m_position_categories_horizontal)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_position_categories_horizontal),
			static_cast<bool>(m_settings->position_categories_horizontal));

	if (m_stay_on_focus_out)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_stay_on_focus_out),
			static_cast<bool>(m_settings->stay_on_focus_out));

	if (m_button_title_visible)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_title_visible),
			static_cast<bool>(m_settings->button_title_visible));
	if (m_button_icon_visible)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_icon_visible),
			static_cast<bool>(m_settings->button_icon_visible));
	if (m_button_single_row)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_single_row),
			static_cast<bool>(m_settings->button_single_row));

	const int vm = static_cast<int>(m_settings->view_mode);
	if (m_show_as_icons && m_show_as_tree && m_show_as_list)
	{
		if (vm == Settings::ViewAsIcons)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_icons), true);
		else if (vm == Settings::ViewAsTree)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_tree), true);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_list), true);
	}

	const int dc = static_cast<int>(m_settings->default_category);
	if (m_display_favorites && m_display_recent && m_display_applications)
	{
		if (dc == Settings::CategoryRecent)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_recent), true);
		else if (dc == Settings::CategoryAll)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_applications), true);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_favorites), true);
	}

	// Recompute every dependent control's sensitivity across all tabs (FR-002):
	// grid controls, layout-mode-gated widgets, Places dependents, and the
	// Sidebar sub-enable greying. The Places/Sidebar hooks are owned by their
	// tab builders and may be empty until those tabs are built.
	update_grid_controls_state();
	apply_layout_mode_sensitivity();
	if (m_places_refresh_sensitivity)
		m_places_refresh_sensitivity();
	if (m_sidebar_apply_sub_enable)
		m_sidebar_apply_sub_enable();

	m_programmatic_update = false;
}

void SettingsDialog::refresh_preset_combo(const std::string& select_id)
{
	// Guard against the programmatic rebuild re-triggering the combo's "changed"
	// handler (which would re-apply a preset). Shares the dialog-wide flag.
	m_programmatic_update = true;

	// Preserve active ID before clearing
	const gchar* current_raw = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
	std::string active_id = select_id.empty()
		? (current_raw ? std::string(current_raw) : std::string())
		: select_id;

	// Rebuild combo entries — built-ins from on-disk files (T041), then user presets.
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(m_preset_combo));

	const auto& file_presets = get_file_presets();
	if (!file_presets.empty())
	{
		for (const auto& p : file_presets)
		{
			// Label from the stored identity name — one code path for built-in
			// and custom presets (FR-011a). For built-ins name == display name.
			const std::string& label = p.name.empty() ? p.display_name : p.name;
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo),
				p.id.c_str(), label.c_str());
		}
	}
	else
	{
		// NOTE: fallback if initialize_file_presets() was not called or all files missing.
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo), "classic",    _("Classic"));
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo), "modern",     _("Modern"));
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo), "fullscreen", _("Full Screen"));
	}

	const auto& user = enumerate_user_presets(m_settings->channel);
	for (const auto& p : user)
	{
		const std::string& label = p.name.empty() ? p.display_name : p.name;
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo),
			p.id.c_str(), label.c_str());
	}

	// Restore selection. The preset field must never be blank (FR-005): when the
	// stored id is unset or resolves to no row (empty, or a deleted/unknown
	// preset), present and select a synthetic non-blank "Custom" entry that
	// truthfully represents the current preset-less layout state.
	// HACK: gtk_combo_box_set_active_id silently leaves the combo with no active
	// selection when the id matches no row, which is exactly the blank case we
	// must avoid; we detect that via its return value and fall back to "Custom".
	static const char* const CUSTOM_PLACEHOLDER_ID = "__custom__";
	bool selected = false;
	if (!active_id.empty())
		selected = gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_preset_combo), active_id.c_str());

	bool is_custom_placeholder = false;
	if (!selected)
	{
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo),
			CUSTOM_PLACEHOLDER_ID, _("Custom"));
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_preset_combo), CUSTOM_PLACEHOLDER_ID);
		is_custom_placeholder = true;
	}

	// Update button sensitivity: Rename/Delete only for user (non-builtin)
	// presets. The synthetic "Custom" placeholder is not a real preset, so it
	// must never enable the user-preset actions.
	bool is_builtin = false;
	for (const auto& p : get_file_presets())
		if (p.id == active_id) { is_builtin = true; break; }
	if (!is_builtin)
	{
		for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
			if (BUILTIN_PRESETS[i].id == active_id) { is_builtin = true; break; }
	}
	bool is_user = !is_custom_placeholder
		&& (gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo)) != nullptr)
		&& !is_builtin;
	if (m_preset_rename_btn)
		gtk_widget_set_sensitive(m_preset_rename_btn, is_user);
	if (m_preset_delete_btn)
		gtk_widget_set_sensitive(m_preset_delete_btn, is_user);
	if (m_preset_export_btn)
		gtk_widget_set_sensitive(m_preset_export_btn, is_user);

	m_programmatic_update = false;
}

//-----------------------------------------------------------------------------

/* install_layout_mode_handler:
 *
 * Subscribes to the Xfconf channel's "property-changed" signal and triggers
 * apply_layout_mode_sensitivity() whenever /layout-mode flips. Per FR-003 the
 * transition must be instantaneous (no dialog close/reopen).
 */
void SettingsDialog::install_layout_mode_handler()
{
	if (!m_settings || !m_settings->channel)
		return;

	m_layout_mode_slot = g_signal_connect(m_settings->channel, "property-changed",
		G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue* value, gpointer data) -> void
		{
			if (g_strcmp0(property, "/layout-mode") != 0)
				return;
			if (!G_VALUE_HOLDS_STRING(value))
				return;
			static_cast<SettingsDialog*>(data)->apply_layout_mode_sensitivity();
		}), this);
}

/* apply_layout_mode_sensitivity:
 *
 * Walks the two per-mode widget vectors and calls gtk_widget_set_sensitive().
 * Builders push widgets onto m_layout_enable_when_docked or
 * m_layout_enable_when_fullscreen as they create them (data-model E-6).
 */
void SettingsDialog::apply_layout_mode_sensitivity()
{
	const bool is_fullscreen = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);
	for (GtkWidget* w : m_layout_enable_when_docked)
	{
		if (w)
			gtk_widget_set_sensitive(w, !is_fullscreen);
	}
	for (GtkWidget* w : m_layout_enable_when_fullscreen)
	{
		if (w)
			gtk_widget_set_sensitive(w, is_fullscreen);
	}
}

/* apply_unified_bar_sensitivity:
 *
 * Updates the live sensitivity, tooltip, and accessible description of the
 * unified-bar toggle. Reasons for being disabled (per contracts/settings-
 * dialog.md):
 *   - layout mode is not FullScreen, OR
 *   - profile/search/session resolve to different vertical ends.
 */
void SettingsDialog::apply_unified_bar_sensitivity()
{
	if (!m_unified_bar)
		return;

	const bool is_fullscreen = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);
	const bool preconditions = unified_bar_preconditions_met(*m_settings);
	const bool sensitive = preconditions;

	const char* tip;
	if (sensitive)
		tip = _("Render profile, search and session on a single horizontal row.");
	else if (!is_fullscreen)
		tip = _("This option requires the FullScreen layout.");
	else
		tip = _("This option requires profile, search and session to be on the same end (all top or all bottom).");

	gtk_widget_set_sensitive(m_unified_bar, sensitive);
	gtk_widget_set_tooltip_text(m_unified_bar, tip);
	atk_object_set_description(gtk_widget_get_accessible(m_unified_bar), tip);

	// Keep the displayed state in sync with the stored value even if it was
	// changed elsewhere (preset switch, xfconf-query, etc.).
	const gboolean active = static_cast<bool>(m_settings->unified_bar);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_unified_bar)) != active)
	{
		g_signal_handlers_block_matched(m_unified_bar, G_SIGNAL_MATCH_DATA,
			0, 0, nullptr, nullptr, this);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_unified_bar), active);
		g_signal_handlers_unblock_matched(m_unified_bar, G_SIGNAL_MATCH_DATA,
			0, 0, nullptr, nullptr, this);
	}
}

//-----------------------------------------------------------------------------
// Per-tab builders for the Properties dialog live in their own translation
// units under ui/properties/. Each init_*_tab() returns a fully wired
// scrolled container ready to be added to the dialog's stack.
//-----------------------------------------------------------------------------

