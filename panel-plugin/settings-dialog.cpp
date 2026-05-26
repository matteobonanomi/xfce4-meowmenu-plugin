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

#include "applications-page.h"
#include "command.h"
#include "command-edit.h"
#include "icon-size.h"
#include "launcher.h"
#include "plugin.h"
#include "search-action.h"
#include "settings.h"
#include "slot.h"
#include "window.h"
#include "preset.h"
#include "preset-io.h"
#include "unified-bar.h"
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
	gtk_window_set_default_size(GTK_WINDOW(m_window), 820, 600);

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
	// Update all preset-governed widgets to match the current settings after a preset
	// is applied. Programmatic updates may fire their normal signal handlers; those
	// writes back to settings are no-ops (same value), but side effects like
	// grid control sensitivity and m_show_descriptions sensitivity are correct.
	//
	// NOTE: each widget is null-guarded because the 003-properties-refactor
	// lands one user-story tab at a time; widgets owned by an as-yet-unfilled
	// tab are nullptr until that story lands. Once US2–US6 are all in, none
	// of these guards short-circuit in normal operation.

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

	if (m_hover_switch_category)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_hover_switch_category),
			static_cast<bool>(m_settings->category_hover_activate));
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

	update_grid_controls_state();
	apply_layout_mode_sensitivity();
}

void SettingsDialog::refresh_preset_combo(const std::string& select_id)
{
	m_loading_preset = true;

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
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo),
				p.id.c_str(), p.display_name.c_str());
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
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo),
			p.id.c_str(), p.display_name.c_str());
	}

	// Restore selection
	if (!active_id.empty())
	{
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_preset_combo), active_id.c_str());
	}

	// Update button sensitivity: Rename/Delete only for user (non-builtin) presets.
	bool is_builtin = false;
	for (const auto& p : get_file_presets())
		if (p.id == active_id) { is_builtin = true; break; }
	if (!is_builtin)
	{
		for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
			if (BUILTIN_PRESETS[i].id == active_id) { is_builtin = true; break; }
	}
	bool is_user = (gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo)) != nullptr)
		&& !is_builtin;
	if (m_preset_rename_btn)
		gtk_widget_set_sensitive(m_preset_rename_btn, is_user);
	if (m_preset_delete_btn)
		gtk_widget_set_sensitive(m_preset_delete_btn, is_user);
	if (m_preset_export_btn)
		gtk_widget_set_sensitive(m_preset_export_btn, is_user);

	m_loading_preset = false;
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
// New 5-tab Properties dialog — empty stubs, filled by Phase 4–8 user stories.
//-----------------------------------------------------------------------------

