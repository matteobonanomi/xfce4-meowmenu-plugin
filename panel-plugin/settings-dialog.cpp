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

static GtkWidget* make_aligned_frame(const gchar* text, GtkWidget* content)
{
	// Create bold label
	gchar* markup = g_markup_printf_escaped("<b>%s</b>", text);
	GtkWidget* label = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);

	// Create frame
	GtkWidget* frame = gtk_frame_new(nullptr);
	gtk_frame_set_label_widget(GTK_FRAME(frame), label);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	// Add content
	gtk_widget_set_margin_start(content, 12);
	gtk_widget_set_margin_top(content, 6);
	gtk_container_add(GTK_CONTAINER(frame), content);

	return frame;
}

//-----------------------------------------------------------------------------

// Frame with bold title and a "?" button that shows an info popover on click.
static GtkWidget* make_info_frame(const gchar* title, GtkWidget* content, const gchar* info_text)
{
	// Header box: [<b>title</b>] [?]
	GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

	gchar* markup = g_markup_printf_escaped("<b>%s</b>", title);
	GtkWidget* title_label = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title_label), markup);
	g_free(markup);
	gtk_box_pack_start(GTK_BOX(header_box), title_label, false, false, 0);

	GtkWidget* info_btn = gtk_button_new_with_label("?");
	gtk_button_set_relief(GTK_BUTTON(info_btn), GTK_RELIEF_NONE);
	gtk_widget_set_valign(info_btn, GTK_ALIGN_CENTER);

	GtkWidget* popover = gtk_popover_new(info_btn);
	GtkWidget* pop_label = gtk_label_new(info_text);
	gtk_label_set_line_wrap(GTK_LABEL(pop_label), true);
	gtk_label_set_max_width_chars(GTK_LABEL(pop_label), 45);
	gtk_widget_set_margin_start(pop_label, 8);
	gtk_widget_set_margin_end(pop_label, 8);
	gtk_widget_set_margin_top(pop_label, 8);
	gtk_widget_set_margin_bottom(pop_label, 8);
	gtk_widget_show(pop_label);
	gtk_container_add(GTK_CONTAINER(popover), pop_label);
	g_signal_connect_swapped(info_btn, "clicked", G_CALLBACK(gtk_popover_popup), popover);

	gtk_box_pack_start(GTK_BOX(header_box), info_btn, false, false, 0);
	gtk_widget_show_all(header_box);

	GtkWidget* frame = gtk_frame_new(nullptr);
	gtk_frame_set_label_widget(GTK_FRAME(frame), header_box);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	gtk_widget_set_margin_start(content, 12);
	gtk_widget_set_margin_top(content, 6);
	gtk_container_add(GTK_CONTAINER(frame), content);

	return frame;
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
	gtk_window_set_icon_name(GTK_WINDOW(m_window), "org.xfce.panel.whiskermenu");
	gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);

	connect(m_window, "response",
		[this](GtkDialog*, int response_id)
		{
			response(response_id);
		});

	// Create sidebar navigation
	GtkStack* stack = GTK_STACK(gtk_stack_new());
	gtk_stack_set_transition_type(stack, GTK_STACK_TRANSITION_TYPE_NONE);

	auto add_page = [stack](GtkWidget* child, const char* id, const char* title)
	{
		GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), true);
		gtk_container_add(GTK_CONTAINER(scroll), child);
		gtk_stack_add_titled(stack, scroll, id, title);
	};

	add_page(init_general_tab(),        "general",     _("General"));
	add_page(init_appearance_tab(),     "appearance",  _("Appearance"));
	add_page(init_behavior_tab(),       "behavior",    _("Behavior"));
	add_page(init_commands_tab(),       "commands",    _("Commands"));
	add_page(init_search_actions_tab(), "search-act",  _("Search Actions"));
	add_page(init_search_tab(),         "adv-search",  _("Advanced Search"));

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

	// Make sure editing is allowed
	gtk_widget_set_sensitive(m_action_remove, true);
	gtk_widget_set_sensitive(m_action_name, true);
	gtk_widget_set_sensitive(m_action_pattern, true);
	gtk_widget_set_sensitive(m_action_command, true);
	gtk_widget_set_sensitive(m_action_regex, true);
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
		gtk_entry_set_text(GTK_ENTRY(m_action_name), "");
		gtk_entry_set_text(GTK_ENTRY(m_action_pattern), "");
		gtk_entry_set_text(GTK_ENTRY(m_action_command), "");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_action_regex), false);

		gtk_widget_set_sensitive(m_action_remove, false);
		gtk_widget_set_sensitive(m_action_name, false);
		gtk_widget_set_sensitive(m_action_pattern, false);
		gtk_widget_set_sensitive(m_action_command, false);
		gtk_widget_set_sensitive(m_action_regex, false);
	}
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

	// Layout mode first.
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_layout_mode_combo),
		static_cast<const gchar*>(m_settings->layout_mode));

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_corner_radius),
		static_cast<int>(m_settings->corner_radius));
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_panel_gap),
		static_cast<int>(m_settings->panel_gap));
	gtk_range_set_value(GTK_RANGE(m_categories_opacity),
		static_cast<int>(m_settings->categories_opacity));
	gtk_range_set_value(GTK_RANGE(m_apps_opacity),
		static_cast<int>(m_settings->apps_opacity));

	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_sidebar_position_combo),
		static_cast<const gchar*>(m_settings->sidebar_position));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_search_bar_position_combo),
		static_cast<const gchar*>(m_settings->search_bar_position));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_profile_position_combo),
		static_cast<const gchar*>(m_settings->profile_position));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_commands_position_combo),
		static_cast<const gchar*>(m_settings->commands_position));

	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_grid_density_combo),
		static_cast<const gchar*>(m_settings->grid_density));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_hover_switch_category),
		static_cast<bool>(m_settings->category_hover_activate));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_position_categories_horizontal),
		static_cast<bool>(m_settings->position_categories_horizontal));

	const int vm = static_cast<int>(m_settings->view_mode);
	if (vm == Settings::ViewAsIcons)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_icons), true);
	else if (vm == Settings::ViewAsTree)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_tree), true);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_list), true);

	update_grid_controls_state();
}

void SettingsDialog::refresh_preset_combo(const std::string& select_id)
{
	m_loading_preset = true;

	// Preserve active ID before clearing
	const gchar* current_raw = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
	std::string active_id = select_id.empty()
		? (current_raw ? std::string(current_raw) : std::string())
		: select_id;

	// Rebuild combo entries
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(m_preset_combo));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo), "classic",    _("Classic"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo), "modern",     _("Modern"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_preset_combo), "fullscreen", _("FullScreen"));

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

	// Update button sensitivity: Rename/Delete only for user presets
	bool is_user = (gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo)) != nullptr)
		&& (active_id != "classic")
		&& (active_id != "modern")
		&& (active_id != "fullscreen");
	if (m_preset_rename_btn)
		gtk_widget_set_sensitive(m_preset_rename_btn, is_user);
	if (m_preset_delete_btn)
		gtk_widget_set_sensitive(m_preset_delete_btn, is_user);
	if (m_preset_export_btn)
		gtk_widget_set_sensitive(m_preset_export_btn, is_user);

	m_loading_preset = false;
}

//-----------------------------------------------------------------------------

