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

#include "config/xfce-helpers.h"
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
	// refresh_preset_combo (runtime implementation / behavior table).
	initialize_file_presets();

	// New 5-tab dictionary per data-model.md E-3. Each init_*_tab() already
	// returns its content wrapped by wrap_in_scrolled().
	auto add_page = [stack](GtkWidget* child, const char* id, const char* title)
	{
		gtk_stack_add_titled(stack, child, id, title);
	};

	add_page(init_general_tab(),       "general", _("General"));
	add_page(init_user_session_tab(),  "user",    _("Session"));
	add_page(init_search_bar_tab(),    "search",  _("Search Bar"));
	add_page(init_app_grid_tab(),      "app-grid", _("Results View"));
	add_page(init_sidebar_tab(),       "sidebar", _("Sidebar"));
	add_page(init_places_tab(),        "places",  _("Places"));
	add_page(init_extras_tab(),        "extras",  _("Extras"));

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
	// is in use (will be removed in runtime implementation). When the new modal-based tab owns
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
		// disappear with runtime implementation).
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
 * Is-regex (coverage analysis, behavior table). OK persists the values into the
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

/* response:
 * @response_id: GTK response emitted by the preferences dialog.
 *
 * Opens Help through the active Xfce helper and presents spawn failures using
 * the same visible error pattern as other launcher actions. Other responses
 * retain the existing settings-save and window-lifecycle behavior.
 */
void SettingsDialog::response(int response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
	{
		std::string command = build_help_command(
				current_xfce_dependency_regime(), PLUGIN_WEBSITE);
		GError* error = nullptr;
		bool result = g_spawn_command_line_async(command.c_str(), &error);

		if (G_UNLIKELY(!result))
		{
			xfce_dialog_show_error(GTK_WINDOW(m_window), error,
					_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
			g_clear_error(&error);
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

// Reserved synthetic id for the transient "Unsaved custom" row. Never a real
// preset: it is appended to the model only while the live settings diverge from
// the applied preset and removed again the moment they re-match.
static const char* const UNSAVED_PLACEHOLDER_ID = "__unsaved__";

/* remove_unsaved_row:
 * @model: the preset combo's backing store.
 *
 * Drops the synthetic "Unsaved custom" placeholder row if present. Safe to call
 * when no placeholder exists.
 */
static void remove_unsaved_row(GtkListStore* model)
{
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;
	do
	{
		gchar* id = nullptr;
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
			SettingsDialog::PRESET_COL_ID, &id, -1);
		const bool is_placeholder = id && strcmp(id, UNSAVED_PLACEHOLDER_ID) == 0;
		g_free(id);
		if (is_placeholder)
		{
			gtk_list_store_remove(model, &iter);
			return;
		}
	}
	while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
}

void SettingsDialog::refresh_customized_indicator()
{
	// Continuous, reversible divergence recompute (supported behavior). Every governed-widget
	// change handler across all tabs calls this, so the active-preset field always
	// reflects whether the live settings still match the applied preset: it reads
	// "Unsaved custom" (italic) while diverged and snaps back to the applied
	// preset's own row the instant the values match again. The field is never
	// blank (supported behavior) — an unset or unknown applied id counts as diverged.
	if (!m_preset_combo || !m_preset_model)
		return;

	// Re-entrancy guard: setting the active row fires the combo "changed" handler,
	// which would otherwise re-apply a preset.
	m_programmatic_update = true;

	// NOTE: the applied preset is the one whose values were last written, tracked
	// by /current-preset-id — not whatever row happens to be active in the combo.
	const gchar* cur = static_cast<const gchar*>(m_settings->current_preset_id);
	const LayoutPreset* applied = find_preset_by_id(cur ? std::string(cur) : std::string());
	const bool diverged = !applied || compute_preset_diff(*applied, *m_settings);

	remove_unsaved_row(m_preset_model);

	if (diverged)
	{
		GtkTreeIter iter;
		gtk_list_store_append(m_preset_model, &iter);
		gtk_list_store_set(m_preset_model, &iter,
			PRESET_COL_ID,     UNSAVED_PLACEHOLDER_ID,
			PRESET_COL_LABEL,  _("Unsaved custom"),
			PRESET_COL_WEIGHT, PANGO_WEIGHT_NORMAL,
			PRESET_COL_STYLE,  PANGO_STYLE_ITALIC,
			-1);
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_preset_combo), UNSAVED_PLACEHOLDER_ID);
		// An "Unsaved custom" state has no preset description; clear the tooltip.
		if (m_preset_help)
			gtk_widget_set_tooltip_text(m_preset_help, nullptr);
	}
	else
	{
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_preset_combo), applied->id.c_str());
		if (m_preset_help)
		{
			const std::string description = preset_description_for_display(*applied);
			gtk_widget_set_tooltip_text(m_preset_help, description.c_str());
		}
	}

	// Rename/Delete/Export apply only to a real, saved user preset — never a
	// built-in and never the transient "Unsaved custom" placeholder.
	const bool is_user = !diverged && applied && !applied->is_builtin;
	if (m_preset_rename_btn)
		gtk_widget_set_sensitive(m_preset_rename_btn, is_user);
	if (m_preset_delete_btn)
		gtk_widget_set_sensitive(m_preset_delete_btn, is_user);
	if (m_preset_export_btn)
		gtk_widget_set_sensitive(m_preset_export_btn, is_user);

	m_programmatic_update = false;
}