/* init_search_bar_tab:
 *
 * Builds the Search Bar tab in the 003-properties-refactor 5-tab dictionary.
 * Sections (top-to-bottom, FR-030):
 *   1. Position        — search-bar-position combo.
 *   2. Ranking         — fuzzy matching, favorites boost, recency weight
 *                        (lifted from the legacy "Advanced Search" tab).
 *   3. Aliases         — desktop-id → search-term map.
 *   4. Search actions  — list of user-defined actions with add/remove/edit;
 *                        the Edit button opens a transient modal per
 *                        research R-7 / data-model E-5.
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_search_bar_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkSizeGroup* label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* control_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	// =========================================================================
	// 1. Position section
	// =========================================================================
	GtkGrid* pos_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(pos_table, 12);
	gtk_grid_set_row_spacing(pos_table, 6);

	GtkWidget* pos_frame = make_aligned_frame(_("Position"), GTK_WIDGET(pos_table));
	gtk_box_pack_start(page, pos_frame, false, false, 0);

	GtkWidget* sbp_label = gtk_label_new_with_mnemonic(_("_Search bar position:"));
	gtk_widget_set_halign(sbp_label, GTK_ALIGN_START);
	gtk_grid_attach(pos_table, sbp_label, 0, 0, 1, 1);

	m_search_bar_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_search_bar_position_combo), "top", _("Top"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_search_bar_position_combo), "bottom", _("Bottom"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_search_bar_position_combo),
		static_cast<const gchar*>(m_settings->search_bar_position));
	gtk_widget_set_halign(m_search_bar_position_combo, GTK_ALIGN_START);
	gtk_grid_attach(pos_table, m_search_bar_position_combo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(sbp_label), m_search_bar_position_combo);
	gtk_size_group_add_widget(label_size_group, sbp_label);
	gtk_size_group_add_widget(control_size_group, m_search_bar_position_combo);

	connect(m_search_bar_position_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			m_settings->search_bar_position = val;
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// =========================================================================
	// 2. Ranking section (lifted from legacy Advanced Search tab)
	// =========================================================================
	{
		GtkBox* row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));

		GtkWidget* lbl_fuzzy = gtk_label_new_with_mnemonic(_("_Fuzzy matching:"));
		gtk_widget_set_halign(lbl_fuzzy, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_fuzzy, GTK_ALIGN_CENTER);
		gtk_box_pack_start(row, lbl_fuzzy, false, false, 0);

		m_fuzzy_enabled = gtk_switch_new();
		gtk_widget_set_halign(m_fuzzy_enabled, GTK_ALIGN_START);
		gtk_widget_set_valign(m_fuzzy_enabled, GTK_ALIGN_CENTER);
		gtk_switch_set_active(GTK_SWITCH(m_fuzzy_enabled),
			static_cast<bool>(m_settings->fuzzy_enabled));
		gtk_box_pack_start(row, m_fuzzy_enabled, false, false, 0);
		gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_fuzzy), m_fuzzy_enabled);

		connect(m_fuzzy_enabled, "notify::active",
			[this](GObject* obj, GParamSpec*)
			{
				const bool active = gtk_switch_get_active(GTK_SWITCH(obj));
				m_settings->fuzzy_enabled = active;
				gtk_widget_set_sensitive(m_fuzzy_threshold, active);
			});

		gtk_box_pack_start(row, gtk_separator_new(GTK_ORIENTATION_VERTICAL), false, false, 4);

		GtkWidget* lbl_errors = gtk_label_new_with_mnemonic(_("Max _errors (0=auto):"));
		gtk_widget_set_halign(lbl_errors, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_errors, GTK_ALIGN_CENTER);
		gtk_box_pack_start(row, lbl_errors, false, false, 0);

		m_fuzzy_threshold = gtk_spin_button_new_with_range(0, 2, 1);
		gtk_widget_set_halign(m_fuzzy_threshold, GTK_ALIGN_START);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_fuzzy_threshold),
			static_cast<int>(m_settings->fuzzy_threshold));
		gtk_widget_set_sensitive(m_fuzzy_threshold,
			static_cast<bool>(m_settings->fuzzy_enabled));
		gtk_box_pack_start(row, m_fuzzy_threshold, false, false, 0);
		gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_errors), m_fuzzy_threshold);

		connect(m_fuzzy_threshold, "value-changed",
			[this](GtkSpinButton* btn)
			{
				m_settings->fuzzy_threshold = gtk_spin_button_get_value_as_int(btn);
			});

		gtk_box_pack_start(page,
			make_info_frame(_("Fuzzy Search"), GTK_WIDGET(row),
				_("Finds apps even when you mistype a word.\n"
				  "Example: \"firfox\" still finds Firefox.\n"
				  "Max errors 0 = automatic (1 for short queries, 2 for longer ones).")),
			false, false, 0);
	}

	{
		GtkGrid* grid = GTK_GRID(gtk_grid_new());
		gtk_grid_set_column_spacing(grid, 12);
		gtk_grid_set_row_spacing(grid, 6);

		GtkBox* boost_row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));

		GtkWidget* lbl_boost = gtk_label_new_with_mnemonic(_("_Boost favorites:"));
		gtk_widget_set_halign(lbl_boost, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_boost, GTK_ALIGN_CENTER);
		gtk_box_pack_start(boost_row, lbl_boost, false, false, 0);

		m_favorites_boost_enabled = gtk_switch_new();
		gtk_widget_set_halign(m_favorites_boost_enabled, GTK_ALIGN_START);
		gtk_widget_set_valign(m_favorites_boost_enabled, GTK_ALIGN_CENTER);
		gtk_switch_set_active(GTK_SWITCH(m_favorites_boost_enabled),
			static_cast<bool>(m_settings->favorites_boost_enabled));
		gtk_box_pack_start(boost_row, m_favorites_boost_enabled, false, false, 0);
		gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_boost), m_favorites_boost_enabled);

		connect(m_favorites_boost_enabled, "notify::active",
			[this](GObject* obj, GParamSpec*)
			{
				const bool active = gtk_switch_get_active(GTK_SWITCH(obj));
				m_settings->favorites_boost_enabled = active;
				gtk_widget_set_sensitive(m_favorites_boost_level, active);
			});

		gtk_box_pack_start(boost_row, gtk_separator_new(GTK_ORIENTATION_VERTICAL), false, false, 4);

		GtkWidget* lbl_level = gtk_label_new_with_mnemonic(_("Boost _level:"));
		gtk_widget_set_halign(lbl_level, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_level, GTK_ALIGN_CENTER);
		gtk_box_pack_start(boost_row, lbl_level, false, false, 0);

		m_favorites_boost_level = gtk_combo_box_text_new();
		gtk_widget_set_halign(m_favorites_boost_level, GTK_ALIGN_START);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_favorites_boost_level), _("Low"));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_favorites_boost_level), _("Medium"));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_favorites_boost_level), _("High"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_favorites_boost_level),
			static_cast<int>(m_settings->favorites_boost_level) - 1);
		gtk_widget_set_sensitive(m_favorites_boost_level,
			static_cast<bool>(m_settings->favorites_boost_enabled));
		gtk_box_pack_start(boost_row, m_favorites_boost_level, false, false, 0);
		gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_level), m_favorites_boost_level);

		connect(m_favorites_boost_level, "changed",
			[this](GtkComboBox* combo)
			{
				m_settings->favorites_boost_level = gtk_combo_box_get_active(combo) + 1;
			});

		gtk_grid_attach(grid, GTK_WIDGET(boost_row), 0, 0, 2, 1);

		GtkWidget* lbl_recency = gtk_label_new_with_mnemonic(_("Recency _weight (%):"));
		gtk_widget_set_halign(lbl_recency, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_recency, GTK_ALIGN_CENTER);
		gtk_grid_attach(grid, lbl_recency, 0, 1, 1, 1);

		m_frecency_alpha = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
		gtk_widget_set_hexpand(m_frecency_alpha, true);
		gtk_scale_set_value_pos(GTK_SCALE(m_frecency_alpha), GTK_POS_RIGHT);
		gtk_range_set_value(GTK_RANGE(m_frecency_alpha),
			static_cast<int>(m_settings->frecency_alpha));
		gtk_grid_attach(grid, m_frecency_alpha, 1, 1, 1, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_recency), m_frecency_alpha);

		connect(m_frecency_alpha, "value-changed",
			[this](GtkRange* range)
			{
				m_settings->frecency_alpha = static_cast<int>(gtk_range_get_value(range));
			});

		gtk_box_pack_start(page,
			make_info_frame(_("Usage Boost"), GTK_WIDGET(grid),
				_("Promotes apps you use frequently or marked as favorites.\n"
				  "Recency weight controls the balance between how recently vs.\n"
				  "how often you launched an app.")),
			false, false, 0);
	}

	// =========================================================================
	// 3. Aliases section
	// =========================================================================
	{
		enum { ALIAS_COL_NAME, ALIAS_COL_TERMS, ALIAS_COL_ID, ALIAS_N_COLS };

		m_aliases_model = gtk_list_store_new(ALIAS_N_COLS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

		if (m_plugin->get_window())
		{
			const auto launchers = m_plugin->get_window()->get_applications()->find_all();
			for (const auto* launcher : launchers)
			{
				const char* id = launcher->get_desktop_id();
				const auto& terms = m_settings->get_aliases(id);
				if (terms.empty())
					continue;
				std::string joined;
				for (size_t i = 0; i < terms.size(); ++i)
				{
					if (i) joined += ", ";
					joined += terms[i];
				}
				gtk_list_store_insert_with_values(m_aliases_model, nullptr, G_MAXINT,
					ALIAS_COL_NAME, launcher->get_display_name(),
					ALIAS_COL_TERMS, joined.c_str(),
					ALIAS_COL_ID, id,
					-1);
			}
		}

		m_aliases_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_aliases_model)));

		GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes(
			_("Application"), renderer, "text", ALIAS_COL_NAME, nullptr);
		gtk_tree_view_column_set_expand(col, true);
		gtk_tree_view_append_column(m_aliases_view, col);

		renderer = gtk_cell_renderer_text_new();
		g_object_set(renderer, "editable", TRUE, nullptr);
		col = gtk_tree_view_column_new_with_attributes(
			_("Aliases (comma-separated)"), renderer, "text", ALIAS_COL_TERMS, nullptr);
		gtk_tree_view_column_set_expand(col, true);
		gtk_tree_view_append_column(m_aliases_view, col);

		connect(renderer, "edited",
			[this](GtkCellRendererText*, const gchar* path_str, const gchar* new_text)
			{
				enum { ALIAS_COL_NAME, ALIAS_COL_TERMS, ALIAS_COL_ID, ALIAS_N_COLS };
				GtkTreeIter iter;
				if (!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(m_aliases_model),
						&iter, path_str))
					return;
				gtk_list_store_set(m_aliases_model, &iter, ALIAS_COL_TERMS, new_text, -1);
				gchar* id_val = nullptr;
				gtk_tree_model_get(GTK_TREE_MODEL(m_aliases_model), &iter,
					ALIAS_COL_ID, &id_val, -1);
				if (id_val)
				{
					std::vector<std::string> terms;
					gchar** parts = g_strsplit(new_text, ",", -1);
					for (int i = 0; parts[i]; ++i)
					{
						gchar* stripped = g_strstrip(parts[i]);
						if (*stripped)
							terms.emplace_back(stripped);
					}
					g_strfreev(parts);
					m_settings->set_aliases(std::string(id_val), terms);
					g_free(id_val);
				}
			});

		GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
		gtk_widget_set_size_request(scrolled, -1, 120);
		gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(m_aliases_view));

		m_alias_add = gtk_button_new_with_mnemonic(_("_Add"));
		m_alias_remove = gtk_button_new_with_mnemonic(_("_Remove"));

		connect(m_alias_add, "clicked",
			[this](GtkButton*)
			{
				if (!m_plugin->get_window())
					return;
				enum { ALIAS_COL_NAME, ALIAS_COL_TERMS, ALIAS_COL_ID, ALIAS_N_COLS };
				GtkWidget* dialog = gtk_dialog_new_with_buttons(
					_("Choose Application"),
					GTK_WINDOW(m_window),
					GTK_DIALOG_MODAL,
					_("_Cancel"), GTK_RESPONSE_CANCEL,
					_("_Add"),    GTK_RESPONSE_ACCEPT,
					nullptr);
				GtkListStore* app_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
				const auto launchers = m_plugin->get_window()->get_applications()->find_all();
				for (const auto* launcher : launchers)
				{
					gtk_list_store_insert_with_values(app_store, nullptr, G_MAXINT,
						0, launcher->get_display_name(),
						1, launcher->get_desktop_id(),
						-1);
				}
				GtkWidget* app_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app_store));
				g_object_unref(app_store);
				GtkCellRenderer* rend = gtk_cell_renderer_text_new();
				gtk_tree_view_append_column(GTK_TREE_VIEW(app_view),
					gtk_tree_view_column_new_with_attributes(
						_("Application"), rend, "text", 0, nullptr));
				GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
				gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
				gtk_widget_set_size_request(sw, 300, 240);
				gtk_container_add(GTK_CONTAINER(sw), app_view);
				gtk_box_pack_start(
					GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
					sw, true, true, 6);
				gtk_widget_show_all(dialog);
				if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
				{
					GtkTreeSelection* sel = gtk_tree_view_get_selection(
						GTK_TREE_VIEW(app_view));
					GtkTreeIter it;
					GtkTreeModel* mdl;
					if (gtk_tree_selection_get_selected(sel, &mdl, &it))
					{
						gchar* name = nullptr;
						gchar* id   = nullptr;
						gtk_tree_model_get(mdl, &it, 0, &name, 1, &id, -1);
						gtk_list_store_insert_with_values(m_aliases_model,
							nullptr, G_MAXINT,
							ALIAS_COL_NAME, name,
							ALIAS_COL_TERMS, "",
							ALIAS_COL_ID, id,
							-1);
						g_free(name);
						g_free(id);
					}
				}
				gtk_widget_destroy(dialog);
				gtk_widget_set_sensitive(m_alias_remove, true);
			});

		connect(m_alias_remove, "clicked",
			[this](GtkButton*)
			{
				enum { ALIAS_COL_NAME, ALIAS_COL_TERMS, ALIAS_COL_ID, ALIAS_N_COLS };
				GtkTreeSelection* sel = gtk_tree_view_get_selection(m_aliases_view);
				GtkTreeIter iter;
				GtkTreeModel* mdl;
				if (!gtk_tree_selection_get_selected(sel, &mdl, &iter))
					return;
				gchar* id_val = nullptr;
				gtk_tree_model_get(mdl, &iter, ALIAS_COL_ID, &id_val, -1);
				if (id_val)
				{
					m_settings->set_aliases(std::string(id_val), {});
					g_free(id_val);
				}
				gtk_list_store_remove(m_aliases_model, &iter);
				const bool has_rows = gtk_tree_model_iter_n_children(
					GTK_TREE_MODEL(m_aliases_model), nullptr) > 0;
				gtk_widget_set_sensitive(m_alias_remove, has_rows);
			});

		const bool has_rows = gtk_tree_model_iter_n_children(
			GTK_TREE_MODEL(m_aliases_model), nullptr) > 0;
		gtk_widget_set_sensitive(m_alias_remove, has_rows);

		GtkWidget* btn_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_button_box_set_layout(GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_START);
		gtk_box_set_spacing(GTK_BOX(btn_box), 6);
		gtk_box_pack_start(GTK_BOX(btn_box), m_alias_add, false, false, 0);
		gtk_box_pack_start(GTK_BOX(btn_box), m_alias_remove, false, false, 0);

		GtkBox* alias_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
		gtk_box_pack_start(alias_vbox, scrolled, true, true, 0);
		gtk_box_pack_start(alias_vbox, btn_box, false, false, 0);

		gtk_box_pack_start(page,
			make_aligned_frame(_("Aliases"), GTK_WIDGET(alias_vbox)),
			false, false, 0);
	}

	// =========================================================================
	// 4. Search actions section — list with add/remove/edit (modal); the modal
	// replaces the legacy inline detail panel per data-model E-5 / FR-031.
	// =========================================================================
	{
		GtkGrid* actions_grid = GTK_GRID(gtk_grid_new());
		gtk_grid_set_column_spacing(actions_grid, 6);
		gtk_grid_set_row_spacing(actions_grid, 6);

		m_actions_model = gtk_list_store_new(N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
		for (auto action : m_settings->search_actions)
		{
			gtk_list_store_insert_with_values(m_actions_model,
				nullptr, G_MAXINT,
				COLUMN_NAME, action->get_name(),
				COLUMN_PATTERN, action->get_pattern(),
				COLUMN_ACTION, action,
				-1);
		}

		m_actions_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_actions_model)));

		GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Name"),
			renderer, "text", COLUMN_NAME, nullptr);
		gtk_tree_view_column_set_expand(column, true);
		gtk_tree_view_append_column(m_actions_view, column);

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(_("Pattern"),
			renderer, "text", COLUMN_PATTERN, nullptr);
		gtk_tree_view_column_set_expand(column, true);
		gtk_tree_view_append_column(m_actions_view, column);

		GtkTreeSelection* selection = gtk_tree_view_get_selection(m_actions_view);
		gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

		GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
		gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(m_actions_view));
		gtk_widget_set_hexpand(sw, true);
		gtk_widget_set_vexpand(sw, true);
		gtk_widget_set_size_request(sw, -1, 160);
		gtk_grid_attach(actions_grid, sw, 0, 0, 1, 1);

		// Add / Remove / Edit buttons
		m_action_add = gtk_button_new();
		gtk_widget_set_tooltip_text(m_action_add, _("Add action"));
		gtk_container_add(GTK_CONTAINER(m_action_add),
			gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON));
		connect(m_action_add, "clicked",
			[this](GtkButton*) { add_action(); });

		m_action_remove = gtk_button_new();
		gtk_widget_set_tooltip_text(m_action_remove, _("Remove selected action"));
		gtk_container_add(GTK_CONTAINER(m_action_remove),
			gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_BUTTON));
		connect(m_action_remove, "clicked",
			[this](GtkButton*) { remove_action(); });

		// Edit button — the "hamburger" of T024; opens the per-action modal
		// (T025). open-menu-symbolic matches the GTK convention for hamburger.
		GtkWidget* edit_btn = gtk_button_new();
		gtk_widget_set_tooltip_text(edit_btn, _("Edit selected action…"));
		gtk_container_add(GTK_CONTAINER(edit_btn),
			gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
		connect(edit_btn, "clicked",
			[this](GtkButton*)
			{
				SearchAction* action = get_selected_action();
				if (!action)
					return;
				edit_search_action_modal(action);
			});

		// Double-clicking a row should also open the modal (UX shortcut).
		connect(m_actions_view, "row-activated",
			[this](GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*)
			{
				SearchAction* action = get_selected_action();
				if (action)
					edit_search_action_modal(action);
			});

		GtkBox* actions_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
		gtk_widget_set_halign(GTK_WIDGET(actions_box), GTK_ALIGN_START);
		gtk_box_pack_start(actions_box, m_action_add, false, false, 0);
		gtk_box_pack_start(actions_box, m_action_remove, false, false, 0);
		gtk_box_pack_start(actions_box, edit_btn, false, false, 0);
		gtk_grid_attach(actions_grid, GTK_WIDGET(actions_box), 1, 0, 1, 1);

		// The legacy inline-detail panel is replaced by the modal; keep the
		// detail entry widgets nullptr so callers that null-guard them stay
		// safe and the destructor's unused-widget paths remain valid.
		m_action_name = nullptr;
		m_action_pattern = nullptr;
		m_action_command = nullptr;
		m_action_regex = nullptr;

		const bool has_rows = !m_settings->search_actions.empty();
		gtk_widget_set_sensitive(m_action_remove, has_rows);
		gtk_widget_set_sensitive(edit_btn, has_rows);
		if (has_rows)
		{
			GtkTreePath* path = gtk_tree_path_new_first();
			gtk_tree_view_set_cursor(m_actions_view, path, nullptr, false);
			gtk_tree_path_free(path);
		}

		// Re-evaluate Edit-button sensitivity whenever the selection toggles.
		connect(selection, "changed",
			[edit_btn](GtkTreeSelection* sel)
			{
				GtkTreeIter iter;
				GtkTreeModel* mdl;
				const bool sel_ok = gtk_tree_selection_get_selected(sel, &mdl, &iter);
				gtk_widget_set_sensitive(edit_btn, sel_ok);
			});

		gtk_box_pack_start(page,
			make_aligned_frame(_("Search actions"), GTK_WIDGET(actions_grid)),
			true, true, 0);
	}

	return wrap_in_scrolled(GTK_WIDGET(page));
}

/* init_app_grid_tab:
 *
 * Builds the App Grid tab in the 003-properties-refactor 5-tab dictionary.
 * Sections (top-to-bottom, FR-040):
 *   1. View              — Show applications as (Icons / List / Tree) icon-radios.
 *   2. Layout            — Grid density, application icon size, show flags.
 *   3. Opacity           — App box opacity (enable-when-docked).
 *
 * Sub-enable rules per view-mode (FR-041 / FR-042): grid-density and
 * launcher-icon-size are sensitive only when view-mode == icons;
 * launcher-show-description only when view-mode == list.
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_app_grid_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkSizeGroup* label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* control_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	// =========================================================================
	// 1. View section — three exclusive icon-buttons (FR-040).
	// =========================================================================
	GtkGrid* view_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(view_table, 12);
	gtk_grid_set_row_spacing(view_table, 6);

	GtkWidget* view_frame = make_aligned_frame(_("View"), GTK_WIDGET(view_table));
	gtk_box_pack_start(page, view_frame, false, false, 0);

	GtkButtonBox* view_box = GTK_BUTTON_BOX(gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_widget_set_halign(GTK_WIDGET(view_box), GTK_ALIGN_CENTER);
	gtk_button_box_set_layout(view_box, GTK_BUTTONBOX_EXPAND);
	gtk_grid_attach(view_table, GTK_WIDGET(view_box), 0, 0, 2, 1);
	gtk_widget_set_margin_bottom(GTK_WIDGET(view_box), 6);

	m_show_as_icons = gtk_radio_button_new_with_mnemonic(nullptr, _("Show as _icons"));
	{
		const gchar* icons[] = { "view-list-icons", "view-grid", nullptr };
		GIcon* gicon = g_themed_icon_new_from_names(const_cast<gchar**>(icons), -1);
		gtk_button_set_image(GTK_BUTTON(m_show_as_icons), gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DND));
		g_object_unref(gicon);
	}
	gtk_button_set_image_position(GTK_BUTTON(m_show_as_icons), GTK_POS_TOP);
	gtk_button_set_always_show_image(GTK_BUTTON(m_show_as_icons), true);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_show_as_icons), false);
	gtk_box_pack_start(GTK_BOX(view_box), m_show_as_icons, true, true, 0);

	m_show_as_list = gtk_radio_button_new_with_mnemonic_from_widget(
		GTK_RADIO_BUTTON(m_show_as_icons), _("Show as lis_t"));
	{
		const gchar* icons[] = { "view-list-compact", "view-list-details", "view-list", nullptr };
		GIcon* gicon = g_themed_icon_new_from_names(const_cast<gchar**>(icons), -1);
		gtk_button_set_image(GTK_BUTTON(m_show_as_list), gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DND));
		g_object_unref(gicon);
	}
	gtk_button_set_image_position(GTK_BUTTON(m_show_as_list), GTK_POS_TOP);
	gtk_button_set_always_show_image(GTK_BUTTON(m_show_as_list), true);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_show_as_list), false);
	gtk_box_pack_start(GTK_BOX(view_box), m_show_as_list, true, true, 0);

	m_show_as_tree = gtk_radio_button_new_with_mnemonic_from_widget(
		GTK_RADIO_BUTTON(m_show_as_list), _("Show as t_ree"));
	{
		const gchar* icons[] = { "view-list-tree", "view-list-details", "pan-end", nullptr };
		GIcon* gicon = g_themed_icon_new_from_names(const_cast<gchar**>(icons), -1);
		gtk_button_set_image(GTK_BUTTON(m_show_as_tree), gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DND));
		g_object_unref(gicon);
	}
	gtk_button_set_image_position(GTK_BUTTON(m_show_as_tree), GTK_POS_TOP);
	gtk_button_set_always_show_image(GTK_BUTTON(m_show_as_tree), true);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_show_as_tree), false);
	gtk_box_pack_start(GTK_BOX(view_box), m_show_as_tree, true, true, 0);

	if (m_settings->view_mode == Settings::ViewAsIcons)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_icons), true);
	else if (m_settings->view_mode == Settings::ViewAsTree)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_tree), true);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_list), true);

	// =========================================================================
	// 2. Layout section — grid density, icon size, show flags.
	// =========================================================================
	GtkGrid* layout_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(layout_table, 12);
	gtk_grid_set_row_spacing(layout_table, 6);

	GtkWidget* layout_frame = make_aligned_frame(_("Layout"), GTK_WIDGET(layout_table));
	gtk_box_pack_start(page, layout_frame, false, false, 0);

	int layout_row = 0;

	// Grid density (icons-only sub-enable)
	GtkWidget* density_label = gtk_label_new_with_mnemonic(_("Grid _density:"));
	gtk_widget_set_halign(density_label, GTK_ALIGN_START);
	gtk_grid_attach(layout_table, density_label, 0, layout_row, 1, 1);

	m_grid_density_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "low", _("Low"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "medium", _("Medium"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "high", _("High"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_grid_density_combo),
		static_cast<const gchar*>(m_settings->grid_density));
	gtk_widget_set_halign(m_grid_density_combo, GTK_ALIGN_START);
	gtk_grid_attach(layout_table, m_grid_density_combo, 1, layout_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(density_label), m_grid_density_combo);
	gtk_size_group_add_widget(label_size_group, density_label);
	gtk_size_group_add_widget(control_size_group, m_grid_density_combo);
	++layout_row;

	connect(m_grid_density_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			m_settings->grid_density = val;
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Application icon size (icons-only sub-enable)
	GtkWidget* icon_size_label = gtk_label_new_with_mnemonic(_("Application icon si_ze:"));
	gtk_widget_set_halign(icon_size_label, GTK_ALIGN_START);
	gtk_grid_attach(layout_table, icon_size_label, 0, layout_row, 1, 1);

	m_item_icon_size = gtk_combo_box_text_new();
	gtk_widget_set_halign(m_item_icon_size, GTK_ALIGN_START);
	const auto icon_sizes = IconSize::get_strings();
	for (const auto& icon_size : icon_sizes)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_item_icon_size), icon_size.c_str());
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_item_icon_size), m_settings->launcher_icon_size + 1);
	gtk_grid_attach(layout_table, m_item_icon_size, 1, layout_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(icon_size_label), m_item_icon_size);
	gtk_size_group_add_widget(label_size_group, icon_size_label);
	gtk_size_group_add_widget(control_size_group, m_item_icon_size);
	++layout_row;

	connect(m_item_icon_size, "changed",
		[this](GtkComboBox* combo)
		{
			m_settings->launcher_icon_size = gtk_combo_box_get_active(combo) - 1;
		});

	// NOTE: launcher_show_name stores "show the real (non-generic) name"; the
	// checkbox is therefore inverted — checked means "Show generic" (false).
	m_show_generic_names = gtk_check_button_new_with_mnemonic(_("Show generic application _names"));
	gtk_grid_attach(layout_table, m_show_generic_names, 0, layout_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_generic_names), !m_settings->launcher_show_name);
	++layout_row;

	connect(m_show_generic_names, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_name = !gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	m_show_tooltips = gtk_check_button_new_with_mnemonic(_("Show application too_ltips"));
	gtk_grid_attach(layout_table, m_show_tooltips, 0, layout_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_tooltips), m_settings->launcher_show_tooltip);
	++layout_row;

	connect(m_show_tooltips, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_tooltip = gtk_toggle_button_get_active(button);
		});

	// Show descriptions — list-only sub-enable.
	m_show_descriptions = gtk_check_button_new_with_mnemonic(_("Show application _descriptions"));
	gtk_grid_attach(layout_table, m_show_descriptions, 0, layout_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_descriptions), m_settings->launcher_show_description);
	++layout_row;

	connect(m_show_descriptions, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_description = gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	// Apply the view-mode sub-enables now and on every toggle.
	auto apply_view_mode_sub_enables = [this, density_label, icon_size_label]()
	{
		const bool is_icons = (m_settings->view_mode == Settings::ViewAsIcons);
		const bool is_list  = (m_settings->view_mode == Settings::ViewAsList);
		gtk_widget_set_sensitive(m_grid_density_combo, is_icons);
		gtk_widget_set_sensitive(density_label, is_icons);
		gtk_widget_set_sensitive(m_item_icon_size, is_icons);
		gtk_widget_set_sensitive(icon_size_label, is_icons);
		gtk_widget_set_sensitive(m_show_descriptions, is_list);
	};
	apply_view_mode_sub_enables();

	connect(m_show_as_icons, "toggled",
		[this, apply_view_mode_sub_enables](GtkToggleButton* button)
		{
			if (!gtk_toggle_button_get_active(button))
				return;
			m_settings->view_mode = Settings::ViewAsIcons;
			apply_view_mode_sub_enables();
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	connect(m_show_as_list, "toggled",
		[this, apply_view_mode_sub_enables](GtkToggleButton* button)
		{
			if (!gtk_toggle_button_get_active(button))
				return;
			m_settings->view_mode = Settings::ViewAsList;
			apply_view_mode_sub_enables();
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	connect(m_show_as_tree, "toggled",
		[this, apply_view_mode_sub_enables](GtkToggleButton* button)
		{
			if (!gtk_toggle_button_get_active(button))
				return;
			m_settings->view_mode = Settings::ViewAsTree;
			apply_view_mode_sub_enables();
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// =========================================================================
	// 3. Opacity section — App box opacity (FR-044, enable-when-docked).
	// =========================================================================
	GtkGrid* opacity_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(opacity_table, 12);
	gtk_grid_set_row_spacing(opacity_table, 6);

	GtkWidget* opacity_frame = make_aligned_frame(_("Opacity"), GTK_WIDGET(opacity_table));
	gtk_box_pack_start(page, opacity_frame, false, false, 0);

	GtkWidget* apps_op_label = gtk_label_new_with_mnemonic(_("App _box opacity:"));
	gtk_widget_set_halign(apps_op_label, GTK_ALIGN_START);
	gtk_grid_attach(opacity_table, apps_op_label, 0, 0, 1, 1);

	m_apps_opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_widget_set_hexpand(m_apps_opacity, true);
	gtk_scale_set_value_pos(GTK_SCALE(m_apps_opacity), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(m_apps_opacity), m_settings->apps_opacity);
	gtk_grid_attach(opacity_table, m_apps_opacity, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(apps_op_label), m_apps_opacity);
	gtk_size_group_add_widget(label_size_group, apps_op_label);

	connect(m_apps_opacity, "value-changed",
		[this](GtkRange* range)
		{
			m_settings->apps_opacity = static_cast<int>(gtk_range_get_value(range));
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Layout-mode-sensitive: enable only in docked.
	m_layout_enable_when_docked.push_back(m_apps_opacity);
	m_layout_enable_when_docked.push_back(apps_op_label);

	return wrap_in_scrolled(GTK_WIDGET(page));
}

/* init_sidebar_tab:
 *
 * Builds the Sidebar tab in the 003-properties-refactor 5-tab dictionary.
 * Sections (top-to-bottom, FR-050):
 *   1. Visuals      — show category names, icon size, sidebar opacity.
 *   2. Position     — sidebar position (left/right/hidden).
 *   3. Behaviour    — hover-switch, sort categories, default category.
 *   4. Recently used — max items, include favorites.
 *
 * Sub-enable rule for category-show-name: greyed when sidebar-position
 * is not one of {left, right} (data-model E-6 §SidebarLeftRight).
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_sidebar_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkSizeGroup* label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* control_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	// =========================================================================
	// 1. Visuals section
	// =========================================================================
	GtkGrid* visuals_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(visuals_table, 12);
	gtk_grid_set_row_spacing(visuals_table, 6);

	GtkWidget* visuals_frame = make_aligned_frame(_("Visuals"), GTK_WIDGET(visuals_table));
	gtk_box_pack_start(page, visuals_frame, false, false, 0);

	int v_row = 0;

	m_show_category_names = gtk_check_button_new_with_mnemonic(_("Show cate_gory names"));
	gtk_grid_attach(visuals_table, m_show_category_names, 0, v_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_category_names),
		m_settings->category_show_name);
	++v_row;

	connect(m_show_category_names, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->category_show_name = gtk_toggle_button_get_active(button);
		});

	// Category icon size
	GtkWidget* cat_size_label = gtk_label_new_with_mnemonic(_("Categ_ory icon size:"));
	gtk_widget_set_halign(cat_size_label, GTK_ALIGN_START);
	gtk_grid_attach(visuals_table, cat_size_label, 0, v_row, 1, 1);

	m_category_icon_size = gtk_combo_box_text_new();
	gtk_widget_set_halign(m_category_icon_size, GTK_ALIGN_START);
	const auto cat_icon_sizes = IconSize::get_strings();
	for (const auto& s : cat_icon_sizes)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_category_icon_size), s.c_str());
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_category_icon_size),
		m_settings->category_icon_size + 1);
	gtk_grid_attach(visuals_table, m_category_icon_size, 1, v_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(cat_size_label), m_category_icon_size);
	gtk_size_group_add_widget(label_size_group, cat_size_label);
	gtk_size_group_add_widget(control_size_group, m_category_icon_size);
	++v_row;

	connect(m_category_icon_size, "changed",
		[this](GtkComboBox* combo)
		{
			m_settings->category_icon_size = gtk_combo_box_get_active(combo) - 1;
		});

	// Sidebar opacity (renamed from "Category opacity"; enable-when-docked).
	GtkWidget* side_op_label = gtk_label_new_with_mnemonic(_("_Sidebar opacity:"));
	gtk_widget_set_halign(side_op_label, GTK_ALIGN_START);
	gtk_grid_attach(visuals_table, side_op_label, 0, v_row, 1, 1);

	m_categories_opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_widget_set_hexpand(m_categories_opacity, true);
	gtk_scale_set_value_pos(GTK_SCALE(m_categories_opacity), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(m_categories_opacity), m_settings->categories_opacity);
	gtk_grid_attach(visuals_table, m_categories_opacity, 1, v_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(side_op_label), m_categories_opacity);
	gtk_size_group_add_widget(label_size_group, side_op_label);
	++v_row;

	connect(m_categories_opacity, "value-changed",
		[this](GtkRange* range)
		{
			m_settings->categories_opacity = static_cast<int>(gtk_range_get_value(range));
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	m_layout_enable_when_docked.push_back(m_categories_opacity);
	m_layout_enable_when_docked.push_back(side_op_label);

	// =========================================================================
	// 2. Position section
	// =========================================================================
	GtkGrid* pos_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(pos_table, 12);
	gtk_grid_set_row_spacing(pos_table, 6);

	GtkWidget* pos_frame = make_aligned_frame(_("Position"), GTK_WIDGET(pos_table));
	gtk_box_pack_start(page, pos_frame, false, false, 0);

	GtkWidget* side_pos_label = gtk_label_new_with_mnemonic(_("Sidebar _position:"));
	gtk_widget_set_halign(side_pos_label, GTK_ALIGN_START);
	gtk_grid_attach(pos_table, side_pos_label, 0, 0, 1, 1);

	m_sidebar_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "left", _("Left"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "right", _("Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "hidden", _("Hidden"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_sidebar_position_combo),
		static_cast<const gchar*>(m_settings->sidebar_position));
	gtk_widget_set_halign(m_sidebar_position_combo, GTK_ALIGN_START);
	gtk_grid_attach(pos_table, m_sidebar_position_combo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(side_pos_label), m_sidebar_position_combo);
	gtk_size_group_add_widget(label_size_group, side_pos_label);
	gtk_size_group_add_widget(control_size_group, m_sidebar_position_combo);

	auto apply_sidebar_sub_enable = [this]()
	{
		const gchar* p = static_cast<const gchar*>(m_settings->sidebar_position);
		const bool lr = p && (g_strcmp0(p, "left") == 0 || g_strcmp0(p, "right") == 0);
		gtk_widget_set_sensitive(m_show_category_names, lr);
	};
	apply_sidebar_sub_enable();

	connect(m_sidebar_position_combo, "changed",
		[this, apply_sidebar_sub_enable](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			m_settings->sidebar_position = val;
			apply_sidebar_sub_enable();
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// =========================================================================
	// 3. Behaviour section
	// =========================================================================
	GtkBox* behavior_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	GtkWidget* behavior_frame = make_aligned_frame(_("Behaviour"), GTK_WIDGET(behavior_vbox));
	gtk_box_pack_start(page, behavior_frame, false, false, 0);

	m_hover_switch_category = gtk_check_button_new_with_mnemonic(_("Switch categories by _hovering"));
	gtk_box_pack_start(behavior_vbox, m_hover_switch_category, false, false, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_hover_switch_category),
		m_settings->category_hover_activate);

	connect(m_hover_switch_category, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->category_hover_activate = gtk_toggle_button_get_active(button);
		});

	m_sort_categories = gtk_check_button_new_with_mnemonic(_("Sort ca_tegories"));
	gtk_box_pack_start(behavior_vbox, m_sort_categories, false, false, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_sort_categories), m_settings->sort_categories);

	connect(m_sort_categories, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->sort_categories = gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	// Default category — three radio buttons.
	GtkBox* default_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	GtkWidget* default_frame = make_aligned_frame(_("Default category"), GTK_WIDGET(default_vbox));
	gtk_box_pack_start(page, default_frame, false, false, 0);

	m_display_favorites = gtk_radio_button_new_with_mnemonic(nullptr, _("Favorites"));
	gtk_box_pack_start(default_vbox, m_display_favorites, false, false, 0);

	m_display_recent = gtk_radio_button_new_with_mnemonic_from_widget(
		GTK_RADIO_BUTTON(m_display_favorites), _("Recently Used"));
	gtk_box_pack_start(default_vbox, m_display_recent, false, false, 0);
	gtk_widget_set_sensitive(m_display_recent, m_settings->recent_items_max);

	m_display_applications = gtk_radio_button_new_with_mnemonic_from_widget(
		GTK_RADIO_BUTTON(m_display_recent), _("All Applications"));
	gtk_box_pack_start(default_vbox, m_display_applications, false, false, 0);

	switch (m_settings->default_category)
	{
	case Settings::CategoryRecent:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_recent), true);
		break;
	case Settings::CategoryAll:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_applications), true);
		break;
	default:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_favorites), true);
		break;
	}

	connect(m_display_favorites, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
				m_settings->default_category = Settings::CategoryFavorites;
		});

	connect(m_display_recent, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
				m_settings->default_category = Settings::CategoryRecent;
		});

	connect(m_display_applications, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
				m_settings->default_category = Settings::CategoryAll;
		});

	// =========================================================================
	// 4. Recently used section
	// =========================================================================
	GtkGrid* recent_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(recent_table, 12);
	gtk_grid_set_row_spacing(recent_table, 6);

	GtkWidget* recent_frame = make_aligned_frame(_("Recently used"), GTK_WIDGET(recent_table));
	gtk_box_pack_start(page, recent_frame, false, false, 0);

	GtkWidget* max_label = gtk_label_new_with_mnemonic(_("Maximum _items:"));
	gtk_widget_set_halign(max_label, GTK_ALIGN_START);
	gtk_grid_attach(recent_table, max_label, 0, 0, 1, 1);

	m_recent_items_max = gtk_spin_button_new_with_range(0, 100, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_recent_items_max), m_settings->recent_items_max);
	gtk_grid_attach(recent_table, m_recent_items_max, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(max_label), m_recent_items_max);
	gtk_size_group_add_widget(label_size_group, max_label);
	gtk_size_group_add_widget(control_size_group, m_recent_items_max);

	connect(m_recent_items_max, "value-changed",
		[this](GtkSpinButton* button)
		{
			m_settings->recent_items_max = gtk_spin_button_get_value_as_int(button);
			const bool active = m_settings->recent_items_max;
			gtk_widget_set_sensitive(m_display_recent, active);
			if (!active && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_display_recent)))
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_favorites), true);
		});

	m_remember_favorites = gtk_check_button_new_with_mnemonic(_("Include _favorites in \"Recent\""));
	gtk_grid_attach(recent_table, m_remember_favorites, 0, 1, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_remember_favorites),
		m_settings->favorites_in_recent);

	connect(m_remember_favorites, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->favorites_in_recent = gtk_toggle_button_get_active(button);
		});

	return wrap_in_scrolled(GTK_WIDGET(page));
}



//-----------------------------------------------------------------------------

/* init_places_tab:
 *
 * Builds the Places panel (milestone 005). Seven controls bound directly to
 * the /places-prefixed Xfconf-backed Settings members. Sensitivity is gated by
 * /places/enabled at the panel level and by /places/favourites-enabled for
 * the sync dropdown (FR-037, FR-038).
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_places_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	std::vector<GtkWidget*> places_dependents;

	// Enable section
	GtkGrid* enable_grid = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(enable_grid, 12);
	gtk_grid_set_row_spacing(enable_grid, 6);
	gtk_box_pack_start(page, make_aligned_frame(_("Places mode"), GTK_WIDGET(enable_grid)), false, false, 0);

	GtkWidget* enable_switch = gtk_switch_new();
	GtkWidget* enable_label = gtk_label_new_with_mnemonic(_("Enable _Places"));
	gtk_widget_set_halign(enable_label, GTK_ALIGN_START);
	gtk_widget_set_hexpand(enable_label, true);
	gtk_switch_set_active(GTK_SWITCH(enable_switch), m_settings->places_enabled);
	gtk_grid_attach(enable_grid, enable_label, 0, 0, 1, 1);
	gtk_grid_attach(enable_grid, enable_switch, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(enable_label), enable_switch);

	// Sections section — history/favourites toggles plus item caps.
	GtkGrid* sections_grid = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(sections_grid, 12);
	gtk_grid_set_row_spacing(sections_grid, 6);
	GtkWidget* sections_frame = make_aligned_frame(_("Sections"), GTK_WIDGET(sections_grid));
	gtk_box_pack_start(page, sections_frame, false, false, 0);
	places_dependents.push_back(sections_frame);

	int row = 0;
	GtkWidget* history_switch = gtk_switch_new();
	GtkWidget* history_label = gtk_label_new_with_mnemonic(_("Enable _History section"));
	gtk_widget_set_halign(history_label, GTK_ALIGN_START);
	gtk_widget_set_hexpand(history_label, true);
	gtk_switch_set_active(GTK_SWITCH(history_switch), m_settings->places_history_enabled);
	gtk_grid_attach(sections_grid, history_label, 0, row, 1, 1);
	gtk_grid_attach(sections_grid, history_switch, 1, row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(history_label), history_switch);
	++row;

	GtkWidget* fav_switch = gtk_switch_new();
	GtkWidget* fav_label = gtk_label_new_with_mnemonic(_("Enable _Favourites section"));
	gtk_widget_set_halign(fav_label, GTK_ALIGN_START);
	gtk_widget_set_hexpand(fav_label, true);
	gtk_switch_set_active(GTK_SWITCH(fav_switch), m_settings->places_favourites_enabled);
	gtk_grid_attach(sections_grid, fav_label, 0, row, 1, 1);
	gtk_grid_attach(sections_grid, fav_switch, 1, row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(fav_label), fav_switch);
	++row;

	GtkWidget* sync_label = gtk_label_new_with_mnemonic(_("Favourite _sync:"));
	gtk_widget_set_halign(sync_label, GTK_ALIGN_START);
	GtkWidget* sync_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sync_combo), "meowmenu", _("MeowMenu only"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sync_combo), "thunar",   _("Thunar bookmarks (read-only)"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(sync_combo),
			static_cast<const gchar*>(m_settings->places_favourite_sync));
	gtk_grid_attach(sections_grid, sync_label, 0, row, 1, 1);
	gtk_grid_attach(sections_grid, sync_combo, 1, row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(sync_label), sync_combo);
	++row;

	GtkWidget* max_label = gtk_label_new_with_mnemonic(_("Maximum places _items:"));
	gtk_widget_set_halign(max_label, GTK_ALIGN_START);
	GtkWidget* max_spin = gtk_spin_button_new_with_range(0, 30, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_spin), m_settings->places_max_items);
	gtk_grid_attach(sections_grid, max_label, 0, row, 1, 1);
	gtk_grid_attach(sections_grid, max_spin, 1, row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(max_label), max_spin);
	++row;

	// Behaviour section
	GtkGrid* behaviour_grid = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(behaviour_grid, 12);
	gtk_grid_set_row_spacing(behaviour_grid, 6);
	GtkWidget* behaviour_frame = make_aligned_frame(_("Behaviour"), GTK_WIDGET(behaviour_grid));
	gtk_box_pack_start(page, behaviour_frame, false, false, 0);
	places_dependents.push_back(behaviour_frame);

	GtkWidget* remember_check = gtk_check_button_new_with_mnemonic(_("_Remember last selected mode"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(remember_check),
			m_settings->places_remember_last_mode);
	gtk_grid_attach(behaviour_grid, remember_check, 0, 0, 2, 1);

	GtkWidget* meta_check = gtk_check_button_new_with_mnemonic(_("Show item _metadata"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check),
			m_settings->places_show_metadata);
	gtk_grid_attach(behaviour_grid, meta_check, 0, 1, 2, 1);

	// Sensitivity helpers (FR-037, FR-038).
	auto refresh_sensitivity = [=]()
	{
		const bool enabled = gtk_switch_get_active(GTK_SWITCH(enable_switch));
		for (GtkWidget* w : places_dependents)
			gtk_widget_set_sensitive(w, enabled);
		const bool fav_enabled = enabled && gtk_switch_get_active(GTK_SWITCH(fav_switch));
		gtk_widget_set_sensitive(sync_combo, fav_enabled);
		gtk_widget_set_sensitive(sync_label, fav_enabled);
	};
	refresh_sensitivity();

	// Signal wiring
	connect(enable_switch, "state-set",
		[this, refresh_sensitivity](GtkSwitch*, gboolean state) -> gboolean
		{
			m_settings->places_enabled = state;
			refresh_sensitivity();
			return FALSE; // let the switch update its visual state
		});
	connect(history_switch, "state-set",
		[this](GtkSwitch*, gboolean state) -> gboolean
		{
			m_settings->places_history_enabled = state;
			return FALSE;
		});
	connect(fav_switch, "state-set",
		[this, refresh_sensitivity](GtkSwitch*, gboolean state) -> gboolean
		{
			m_settings->places_favourites_enabled = state;
			refresh_sensitivity();
			return FALSE;
		});
	connect(sync_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val) m_settings->places_favourite_sync = val;
		});
	connect(max_spin, "value-changed",
		[this](GtkSpinButton* btn)
		{
			m_settings->places_max_items = gtk_spin_button_get_value_as_int(btn);
		});
	connect(remember_check, "toggled",
		[this](GtkToggleButton* btn)
		{
			m_settings->places_remember_last_mode = gtk_toggle_button_get_active(btn);
		});
	connect(meta_check, "toggled",
		[this](GtkToggleButton* btn)
		{
			m_settings->places_show_metadata = gtk_toggle_button_get_active(btn);
		});

	return wrap_in_scrolled(GTK_WIDGET(page));
}