GtkWidget* SettingsDialog::init_general_tab()
{
	// Create general page — preset hub
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 12));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);


	// Preset hub section
	GtkGrid* preset_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(preset_table, 12);
	gtk_grid_set_row_spacing(preset_table, 6);

	GtkWidget* preset_frame = make_aligned_frame(_("Layout Preset"), GTK_WIDGET(preset_table));
	gtk_box_pack_start(page, preset_frame, false, false, 0);

	// Preset combobox (T091: populated by refresh_preset_combo)
	GtkWidget* preset_label = gtk_label_new_with_mnemonic(_("_Preset:"));
	gtk_widget_set_halign(preset_label, GTK_ALIGN_START);
	gtk_grid_attach(preset_table, preset_label, 0, 0, 1, 1);

	m_preset_combo = gtk_combo_box_text_new();
	m_preset_rename_btn = nullptr;
	m_preset_delete_btn = nullptr;
	m_preset_export_btn = nullptr;
	gtk_widget_set_hexpand(m_preset_combo, true);
	gtk_grid_attach(preset_table, m_preset_combo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(preset_label), m_preset_combo);

	// Preset description label
	m_preset_description = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(m_preset_description), true);
	gtk_label_set_xalign(GTK_LABEL(m_preset_description), 0.0f);
	gtk_widget_set_hexpand(m_preset_description, true);
	gtk_grid_attach(preset_table, m_preset_description, 0, 1, 2, 1);

	// "Customized" indicator
	m_preset_customized = gtk_label_new(_("● Customized"));
	gtk_label_set_xalign(GTK_LABEL(m_preset_customized), 0.0f);
	gtk_grid_attach(preset_table, m_preset_customized, 0, 2, 2, 1);
	gtk_widget_hide(m_preset_customized);

	// Preset action buttons row
	GtkWidget* action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_top(action_box, 4);
	gtk_grid_attach(preset_table, action_box, 0, 3, 2, 1);

	GtkWidget* reset_preset_btn = gtk_button_new_with_mnemonic(_("_Reset preset"));
	gtk_box_pack_start(GTK_BOX(action_box), reset_preset_btn, false, false, 0);

	connect(reset_preset_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* pid = static_cast<const gchar*>(m_settings->current_preset_id);
			const LayoutPreset* preset = find_preset_by_id(pid ? std::string(pid) : std::string());
			if (!preset)
			{
				return;
			}
			if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				"edit-undo", _("_Reset"),
				_("All customizations will be discarded and the preset values restored."),
				_("Reset preset \"%s\"?"), _(preset->display_name.c_str())))
			{
				apply_preset(*preset, *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_customized_indicator();
			}
		});

	// Save as new preset… (T090)
	GtkWidget* save_btn = gtk_button_new_with_mnemonic(_("_Save as new…"));
	gtk_box_pack_start(GTK_BOX(action_box), save_btn, false, false, 0);

	connect(save_btn, "clicked",
		[this](GtkButton*)
		{
			GtkWidget* dlg = gtk_dialog_new_with_buttons(_("Save as new preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_DIALOG_MODAL,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Save"),   GTK_RESPONSE_OK,
				nullptr);
			gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

			GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
			GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
			gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
			gtk_box_pack_start(GTK_BOX(content), hbox, false, false, 0);

			GtkWidget* lbl = gtk_label_new_with_mnemonic(_("_Name:"));
			gtk_box_pack_start(GTK_BOX(hbox), lbl, false, false, 0);
			GtkWidget* entry = gtk_entry_new();
			gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
			gtk_widget_set_hexpand(entry, true);
			gtk_box_pack_start(GTK_BOX(hbox), entry, true, true, 0);
			gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), entry);

			gtk_widget_show_all(dlg);
			if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
			{
				const gchar* name = gtk_entry_get_text(GTK_ENTRY(entry));
				std::string uuid = save_current_as_user_preset(name ? name : "", *m_settings);
				if (uuid.empty())
				{
					xfce_dialog_show_warning(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
						_("A preset with that name already exists or the name is empty."),
						"%s", _("Could not save preset"));
				}
				else
				{
					refresh_preset_combo(uuid);
					m_plugin->reload_menu();
					refresh_customized_indicator();
				}
			}
			gtk_widget_destroy(dlg);
		});

	// Rename… (T090) — enabled only for user presets
	m_preset_rename_btn = gtk_button_new_with_mnemonic(_("Re_name…"));
	gtk_widget_set_sensitive(m_preset_rename_btn, false);
	gtk_box_pack_start(GTK_BOX(action_box), m_preset_rename_btn, false, false, 0);

	connect(m_preset_rename_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
			if (!active_id)
				return;
			const LayoutPreset* preset = find_preset_by_id(std::string(active_id));
			if (!preset || preset->is_builtin)
				return;

			GtkWidget* dlg = gtk_dialog_new_with_buttons(_("Rename preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_DIALOG_MODAL,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Rename"), GTK_RESPONSE_OK,
				nullptr);
			gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

			GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
			GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
			gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
			gtk_box_pack_start(GTK_BOX(content), hbox, false, false, 0);

			GtkWidget* lbl = gtk_label_new_with_mnemonic(_("_Name:"));
			gtk_box_pack_start(GTK_BOX(hbox), lbl, false, false, 0);
			GtkWidget* entry = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(entry), preset->display_name.c_str());
			gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
			gtk_widget_set_hexpand(entry, true);
			gtk_box_pack_start(GTK_BOX(hbox), entry, true, true, 0);
			gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), entry);

			gtk_widget_show_all(dlg);
			if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
			{
				const gchar* name = gtk_entry_get_text(GTK_ENTRY(entry));
				std::string uuid = preset->id;
				if (!rename_user_preset(uuid, name ? name : "", *m_settings))
				{
					xfce_dialog_show_warning(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
						_("A preset with that name already exists or the name is empty."),
						"%s", _("Could not rename preset"));
				}
				else
				{
					refresh_preset_combo(uuid);
				}
			}
			gtk_widget_destroy(dlg);
		});

	// Delete (T090) — enabled only for user presets
	m_preset_delete_btn = gtk_button_new_with_mnemonic(_("_Delete"));
	gtk_widget_set_sensitive(m_preset_delete_btn, false);
	gtk_box_pack_start(GTK_BOX(action_box), m_preset_delete_btn, false, false, 0);

	connect(m_preset_delete_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
			if (!active_id)
				return;
			const LayoutPreset* preset = find_preset_by_id(std::string(active_id));
			if (!preset || preset->is_builtin)
				return;

			if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				"edit-delete", _("_Delete"),
				_("This preset will be permanently removed."),
				_("Delete preset \"%s\"?"), preset->display_name.c_str()))
			{
				std::string uuid = preset->id;
				delete_user_preset(uuid, *m_settings);
				refresh_preset_combo("modern");
				apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_customized_indicator();
			}
		});

	// Export / Import row (T110, T111)
	GtkWidget* io_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_top(io_box, 2);
	gtk_grid_attach(preset_table, io_box, 0, 4, 2, 1);

	m_preset_export_btn = gtk_button_new_with_mnemonic(_("E_xport…"));
	gtk_widget_set_sensitive(m_preset_export_btn, false);
	gtk_box_pack_start(GTK_BOX(io_box), m_preset_export_btn, false, false, 0);

	connect(m_preset_export_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
			if (!active_id)
				return;
			const LayoutPreset* preset = find_preset_by_id(std::string(active_id));
			if (!preset || preset->is_builtin)
				return;

			GtkFileChooserNative* chooser = gtk_file_chooser_native_new(
				_("Export preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				_("_Export"), _("_Cancel"));

			std::string suggested = preset->display_name + ".meowpreset";
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), suggested.c_str());

			GtkFileFilter* filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter, _("MeowMenu preset (*.meowpreset)"));
			gtk_file_filter_add_pattern(filter, "*.meowpreset");
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

			if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
			{
				gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
				if (path)
				{
					if (!export_user_preset(preset->id, std::string(path), *m_settings))
					{
						xfce_dialog_show_error(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
							nullptr, "%s", _("Could not export preset."));
					}
					g_free(path);
				}
			}
			g_object_unref(chooser);
		});

	GtkWidget* import_btn = gtk_button_new_with_mnemonic(_("_Import…"));
	gtk_box_pack_start(GTK_BOX(io_box), import_btn, false, false, 0);

	connect(import_btn, "clicked",
		[this](GtkButton*)
		{
			GtkFileChooserNative* chooser = gtk_file_chooser_native_new(
				_("Import preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				_("_Import"), _("_Cancel"));

			GtkFileFilter* filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter, _("MeowMenu preset (*.meowpreset)"));
			gtk_file_filter_add_pattern(filter, "*.meowpreset");
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

			if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT)
			{
				g_object_unref(chooser);
				return;
			}
			gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
			g_object_unref(chooser);
			if (!path)
				return;

			std::string file_path(path);
			g_free(path);

			ImportResult result = import_user_preset(file_path, *m_settings);

			if (result.status == ImportStatus::ConflictUser)
			{
				GtkWidget* dlg = gtk_message_dialog_new(
					GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					_("A preset named \"%s\" already exists. What would you like to do?"),
					result.display_name.c_str());
				gtk_dialog_add_buttons(GTK_DIALOG(dlg),
					_("_Overwrite"), 1,
					_("Re_name"),   2,
					_("_Cancel"),   GTK_RESPONSE_CANCEL,
					nullptr);
				int resp = gtk_dialog_run(GTK_DIALOG(dlg));
				gtk_widget_destroy(dlg);

				if (resp == 1)
				{
					result = import_user_preset(file_path, *m_settings, {}, result.conflict_uuid);
				}
				else if (resp == 2)
				{
					GtkWidget* ndlg = gtk_dialog_new_with_buttons(_("Rename imported preset"),
						GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
						GTK_DIALOG_MODAL,
						_("_Cancel"), GTK_RESPONSE_CANCEL,
						_("_Import"), GTK_RESPONSE_OK,
						nullptr);
					gtk_dialog_set_default_response(GTK_DIALOG(ndlg), GTK_RESPONSE_OK);
					GtkWidget* ncontent = gtk_dialog_get_content_area(GTK_DIALOG(ndlg));
					GtkWidget* nhbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
					gtk_container_set_border_width(GTK_CONTAINER(nhbox), 12);
					gtk_box_pack_start(GTK_BOX(ncontent), nhbox, false, false, 0);
					GtkWidget* nlbl = gtk_label_new_with_mnemonic(_("_Name:"));
					gtk_box_pack_start(GTK_BOX(nhbox), nlbl, false, false, 0);
					GtkWidget* nentry = gtk_entry_new();
					gtk_entry_set_text(GTK_ENTRY(nentry), result.display_name.c_str());
					gtk_entry_set_activates_default(GTK_ENTRY(nentry), true);
					gtk_widget_set_hexpand(nentry, true);
					gtk_box_pack_start(GTK_BOX(nhbox), nentry, true, true, 0);
					gtk_label_set_mnemonic_widget(GTK_LABEL(nlbl), nentry);
					gtk_widget_show_all(ndlg);

					if (gtk_dialog_run(GTK_DIALOG(ndlg)) == GTK_RESPONSE_OK)
					{
						const gchar* nname = gtk_entry_get_text(GTK_ENTRY(nentry));
						result = import_user_preset(file_path, *m_settings,
							nname ? std::string(nname) : std::string());
					}
					else
					{
						result.status = ImportStatus::ParseError; // signal: cancelled
					}
					gtk_widget_destroy(ndlg);
				}
				else
				{
					return; // user cancelled
				}
			}

			if (result.status == ImportStatus::Ok)
			{
				refresh_preset_combo(result.new_uuid);
			}
			else if (result.status != ImportStatus::ParseError)
			{
				xfce_dialog_show_error(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
					nullptr, "%s: %s", _("Could not import preset"),
					result.error_message.c_str());
			}
		});

	// Reset to defaults (full channel reset)  — T112
	GtkWidget* defaults_btn = gtk_button_new_with_mnemonic(_("Reset to _defaults"));
	gtk_widget_set_margin_top(defaults_btn, 8);
	gtk_widget_set_halign(defaults_btn, GTK_ALIGN_START);
	gtk_grid_attach(preset_table, defaults_btn, 0, 5, 2, 1);

	connect(defaults_btn, "clicked",
		[this](GtkButton*)
		{
			if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				"edit-undo", _("_Reset"),
				_("All settings will be reset to defaults and the Modern preset will be applied."),
				_("Reset all settings to defaults?")))
			{
				// Hard reset: clear all plugin properties except saved user presets.
				GHashTable* props = xfconf_channel_get_properties(m_settings->channel, nullptr);
				if (props)
				{
					GHashTableIter iter;
					gpointer key_ptr, value_ptr;
					g_hash_table_iter_init(&iter, props);
					while (g_hash_table_iter_next(&iter, &key_ptr, &value_ptr))
					{
						(void)value_ptr;
						const gchar* path = static_cast<const gchar*>(key_ptr);
						if (g_str_has_prefix(path, "/presets"))
						{
							continue;
						}
						xfconf_channel_reset_property(m_settings->channel, path, FALSE);
					}
					g_hash_table_unref(props);
				}

				apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_preset_combo("modern");
				refresh_customized_indicator();
			}
		});

	// Populate combo (T091) and set initial description
	refresh_preset_combo(static_cast<const gchar*>(m_settings->current_preset_id)
		? std::string(static_cast<const gchar*>(m_settings->current_preset_id))
		: std::string());
	{
		const gchar* pid = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
		const LayoutPreset* preset = find_preset_by_id(pid ? std::string(pid) : std::string());
		if (preset)
		{
			gtk_label_set_text(GTK_LABEL(m_preset_description), _(preset->description.c_str()));
		}
	}

	// Update description and apply preset on combo change (T051, T091)
	connect(m_preset_combo, "changed",
		[this](GtkComboBox* combo)
		{
			if (m_loading_preset)
				return;
			const gchar* id = gtk_combo_box_get_active_id(combo);
			if (!id)
			{
				return;
			}
			const LayoutPreset* preset = find_preset_by_id(std::string(id));
			if (!preset)
			{
				return;
			}
			gtk_label_set_text(GTK_LABEL(m_preset_description), _(preset->description.c_str()));
			apply_preset(*preset, *m_settings);
			m_plugin->reload_menu();
			sync_preset_widgets();
			refresh_customized_indicator();
			// Update Rename/Delete/Export sensitivity
			bool is_user = !preset->is_builtin;
			gtk_widget_set_sensitive(m_preset_rename_btn, is_user);
			gtk_widget_set_sensitive(m_preset_delete_btn, is_user);
			gtk_widget_set_sensitive(m_preset_export_btn, is_user);
		});


	// T120: Wayland / FullScreen warning InfoBar
	// Shown only when FullScreen is active but gtk-layer-shell is unavailable.