void SettingsDialog::update_grid_controls_state()
{
	if (!m_grid_density_combo)
	{
		return;
	}

	const bool icons_view = (static_cast<int>(m_settings->view_mode) == Settings::ViewAsIcons);
	gtk_widget_set_sensitive(m_grid_density_combo, icons_view);
	if (m_item_icon_size)
		gtk_widget_set_sensitive(m_item_icon_size, icons_view);
	if (m_transparent_grid)
		gtk_widget_set_sensitive(m_transparent_grid, icons_view);
}

void SettingsDialog::sync_preset_widgets()
{
	// Drive every preset-governed widget across all tabs to match the current
	// settings after a preset is applied, then recompute every dependent
	// control's sensitivity. The whole body runs under m_programmatic_update so
	// the cascade of set_active calls cannot write a divergent value back into
	// Settings (supported behavior) — each widget handler early-returns while it is set.
	//
	// The authoritative set of keys driven here is synced_keys(); a unit test
	// asserts it equals governed_keys() so no governed control is left stale
	// (supported behavior). When adding a governed key, add both its sync call below
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
	if (m_menu_opacity)
		gtk_range_set_value(GTK_RANGE(m_menu_opacity),
			static_cast<int>(m_settings->menu_opacity));

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
	if (m_show_profile)
		gtk_switch_set_active(GTK_SWITCH(m_show_profile),
			static_cast<bool>(m_settings->show_profile));
	if (m_profile_shape)
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_profile_shape),
			static_cast<int>(m_settings->profile_shape));
	if (m_profile_shape_label)
		gtk_widget_set_sensitive(m_profile_shape_label,
			static_cast<bool>(m_settings->show_profile));
	if (m_profile_shape)
		gtk_widget_set_sensitive(m_profile_shape,
			static_cast<bool>(m_settings->show_profile));
	if (m_show_session)
		gtk_switch_set_active(GTK_SWITCH(m_show_session),
			static_cast<bool>(m_settings->show_session));

	if (m_grid_density_combo)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_grid_density_combo),
			static_cast<const gchar*>(m_settings->grid_density));
	if (m_transparent_grid)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_transparent_grid),
			static_cast<bool>(m_settings->transparent_grid));

	if (m_places_enabled_switch)
		gtk_switch_set_active(GTK_SWITCH(m_places_enabled_switch),
			static_cast<bool>(m_settings->places_enabled));
	if (m_places_switch_show_icons)
		gtk_switch_set_active(GTK_SWITCH(m_places_switch_show_icons),
			static_cast<bool>(m_settings->places_switch_show_icons));
	if (m_calculator_engine)
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_calculator_engine),
			static_cast<const gchar*>(m_settings->calculator_engine));
	if (m_calculator_result_font_size)
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_calculator_result_font_size),
			static_cast<int>(m_settings->calculator_result_font_size) + 1);
	if (m_calculator_max_decimal_places)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_calculator_max_decimal_places),
			static_cast<int>(m_settings->calculator_max_decimal_places));
	if (m_calculator_engine)
	{
		const bool enabled = g_strcmp0(m_settings->calculator_engine, "none") != 0;
		gtk_widget_set_sensitive(m_calculator_result_font_size, enabled);
		gtk_widget_set_sensitive(m_calculator_max_decimal_places, enabled);
		gtk_widget_set_sensitive(m_calculator_result_font_size_label, enabled);
		gtk_widget_set_sensitive(m_calculator_max_decimal_places_label, enabled);
	}

	if (m_hover_switch_category)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_hover_switch_category),
			static_cast<bool>(m_settings->category_hover_activate));
	if (m_item_icon_size)
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_item_icon_size),
			static_cast<int>(m_settings->launcher_icon_size) + 1);
	if (m_category_icon_size)
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_category_icon_size),
			static_cast<int>(m_settings->category_icon_size) + 1);
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

	// Recompute every dependent control's sensitivity across all tabs (supported behavior):
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