#if defined(HAVE_GTK_LAYER_SHELL)
	(void)0; // gtk-layer-shell is present — no InfoBar needed
#else
	{
		GtkWidget* infobar = gtk_info_bar_new();
		gtk_info_bar_set_message_type(GTK_INFO_BAR(infobar), GTK_MESSAGE_WARNING);
		GtkWidget* infobar_content = gtk_info_bar_get_content_area(GTK_INFO_BAR(infobar));
		GtkWidget* infobar_label = gtk_label_new(
			_("FullScreen mode requires gtk-layer-shell on Wayland. "
			  "The menu will open as a regular window instead."));
		gtk_label_set_line_wrap(GTK_LABEL(infobar_label), true);
		gtk_label_set_xalign(GTK_LABEL(infobar_label), 0.0f);
		gtk_container_add(GTK_CONTAINER(infobar_content), infobar_label);

		const gchar* cur_id = static_cast<const gchar*>(m_settings->current_preset_id);
		gtk_widget_set_visible(infobar, cur_id && strcmp(cur_id, "fullscreen") == 0);
		gtk_box_pack_start(page, infobar, false, false, 0);
	}
#endif

	// Display preferences section
	GtkGrid* display_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(display_table, 12);
	gtk_grid_set_row_spacing(display_table, 6);

	GtkWidget* display_frame = make_aligned_frame(_("Display"), GTK_WIDGET(display_table));
	gtk_box_pack_start(page, display_frame, false, false, 0);

	// Add option to use generic names
	m_show_generic_names = gtk_check_button_new_with_mnemonic(_("Show generic application _names"));
	gtk_grid_attach(display_table, m_show_generic_names, 0, 0, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_generic_names), !m_settings->launcher_show_name);

	connect(m_show_generic_names, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_name = !gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	// Add option to show category names
	m_show_category_names = gtk_check_button_new_with_mnemonic(_("Show cate_gory names"));
	gtk_grid_attach(display_table, m_show_category_names, 0, 1, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_category_names), m_settings->category_show_name);
	gtk_widget_set_sensitive(m_show_category_names, (m_settings->category_icon_size != -1) && !m_settings->position_categories_horizontal);

	connect(m_show_category_names, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->category_show_name = gtk_toggle_button_get_active(button);
		});

	// Add option to show tooltips
	m_show_tooltips = gtk_check_button_new_with_mnemonic(_("Show application too_ltips"));
	gtk_grid_attach(display_table, m_show_tooltips, 0, 2, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_tooltips), m_settings->launcher_show_tooltip);

	connect(m_show_tooltips, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_tooltip = gtk_toggle_button_get_active(button);
		});

	// Add option to show descriptions
	m_show_descriptions = gtk_check_button_new_with_mnemonic(_("Show application _descriptions"));
	gtk_grid_attach(display_table, m_show_descriptions, 0, 3, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_descriptions), m_settings->launcher_show_description);
	gtk_widget_set_sensitive(m_show_descriptions, m_settings->view_mode != Settings::ViewAsIcons);

	connect(m_show_descriptions, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_description = gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	return GTK_WIDGET(page);
}

//-----------------------------------------------------------------------------

GtkWidget* SettingsDialog::init_appearance_tab()
{
	// Create appearance page
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);


	// Align labels across sections
	GtkSizeGroup* label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);


	// Create view section (moved from General tab)
	GtkGrid* view_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(view_table, 12);
	gtk_grid_set_row_spacing(view_table, 6);

	GtkWidget* view_frame = make_aligned_frame(_("View"), GTK_WIDGET(view_table));
	gtk_box_pack_start(page, view_frame, false, false, 0);

	// View mode radio buttons
	GtkButtonBox* display_box = GTK_BUTTON_BOX(gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_widget_set_halign(GTK_WIDGET(display_box), GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(GTK_WIDGET(display_box), false);
	gtk_button_box_set_layout(display_box, GTK_BUTTONBOX_EXPAND);
	gtk_grid_attach(view_table, GTK_WIDGET(display_box), 0, 0, 2, 1);
	gtk_widget_set_margin_bottom(GTK_WIDGET(display_box), 6);

	m_show_as_icons = gtk_radio_button_new_with_mnemonic(nullptr, _("Show as _icons"));
	{
		const gchar* icons[] = {
			"view-list-icons",
			"view-grid",
			nullptr
		};
		GIcon* gicon = g_themed_icon_new_from_names(const_cast<gchar**>(icons), -1);
		gtk_button_set_image(GTK_BUTTON(m_show_as_icons), gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DND));
		g_object_unref(gicon);
	}
	gtk_button_set_image_position(GTK_BUTTON(m_show_as_icons), GTK_POS_TOP);
	gtk_button_set_always_show_image(GTK_BUTTON(m_show_as_icons), true);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_show_as_icons), false);
	gtk_box_pack_start(GTK_BOX(display_box), m_show_as_icons, true, true, 0);

	m_show_as_list = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(m_show_as_icons), _("Show as lis_t"));
	{
		const gchar* icons[] = {
			"view-list-compact",
			"view-list-details",
			"view-list",
			nullptr
		};
		GIcon* gicon = g_themed_icon_new_from_names(const_cast<gchar**>(icons), -1);
		gtk_button_set_image(GTK_BUTTON(m_show_as_list), gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DND));
		g_object_unref(gicon);
	}
	gtk_button_set_image_position(GTK_BUTTON(m_show_as_list), GTK_POS_TOP);
	gtk_button_set_always_show_image(GTK_BUTTON(m_show_as_list), true);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_show_as_list), false);
	gtk_box_pack_start(GTK_BOX(display_box), m_show_as_list, true, true, 0);

	m_show_as_tree = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(m_show_as_list), _("Show as t_ree"));
	{
		const gchar* icons[] = {
			"view-list-tree",
			"view-list-details",
			"pan-end",
			nullptr
		};
		GIcon* gicon = g_themed_icon_new_from_names(const_cast<gchar**>(icons), -1);
		gtk_button_set_image(GTK_BUTTON(m_show_as_tree), gtk_image_new_from_gicon(gicon, GTK_ICON_SIZE_DND));
		g_object_unref(gicon);
	}
	gtk_button_set_image_position(GTK_BUTTON(m_show_as_tree), GTK_POS_TOP);
	gtk_button_set_always_show_image(GTK_BUTTON(m_show_as_tree), true);
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_show_as_tree), false);
	gtk_box_pack_start(GTK_BOX(display_box), m_show_as_tree, true, true, 0);

	if (m_settings->view_mode == Settings::ViewAsIcons)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_icons), true);
	}
	else if (m_settings->view_mode == Settings::ViewAsTree)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_tree), true);
	}
	else
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_as_list), true);
	}

	connect(m_show_as_icons, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->view_mode = Settings::ViewAsIcons;
				m_plugin->reload_menu();
				gtk_widget_set_sensitive(m_show_descriptions, false);
				update_grid_controls_state();
			}
		});

	connect(m_show_as_list, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->view_mode = Settings::ViewAsList;
				m_plugin->reload_menu();
				gtk_widget_set_sensitive(m_show_descriptions, true);
				update_grid_controls_state();
			}
		});

	connect(m_show_as_tree, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->view_mode = Settings::ViewAsTree;
				m_plugin->reload_menu();
				gtk_widget_set_sensitive(m_show_descriptions, true);
				update_grid_controls_state();
			}
		});

	// Application icon size
	GtkWidget* label = gtk_label_new_with_mnemonic(_("Application icon si_ze:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(view_table, label, 0, 1, 1, 1);

	m_item_icon_size = gtk_combo_box_text_new();
	gtk_widget_set_halign(m_item_icon_size, GTK_ALIGN_START);
	gtk_widget_set_hexpand(m_item_icon_size, false);
	const auto icon_sizes = IconSize::get_strings();
	for (const auto& icon_size : icon_sizes)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_item_icon_size), icon_size.c_str());
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_item_icon_size), m_settings->launcher_icon_size + 1);
	gtk_grid_attach(view_table, m_item_icon_size, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_item_icon_size);

	connect(m_item_icon_size, "changed",
		[this](GtkComboBox* combo)
		{
			m_settings->launcher_icon_size = gtk_combo_box_get_active(combo) - 1;
		});

	// Category icon size
	label = gtk_label_new_with_mnemonic(_("Categ_ory icon size:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(view_table, label, 0, 2, 1, 1);

	m_category_icon_size = gtk_combo_box_text_new();
	gtk_widget_set_halign(m_category_icon_size, GTK_ALIGN_START);
	gtk_widget_set_hexpand(m_category_icon_size, false);
	for (const auto& icon_size : icon_sizes)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_category_icon_size), icon_size.c_str());
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_category_icon_size), m_settings->category_icon_size + 1);
	gtk_grid_attach(view_table, m_category_icon_size, 1, 2, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_category_icon_size);

	connect(m_category_icon_size, "changed",
		[this](GtkComboBox* combo)
		{
			m_settings->category_icon_size = gtk_combo_box_get_active(combo) - 1;
			const bool active = (m_settings->category_icon_size != -1) && !m_settings->position_categories_horizontal;
			gtk_widget_set_sensitive(m_show_category_names, active);
			if (!active)
			{
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_category_names), true);
			}
		});

	// Menu width
	label = gtk_label_new_with_mnemonic(_("Menu _width:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(view_table, label, 0, 3, 1, 1);

	m_menu_width = gtk_spin_button_new_with_range(10, SHRT_MAX, 1);
	gtk_widget_set_halign(m_menu_width, GTK_ALIGN_START);
	gtk_widget_set_hexpand(m_menu_width, false);
	gtk_grid_attach(view_table, m_menu_width, 1, 3, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_menu_width);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_width), m_settings->menu_width);

	connect(m_menu_width, "value-changed",
		[this](GtkSpinButton* button)
		{
			m_settings->menu_width = gtk_spin_button_get_value_as_int(button);
		});

	// Menu height
	label = gtk_label_new_with_mnemonic(_("Menu _height:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(view_table, label, 0, 4, 1, 1);

	m_menu_height = gtk_spin_button_new_with_range(10, SHRT_MAX, 1);
	gtk_widget_set_halign(m_menu_height, GTK_ALIGN_START);
	gtk_widget_set_hexpand(m_menu_height, false);
	gtk_grid_attach(view_table, m_menu_height, 1, 4, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_menu_height);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_height), m_settings->menu_height);

	connect(m_menu_height, "value-changed",
		[this](GtkSpinButton* button)
		{
			m_settings->menu_height = gtk_spin_button_get_value_as_int(button);
		});


	// Create menu section
	GtkGrid* menu_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(menu_table, 12);
	gtk_grid_set_row_spacing(menu_table, 6);

	GtkWidget* behavior_frame = make_aligned_frame(_("Menu"), GTK_WIDGET(menu_table));
	gtk_box_pack_start(page, behavior_frame, false, false, 0);

	// Add option to use horizontal categories
	m_position_categories_horizontal = gtk_check_button_new_with_mnemonic(_("Position categories _horizontally"));
	gtk_grid_attach(menu_table, m_position_categories_horizontal, 0, 0, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_position_categories_horizontal), m_settings->position_categories_horizontal);

	connect(m_position_categories_horizontal, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->position_categories_horizontal = gtk_toggle_button_get_active(button);
			const bool active = (m_settings->category_icon_size != -1) && !m_settings->position_categories_horizontal;
			gtk_widget_set_sensitive(m_show_category_names, active);
		});

	// Add profile shape selector
	label = gtk_label_new_with_mnemonic(_("P_rofile:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, label, 0, 1, 1, 1);

	m_profile_shape = gtk_combo_box_text_new();
	gtk_widget_set_halign(m_profile_shape, GTK_ALIGN_START);
	gtk_widget_set_hexpand(m_profile_shape, true);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_profile_shape), _("Round Picture"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_profile_shape), _("Square Picture"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_profile_shape), _("Hidden"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_profile_shape), m_settings->profile_shape);
	gtk_grid_attach(menu_table, m_profile_shape, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_profile_shape);

	connect(m_profile_shape, "changed",
		[this](GtkComboBox* combo)
		{
			m_settings->profile_shape = gtk_combo_box_get_active(combo);
		});

	gtk_size_group_add_widget(label_size_group, label);
	gtk_size_group_add_widget(size_group, m_profile_shape);


	// Create panel button section
	// Customization section (T070) — granular preset controls
	GtkGrid* custom_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(custom_table, 12);
	gtk_grid_set_row_spacing(custom_table, 6);

	GtkWidget* custom_frame = make_aligned_frame(_("Customization"), GTK_WIDGET(custom_table));
	gtk_box_pack_start(page, custom_frame, false, false, 0);

	// Corner radius
	label = gtk_label_new_with_mnemonic(_("_Corner radius:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(custom_table, label, 0, 0, 1, 1);

	m_corner_radius = gtk_spin_button_new_with_range(0, 24, 1);
	gtk_widget_set_halign(m_corner_radius, GTK_ALIGN_START);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_corner_radius), m_settings->corner_radius);
	gtk_grid_attach(custom_table, m_corner_radius, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_corner_radius);
	gtk_size_group_add_widget(label_size_group, label);
	gtk_size_group_add_widget(size_group, m_corner_radius);

	connect(m_corner_radius, "value-changed",
		[this](GtkSpinButton* button)
		{
			m_settings->corner_radius = gtk_spin_button_get_value_as_int(button);
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Categories opacity
	label = gtk_label_new_with_mnemonic(_("_Categories opacity:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(custom_table, label, 0, 1, 1, 1);

	m_categories_opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_widget_set_hexpand(m_categories_opacity, true);
	gtk_scale_set_value_pos(GTK_SCALE(m_categories_opacity), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(m_categories_opacity), m_settings->categories_opacity);
	gtk_grid_attach(custom_table, m_categories_opacity, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_categories_opacity);
	gtk_size_group_add_widget(label_size_group, label);

	connect(m_categories_opacity, "value-changed",
		[this](GtkRange* range)
		{
			m_settings->categories_opacity = static_cast<int>(gtk_range_get_value(range));
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Apps & search opacity
	label = gtk_label_new_with_mnemonic(_("_Apps opacity:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(custom_table, label, 0, 2, 1, 1);

	m_apps_opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_widget_set_hexpand(m_apps_opacity, true);
	gtk_scale_set_value_pos(GTK_SCALE(m_apps_opacity), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(m_apps_opacity), m_settings->apps_opacity);
	gtk_grid_attach(custom_table, m_apps_opacity, 1, 2, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_apps_opacity);
	gtk_size_group_add_widget(label_size_group, label);

	connect(m_apps_opacity, "value-changed",
		[this](GtkRange* range)
		{
			m_settings->apps_opacity = static_cast<int>(gtk_range_get_value(range));
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Sidebar position
	label = gtk_label_new_with_mnemonic(_("_Sidebar:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(custom_table, label, 0, 3, 1, 1);

	m_sidebar_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "left", _("Left"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "right", _("Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "hidden", _("Hidden"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_sidebar_position_combo),
		static_cast<const gchar*>(m_settings->sidebar_position));
	gtk_grid_attach(custom_table, m_sidebar_position_combo, 1, 3, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_sidebar_position_combo);
	gtk_size_group_add_widget(label_size_group, label);
	gtk_size_group_add_widget(size_group, m_sidebar_position_combo);

	connect(m_sidebar_position_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val)
			{
				m_settings->sidebar_position = val;
				m_plugin->reload_menu();
				refresh_customized_indicator();
			}
		});

	// Search bar position
	label = gtk_label_new_with_mnemonic(_("_Search bar:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(custom_table, label, 0, 4, 1, 1);

	m_search_bar_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_search_bar_position_combo), "top", _("Top"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_search_bar_position_combo), "bottom", _("Bottom"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_search_bar_position_combo),
		static_cast<const gchar*>(m_settings->search_bar_position));
	gtk_grid_attach(custom_table, m_search_bar_position_combo, 1, 4, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_search_bar_position_combo);
	gtk_size_group_add_widget(label_size_group, label);
	gtk_size_group_add_widget(size_group, m_search_bar_position_combo);

	connect(m_search_bar_position_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val)
			{
				m_settings->search_bar_position = val;
				m_plugin->reload_menu();
				refresh_customized_indicator();
			}
		});

	// Profile position
	label = gtk_label_new_with_mnemonic(_("_Profile:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(custom_table, label, 0, 5, 1, 1);

	m_profile_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profile_position_combo), "top", _("Top"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profile_position_combo), "bottom", _("Bottom"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profile_position_combo), "bottom-right", _("Bottom Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profile_position_combo), "hidden", _("Hidden"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_profile_position_combo),
		static_cast<const gchar*>(m_settings->profile_position));
	gtk_grid_attach(custom_table, m_profile_position_combo, 1, 5, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_profile_position_combo);
	gtk_size_group_add_widget(label_size_group, label);
	gtk_size_group_add_widget(size_group, m_profile_position_combo);

	connect(m_profile_position_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val)
			{
				m_settings->profile_position = val;
				m_plugin->reload_menu();
				refresh_customized_indicator();
			}
		});

	// Commands position
	label = gtk_label_new_with_mnemonic(_("Co_mmands:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(custom_table, label, 0, 6, 1, 1);

	m_commands_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_commands_position_combo), "top-right", _("Top Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_commands_position_combo), "bottom-right", _("Bottom Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_commands_position_combo), "hidden", _("Hidden"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_commands_position_combo),
		static_cast<const gchar*>(m_settings->commands_position));
	gtk_grid_attach(custom_table, m_commands_position_combo, 1, 6, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_commands_position_combo);
	gtk_size_group_add_widget(label_size_group, label);
	gtk_size_group_add_widget(size_group, m_commands_position_combo);

	connect(m_commands_position_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val)
			{
				m_settings->commands_position = val;
				m_plugin->reload_menu();
				refresh_customized_indicator();
			}
		});


	GtkGrid* panel_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(panel_table, 12);
	gtk_grid_set_row_spacing(panel_table, 6);

	GtkWidget* recent_frame = make_aligned_frame(_("Panel Button"), GTK_WIDGET(panel_table));
	gtk_box_pack_start(page, recent_frame, false, false, 0);

	// Add button style selector
	label = gtk_label_new_with_mnemonic(_("Di_splay:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(panel_table, label, 0, 0, 1, 1);

	m_button_style = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_button_style), _("Icon"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_button_style), _("Title"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_button_style), _("Icon and title"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_button_style), static_cast<int>(m_plugin->get_button_style()) - 1);
	gtk_widget_set_halign(m_button_style, GTK_ALIGN_START);
	gtk_widget_set_hexpand(m_button_style, false);
	gtk_grid_attach(panel_table, m_button_style, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_button_style);

	connect(m_button_style, "changed",
		[this](GtkComboBox* combo)
		{
			m_plugin->set_button_style(Plugin::ButtonStyle(gtk_combo_box_get_active(combo) + 1));
			gtk_widget_set_sensitive(m_button_single_row, gtk_combo_box_get_active(combo) == 0);
		});

	gtk_size_group_add_widget(label_size_group, label);
	gtk_size_group_add_widget(size_group, m_button_style);

	// Add title selector
	label = gtk_label_new_with_mnemonic(_("_Title:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(panel_table, label, 0, 1, 1, 1);

	m_title = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(m_title), m_settings->button_title);
	gtk_widget_set_hexpand(m_title, true);
	gtk_grid_attach(panel_table, m_title, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_title);

	connect(m_title, "changed",
		[this](GtkEditable* editable)
		{
			const gchar* text = gtk_entry_get_text(GTK_ENTRY(editable));
			m_plugin->set_button_title(text ? text : "");
		});

	// Add icon selector
	label = gtk_label_new_with_mnemonic(_("_Icon:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(panel_table, label, 0, 2, 1, 1);

	m_icon_button = gtk_button_new();
	gtk_widget_set_halign(m_icon_button, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_icon_button);
	gtk_grid_attach(panel_table, m_icon_button, 1, 2, 1, 1);

	connect(m_icon_button, "clicked",
		[this](GtkButton*)
		{
			choose_icon();
		});

	m_icon = gtk_image_new_from_icon_name(m_settings->button_icon_name, GTK_ICON_SIZE_DIALOG);
	gtk_container_add(GTK_CONTAINER(m_icon_button), m_icon);

	m_button_single_row = gtk_check_button_new_with_mnemonic(_("Use a single _panel row"));
	gtk_grid_attach(panel_table, m_button_single_row, 1, 3, 1, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_single_row), m_settings->button_single_row);
	gtk_widget_set_sensitive(m_button_single_row, gtk_combo_box_get_active(GTK_COMBO_BOX (m_button_style)) == 0);

	connect(m_button_single_row, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->button_single_row = gtk_toggle_button_get_active(button);
			m_plugin->set_button_style(m_plugin->get_button_style());
		});

	return GTK_WIDGET(page);
}

//-----------------------------------------------------------------------------

GtkWidget* SettingsDialog::init_behavior_tab()
{
	// Create behavior page
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);


	// Create default display section
	GtkBox* display_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	GtkWidget* display_frame = make_aligned_frame(_("Default Category"), GTK_WIDGET(display_vbox));
	gtk_box_pack_start(page, display_frame, false, false, 0);

	// Add option to display favorites
	m_display_favorites = gtk_radio_button_new_with_mnemonic(nullptr, _("Favorites"));
	gtk_box_pack_start(display_vbox, m_display_favorites, true, true, 0);

	// Add option to display recently used
	m_display_recent = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(m_display_favorites), _("Recently Used"));
	gtk_box_pack_start(display_vbox, m_display_recent, true, true, 0);
	gtk_widget_set_sensitive(GTK_WIDGET(m_display_recent), m_settings->recent_items_max);

	// Add option to display all applications
	m_display_applications = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(m_display_recent), _("All Applications"));
	gtk_box_pack_start(display_vbox, m_display_applications, true, true, 0);

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
			{
				m_settings->default_category = Settings::CategoryFavorites;
			}
		});

	connect(m_display_recent, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->default_category = Settings::CategoryRecent;
			}
		});

	connect(m_display_applications, "toggled",
		[this](GtkToggleButton* button)
		{
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->default_category = Settings::CategoryAll;
			}
		});


	// Create menu section
	GtkBox* behavior_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	GtkWidget* behavior_frame = make_aligned_frame(_("Menu"), GTK_WIDGET(behavior_vbox));
	gtk_box_pack_start(page, behavior_frame, false, false, 0);

	// Add option to switch categories by hovering
	m_hover_switch_category = gtk_check_button_new_with_mnemonic(_("Switch categories by _hovering"));
	gtk_box_pack_start(behavior_vbox, m_hover_switch_category, true, true, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_hover_switch_category), m_settings->category_hover_activate);

	connect(m_hover_switch_category, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->category_hover_activate = gtk_toggle_button_get_active(button);
		});

	// Add option to stay when menu loses focus
	m_stay_on_focus_out = gtk_check_button_new_with_mnemonic(_("Stay _visible when focus is lost"));
	gtk_box_pack_start(behavior_vbox, m_stay_on_focus_out, true, true, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_stay_on_focus_out), m_settings->stay_on_focus_out);

	connect(m_stay_on_focus_out, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->stay_on_focus_out = gtk_toggle_button_get_active(button);
		});

	// Add option to sort categories
	m_sort_categories = gtk_check_button_new_with_mnemonic(_("Sort ca_tegories"));
	gtk_box_pack_start(behavior_vbox, m_sort_categories, true, true, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_sort_categories), m_settings->sort_categories);

	connect(m_sort_categories, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->sort_categories = gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});


	// Create recently used section
	GtkGrid* recent_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(recent_table, 12);
	gtk_grid_set_row_spacing(recent_table, 6);

	GtkWidget* recent_frame = make_aligned_frame(_("Recently Used"), GTK_WIDGET(recent_table));
	gtk_box_pack_start(page, recent_frame, false, false, 0);

	// Add value to change maximum number of recently used entries
	GtkWidget* label = gtk_label_new_with_mnemonic(_("Amount of _items:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(recent_table, label, 0, 0, 1, 1);

	m_recent_items_max = gtk_spin_button_new_with_range(0, 100, 1);
	gtk_grid_attach(recent_table, m_recent_items_max, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_recent_items_max);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_recent_items_max), m_settings->recent_items_max);

	connect(m_recent_items_max, "value-changed",
		[this](GtkSpinButton* button)
		{
			m_settings->recent_items_max = gtk_spin_button_get_value_as_int(button);

			const bool active = m_settings->recent_items_max;
			gtk_widget_set_sensitive(GTK_WIDGET(m_display_recent), active);
			if (!active && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_display_recent)))
			{
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_favorites), true);
			}
		});

	// Add option to remember favorites
	m_remember_favorites = gtk_check_button_new_with_mnemonic(_("Include _favorites"));
	gtk_grid_attach(recent_table, m_remember_favorites, 0, 1, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_remember_favorites), m_settings->favorites_in_recent);

	connect(m_remember_favorites, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->favorites_in_recent = gtk_toggle_button_get_active(button);
		});


	// Layout section (T071)
	GtkGrid* layout_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(layout_table, 12);
	gtk_grid_set_row_spacing(layout_table, 6);

	GtkWidget* layout_frame = make_aligned_frame(_("Layout"), GTK_WIDGET(layout_table));
	gtk_box_pack_start(page, layout_frame, false, false, 0);

	// Panel gap
	GtkWidget* layout_label = gtk_label_new_with_mnemonic(_("_Panel gap:"));
	gtk_widget_set_halign(layout_label, GTK_ALIGN_START);
	gtk_grid_attach(layout_table, layout_label, 0, 0, 1, 1);

	m_panel_gap = gtk_spin_button_new_with_range(0, 50, 1);
	gtk_widget_set_halign(m_panel_gap, GTK_ALIGN_START);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_panel_gap), m_settings->panel_gap);
	gtk_grid_attach(layout_table, m_panel_gap, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(layout_label), m_panel_gap);

	connect(m_panel_gap, "value-changed",
		[this](GtkSpinButton* button)
		{
			m_settings->panel_gap = gtk_spin_button_get_value_as_int(button);
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Layout mode
	layout_label = gtk_label_new_with_mnemonic(_("_Layout mode:"));
	gtk_widget_set_halign(layout_label, GTK_ALIGN_START);
	gtk_grid_attach(layout_table, layout_label, 0, 1, 1, 1);

	m_layout_mode_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_layout_mode_combo), "docked", _("Docked"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_layout_mode_combo), "fullscreen", _("FullScreen"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_layout_mode_combo),
		static_cast<const gchar*>(m_settings->layout_mode));
	gtk_grid_attach(layout_table, m_layout_mode_combo, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(layout_label), m_layout_mode_combo);

	m_grid_auto_size = nullptr;
	m_grid_columns = nullptr;
	m_grid_rows = nullptr;

	// Grid section
	m_grid_section = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(m_grid_section), 12);
	gtk_grid_set_row_spacing(GTK_GRID(m_grid_section), 6);
	gtk_widget_set_margin_top(m_grid_section, 4);
	gtk_grid_attach(layout_table, m_grid_section, 0, 2, 2, 1);

	// Grid density
	GtkWidget* grid_label = gtk_label_new_with_mnemonic(_("Grid _density:"));
	gtk_widget_set_halign(grid_label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(m_grid_section), grid_label, 0, 0, 1, 1);

	m_grid_density_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "low", _("Low"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "medium", _("Medium"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "high", _("High"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_grid_density_combo),
		static_cast<const gchar*>(m_settings->grid_density));
	gtk_grid_attach(GTK_GRID(m_grid_section), m_grid_density_combo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(grid_label), m_grid_density_combo);

	connect(m_grid_density_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val)
			{
				m_settings->grid_density = val;
				m_plugin->reload_menu();
				refresh_customized_indicator();
			}
		});

	// Keep layout mode and grid control sensitivity in sync
	connect(m_layout_mode_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val)
			{
				m_settings->layout_mode = val;
				update_grid_controls_state();
				m_plugin->reload_menu();
				refresh_customized_indicator();
			}
		});

	update_grid_controls_state();


	// Create command buttons section
	GtkBox* command_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	GtkWidget* command_frame = make_aligned_frame(_("Session Commands"), GTK_WIDGET(command_vbox));
	gtk_box_pack_start(page, command_frame, false, false, 0);

	// Add option to show confirmation dialogs
	m_confirm_session_command = gtk_check_button_new_with_mnemonic(_("Show c_onfirmation dialog"));
	gtk_box_pack_start(command_vbox, m_confirm_session_command, true, true, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_confirm_session_command), m_settings->confirm_session_command);

	connect(m_confirm_session_command, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->confirm_session_command = gtk_toggle_button_get_active(button);
		});

	return GTK_WIDGET(page);
}