/* append_preset_row:
 * @model:      the preset combo's backing store.
 * @id:         stable selection id.
 * @label:      translated display text.
 * @is_builtin: true → bold (built-in), false → standard weight (saved custom).
 *
 * Appends one concrete preset row with its data-driven typography. Built-ins
 * render bold so a future built-in inherits bold automatically (supported behavior);
 * saved customs render at normal weight. The italic "Unsaved custom" row is not
 * a preset and is added separately by the divergence recompute.
 */
static void append_preset_row(GtkListStore* model, const std::string& id,
	const std::string& label, bool is_builtin)
{
	GtkTreeIter iter;
	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
		SettingsDialog::PRESET_COL_ID,     id.c_str(),
		SettingsDialog::PRESET_COL_LABEL,  label.c_str(),
		SettingsDialog::PRESET_COL_WEIGHT, is_builtin ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		SettingsDialog::PRESET_COL_STYLE,  PANGO_STYLE_NORMAL,
		-1);
}

void SettingsDialog::refresh_preset_combo(const std::string& select_id)
{
	// (void) the legacy select_id parameter: row selection is no longer driven by
	// the caller's hint but by the applied preset id (/current-preset-id) and the
	// live divergence state, resolved in refresh_customized_indicator(). Callers
	// set /current-preset-id (via apply/save) before invoking this, so the active
	// row follows the applied preset automatically.
	(void) select_id;

	if (!m_preset_combo || !m_preset_model)
		return;

	// Guard the rebuild: clearing the model resets the active row, which would
	// otherwise fire the combo "changed" handler and re-apply a preset.
	m_programmatic_update = true;

	gtk_list_store_clear(m_preset_model);

	// Built-ins first (file-seeded order), bold.
	const auto& file_presets = get_file_presets();
	if (!file_presets.empty())
	{
		for (const auto& p : file_presets)
		{
			const std::string label = preset_name_for_display(p);
			append_preset_row(m_preset_model, p.id, label, true);
		}
	}
	else
	{
		// NOTE: fallback if initialize_file_presets() was not called or all files missing.
		for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
		{
			const LayoutPreset& preset = BUILTIN_PRESETS[i];
			append_preset_row(m_preset_model, preset.id,
				preset_name_for_display(preset), true);
		}
	}

	// Then saved customs (uuid order), standard weight.
	const auto& user = enumerate_user_presets(m_settings->channel);
	for (const auto& p : user)
	{
		const std::string label = preset_name_for_display(p);
		append_preset_row(m_preset_model, p.id, label, false);
	}

	m_programmatic_update = false;

	// Resolve the active row + the transient "Unsaved custom" placeholder, and
	// drive Rename/Delete/Export sensitivity. The field is never left blank.
	refresh_customized_indicator();
}

//-----------------------------------------------------------------------------

/* install_layout_mode_handler:
 *
 * Subscribes to the Xfconf channel's "property-changed" signal and triggers
 * apply_layout_mode_sensitivity() whenever /layout-mode flips. Per supported behavior the
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
 * Classifies the current /layout-mode once, then refreshes every layout-driven
 * control. The five supported behavior matrix controls registered in m_layout_controls are
 * driven by the pure control_enabled() table; the remaining out-of-matrix
 * per-region opacity controls keep their windowed-vs-full-screen rule (enabled
 * in Docked and Centered, greyed in Full-Screen). Idempotent and safe to call
 * repeatedly on every layout-mode change.
 */
void SettingsDialog::apply_layout_mode_sensitivity()
{
	const LayoutMode mode = layout_mode_from_key(m_settings->layout_mode);

	for (const auto& entry : m_layout_controls)
	{
		if (entry.first)
			gtk_widget_set_sensitive(entry.first, control_enabled(entry.second, mode));
	}

	// Out-of-matrix per-region opacity controls: windowed (Docked/Centered)
	// enables them, Full-Screen greys them.
	const bool windowed = (mode != LayoutMode::FullScreen);
	for (GtkWidget* w : m_layout_enable_when_docked)
	{
		if (w)
			gtk_widget_set_sensitive(w, windowed);
	}
	for (GtkWidget* w : m_layout_enable_when_fullscreen)
	{
		if (w)
			gtk_widget_set_sensitive(w, !windowed);
	}
}

// Per-tab builders for the Properties dialog live in their own translation
// units under ui/properties/. Each init_*_tab() returns a fully wired
// scrolled container ready to be added to the dialog's stack.
//-----------------------------------------------------------------------------