//-----------------------------------------------------------------------------

GtkWidget* SettingsDialog::init_commands_tab()
{
	// Create commands page
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);
	GtkSizeGroup* label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	// Add command entries
	for (auto command : m_settings->command)
	{
		CommandEdit* command_edit = new CommandEdit(command, label_size_group);
		gtk_box_pack_start(page, command_edit->get_widget(), false, false, 0);
		m_commands.push_back(command_edit);
	}

	return GTK_WIDGET(page);
}

//-----------------------------------------------------------------------------

GtkWidget* SettingsDialog::init_search_actions_tab()
{
	// Create search actions page
	GtkGrid* page = GTK_GRID(gtk_grid_new());
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);
	gtk_grid_set_column_spacing(page, 6);
	gtk_grid_set_row_spacing(page, 6);

	// Create model
	m_actions_model = gtk_list_store_new(N_COLUMNS,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);
	for (auto action : m_settings->search_actions)
	{
		gtk_list_store_insert_with_values(m_actions_model,
				nullptr, G_MAXINT,
				COLUMN_NAME, action->get_name(),
				COLUMN_PATTERN, action->get_pattern(),
				COLUMN_ACTION, action,
				-1);
	}

	// Create view
	m_actions_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_actions_model)));

	connect(m_actions_view, "cursor-changed",
		[this](GtkTreeView*)
		{
			SearchAction* action = get_selected_action();
			if (action)
			{
				gtk_entry_set_text(GTK_ENTRY(m_action_name), action->get_name());
				gtk_entry_set_text(GTK_ENTRY(m_action_pattern), action->get_pattern());
				gtk_entry_set_text(GTK_ENTRY(m_action_command), action->get_command());
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_action_regex), action->get_is_regex());
			}
		});

	GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Name"),
			renderer, "text", COLUMN_NAME, nullptr);
	gtk_tree_view_append_column(m_actions_view, column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Pattern"),
			renderer, "text", COLUMN_PATTERN, nullptr);
	gtk_tree_view_append_column(m_actions_view, column);

	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_actions_view);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	GtkWidget* scrolled_window = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(m_actions_view));
	gtk_widget_set_hexpand(GTK_WIDGET(scrolled_window), true);
	gtk_widget_set_vexpand(GTK_WIDGET(scrolled_window), true);
	gtk_grid_attach(page, scrolled_window, 0, 0, 1, 1);

	// Create buttons
	m_action_add = gtk_button_new();
	gtk_widget_set_tooltip_text(m_action_add, _("Add action"));

	GtkWidget* image = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(m_action_add), image);

	connect(m_action_add, "clicked",
		[this](GtkButton*)
		{
			add_action();
		});

	m_action_remove = gtk_button_new();
	gtk_widget_set_tooltip_text(m_action_remove, _("Remove selected action"));

	image = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(m_action_remove), image);

	connect(m_action_remove, "clicked",
		[this](GtkButton*)
		{
			remove_action();
		});

	GtkBox* actions_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	gtk_widget_set_halign(GTK_WIDGET(actions_box), GTK_ALIGN_START);
	gtk_box_pack_start(actions_box, m_action_add, false, false, 0);
	gtk_box_pack_start(actions_box, m_action_remove, false, false, 0);
	gtk_grid_attach(page, GTK_WIDGET(actions_box), 1, 0, 1, 1);

	// Create details section
	GtkGrid* details_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(details_table, 12);
	gtk_grid_set_row_spacing(details_table, 6);
	GtkWidget* details_frame = make_aligned_frame(_("Details"), GTK_WIDGET(details_table));
	gtk_grid_attach(page, details_frame, 0, 1, 2, 1);

	// Create entry for name
	GtkWidget* label = gtk_label_new_with_mnemonic(_("Nam_e:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(details_table, label, 0, 0, 1, 1);

	m_action_name = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_action_name);
	gtk_widget_set_hexpand(m_action_name, true);
	gtk_grid_attach(details_table, m_action_name, 1, 0, 1, 1);

	connect(m_action_name, "changed",
		[this](GtkEditable* editable)
		{
			GtkTreeIter iter;
			SearchAction* action = get_selected_action(&iter);
			if (action)
			{
				const gchar* text = gtk_entry_get_text(GTK_ENTRY(editable));
				action->set_name(text);
				gtk_list_store_set(m_actions_model, &iter, COLUMN_NAME, text, -1);
			}
		});

	// Create entry for keyword
	label = gtk_label_new_with_mnemonic(_("_Pattern:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(details_table, label, 0, 1, 1, 1);

	m_action_pattern = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_action_pattern);
	gtk_grid_attach(details_table, m_action_pattern, 1, 1, 1, 1);

	connect(m_action_pattern, "changed",
		[this](GtkEditable* editable)
		{
			GtkTreeIter iter;
			SearchAction* action = get_selected_action(&iter);
			if (action)
			{
				const gchar* text = gtk_entry_get_text(GTK_ENTRY(editable));
				action->set_pattern(text);
				gtk_list_store_set(m_actions_model, &iter, COLUMN_PATTERN, text, -1);
			}
		});

	// Create entry for command
	label = gtk_label_new_with_mnemonic(_("C_ommand:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(details_table, label, 0, 2, 1, 1);

	m_action_command = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), m_action_command);
	gtk_grid_attach(details_table, m_action_command, 1, 2, 1, 1);

	connect(m_action_command, "changed",
		[this](GtkEditable* editable)
		{
			SearchAction* action = get_selected_action();
			if (action)
			{
				action->set_command(gtk_entry_get_text(GTK_ENTRY(editable)));
			}
		});

	// Create toggle button for regular expressions
	m_action_regex = gtk_check_button_new_with_mnemonic(_("_Regular expression"));
	gtk_grid_attach(details_table, m_action_regex, 1, 3, 1, 1);

	connect(m_action_regex, "toggled",
		[this](GtkToggleButton* button)
		{
			SearchAction* action = get_selected_action();
			if (action)
			{
				action->set_is_regex(gtk_toggle_button_get_active(button));
			}
		});

	// Select first action
	if (!m_settings->search_actions.empty())
	{
		GtkTreePath* path = gtk_tree_path_new_first();
		gtk_tree_view_set_cursor(m_actions_view, path, nullptr, false);
		gtk_tree_path_free(path);
	}
	else
	{
		gtk_widget_set_sensitive(m_action_remove, false);
		gtk_widget_set_sensitive(m_action_name, false);
		gtk_widget_set_sensitive(m_action_pattern, false);
		gtk_widget_set_sensitive(m_action_command, false);
		gtk_widget_set_sensitive(m_action_regex, false);
	}

	return GTK_WIDGET(page);
}

//-----------------------------------------------------------------------------

GtkWidget* SettingsDialog::init_search_tab()
{
	// Create search ranking page
	GtkBox* vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

	// Fuzzy Search section — switch and spinbutton on one row
	{
		GtkBox* row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
		gtk_widget_set_margin_bottom(GTK_WIDGET(row), 2);

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
				m_settings->fuzzy_enabled = gtk_switch_get_active(GTK_SWITCH(obj));
				gtk_widget_set_sensitive(m_fuzzy_threshold,
				                        gtk_switch_get_active(GTK_SWITCH(obj)));
			});

		gtk_box_pack_start(row,
		    gtk_separator_new(GTK_ORIENTATION_VERTICAL), false, false, 4);

		GtkWidget* lbl_errors = gtk_label_new_with_mnemonic(_("Max _errors (0=auto):"));
		gtk_widget_set_halign(lbl_errors, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_errors, GTK_ALIGN_CENTER);
		gtk_box_pack_start(row, lbl_errors, false, false, 0);

		m_fuzzy_threshold = gtk_spin_button_new_with_range(0, 2, 1);
		gtk_widget_set_halign(m_fuzzy_threshold, GTK_ALIGN_START);
		gtk_widget_set_valign(m_fuzzy_threshold, GTK_ALIGN_CENTER);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_fuzzy_threshold),
		                          static_cast<double>(static_cast<int>(m_settings->fuzzy_threshold)));
		gtk_widget_set_sensitive(m_fuzzy_threshold,
		                         static_cast<bool>(m_settings->fuzzy_enabled));
		gtk_box_pack_start(row, m_fuzzy_threshold, false, false, 0);
		gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_errors), m_fuzzy_threshold);

		connect(m_fuzzy_threshold, "value-changed",
			[this](GtkSpinButton* btn)
			{
				m_settings->fuzzy_threshold = gtk_spin_button_get_value_as_int(btn);
			});

		gtk_box_pack_start(vbox,
		    make_info_frame(_("Fuzzy Search"), GTK_WIDGET(row),
		        _("Finds apps even when you mistype a word.\n"
		          "Example: \"firfox\" still finds Firefox.\n"
		          "Max errors 0 = automatic (1 for short queries, 2 for longer ones).")),
		    false, false, 0);
	}

	// Usage Boost section — boost switch + level combo on one row; recency on its own row
	{
		GtkGrid* grid = GTK_GRID(gtk_grid_new());
		gtk_grid_set_column_spacing(grid, 12);
		gtk_grid_set_row_spacing(grid, 6);

		// Row 0: boost favorites switch and level combo affiancati
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
				m_settings->favorites_boost_enabled = gtk_switch_get_active(GTK_SWITCH(obj));
				gtk_widget_set_sensitive(m_favorites_boost_level,
				                        gtk_switch_get_active(GTK_SWITCH(obj)));
			});

		gtk_box_pack_start(boost_row,
		    gtk_separator_new(GTK_ORIENTATION_VERTICAL), false, false, 4);

		GtkWidget* lbl_level = gtk_label_new_with_mnemonic(_("Boost _level:"));
		gtk_widget_set_halign(lbl_level, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_level, GTK_ALIGN_CENTER);
		gtk_box_pack_start(boost_row, lbl_level, false, false, 0);

		m_favorites_boost_level = gtk_combo_box_text_new();
		gtk_widget_set_halign(m_favorites_boost_level, GTK_ALIGN_START);
		gtk_widget_set_valign(m_favorites_boost_level, GTK_ALIGN_CENTER);
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

		// Row 1: recency weight slider
		GtkWidget* lbl_recency = gtk_label_new_with_mnemonic(_("Recency _weight (%):"));
		gtk_widget_set_halign(lbl_recency, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl_recency, GTK_ALIGN_CENTER);
		gtk_grid_attach(grid, lbl_recency, 0, 1, 1, 1);

		m_frecency_alpha = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
		gtk_widget_set_hexpand(m_frecency_alpha, true);
		gtk_scale_set_value_pos(GTK_SCALE(m_frecency_alpha), GTK_POS_RIGHT);
		gtk_range_set_value(GTK_RANGE(m_frecency_alpha),
		                    static_cast<double>(static_cast<int>(m_settings->frecency_alpha)));
		gtk_grid_attach(grid, m_frecency_alpha, 1, 1, 1, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_recency), m_frecency_alpha);

		connect(m_frecency_alpha, "value-changed",
			[this](GtkRange* range)
			{
				m_settings->frecency_alpha = static_cast<int>(gtk_range_get_value(range));
			});

		// Row 2: small always-visible hint for recency weight
		GtkWidget* recency_hint = gtk_label_new(
		    _("Higher = more weight to recently launched apps; "
		      "lower = more weight to launch frequency."));
		gtk_label_set_line_wrap(GTK_LABEL(recency_hint), true);
		gtk_widget_set_halign(recency_hint, GTK_ALIGN_START);
		PangoAttrList* attrs = pango_attr_list_new();
		pango_attr_list_insert(attrs, pango_attr_scale_new(PANGO_SCALE_SMALL));
		gtk_label_set_attributes(GTK_LABEL(recency_hint), attrs);
		pango_attr_list_unref(attrs);
		gtk_grid_attach(grid, recency_hint, 0, 2, 2, 1);

		gtk_box_pack_start(vbox,
		    make_info_frame(_("Usage Boost"), GTK_WIDGET(grid),
		        _("Promotes apps you use frequently or marked as favorites.\n"
		          "Favorites always appear before non-favorites at equal relevance.\n"
		          "Recency weight controls the balance between how recently vs.\n"
		          "how often you launched an app.")),
		    false, false, 0);
	}

	// Application Aliases section
	{
		enum { ALIAS_COL_NAME, ALIAS_COL_TERMS, ALIAS_COL_ID, ALIAS_N_COLS };

		m_aliases_model = gtk_list_store_new(ALIAS_N_COLS,
		                                     G_TYPE_STRING,
		                                     G_TYPE_STRING,
		                                     G_TYPE_STRING);

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

		gtk_box_pack_start(vbox,
		    make_aligned_frame(_("Application Aliases"), GTK_WIDGET(alias_vbox)),
		    true, true, 0);
	}

	// Reset to defaults button
	GtkWidget* reset_button = gtk_button_new_with_mnemonic(_("_Reset to Defaults"));
	gtk_widget_set_halign(reset_button, GTK_ALIGN_END);
	gtk_widget_set_margin_top(reset_button, 6);
	gtk_box_pack_start(vbox, reset_button, false, false, 0);

	connect(reset_button, "clicked",
		[this](GtkButton*)
		{
			m_settings->fuzzy_enabled = true;
			m_settings->fuzzy_threshold = 0;
			m_settings->favorites_boost_enabled = true;
			m_settings->favorites_boost_level = 2;
			m_settings->frecency_alpha = 70;

			gtk_switch_set_active(GTK_SWITCH(m_fuzzy_enabled), true);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_fuzzy_threshold), 0);
			gtk_switch_set_active(GTK_SWITCH(m_favorites_boost_enabled), true);
			gtk_combo_box_set_active(GTK_COMBO_BOX(m_favorites_boost_level), 1);
			gtk_range_set_value(GTK_RANGE(m_frecency_alpha), 70.0);

			// Clear all aliases: iterate model rows and wipe each entry
			enum { ALIAS_COL_NAME, ALIAS_COL_TERMS, ALIAS_COL_ID, ALIAS_N_COLS };
			GtkTreeIter iter;
			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_aliases_model), &iter))
			{
				do
				{
					gchar* id_val = nullptr;
					gtk_tree_model_get(GTK_TREE_MODEL(m_aliases_model), &iter,
					                   ALIAS_COL_ID, &id_val, -1);
					if (id_val)
					{
						m_settings->set_aliases(std::string(id_val), {});
						g_free(id_val);
					}
				} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_aliases_model), &iter));
			}
			gtk_list_store_clear(m_aliases_model);
			gtk_widget_set_sensitive(m_alias_remove, false);
		});

	return GTK_WIDGET(vbox);
}

//-----------------------------------------------------------------------------
