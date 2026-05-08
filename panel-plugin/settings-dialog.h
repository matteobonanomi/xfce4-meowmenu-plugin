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

#ifndef WHISKERMENU_SETTINGS_DIALOG_H
#define WHISKERMENU_SETTINGS_DIALOG_H

#include <string>
#include <vector>

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class CommandEdit;
class Plugin;
class SearchAction;
class Settings;

class SettingsDialog
{
public:
	SettingsDialog(Settings* settings, Plugin* plugin);
	~SettingsDialog();

	SettingsDialog(const SettingsDialog&) = delete;
	SettingsDialog(SettingsDialog&&) = delete;
	SettingsDialog& operator=(const SettingsDialog&) = delete;
	SettingsDialog& operator=(SettingsDialog&&) = delete;

	GtkWidget* get_widget() const
	{
		return m_window;
	}

private:
	void choose_icon();

	SearchAction* get_selected_action(GtkTreeIter* iter = nullptr) const;
	void add_action();
	void remove_action();

	void response(int response_id);
	void refresh_customized_indicator();
	void refresh_preset_combo(const std::string& select_id = {});
	void sync_preset_widgets();
	void update_grid_controls_state();

	GtkWidget* init_general_tab();
	GtkWidget* init_appearance_tab();
	GtkWidget* init_behavior_tab();
	GtkWidget* init_commands_tab();
	GtkWidget* init_search_actions_tab();
	GtkWidget* init_search_tab();

private:
	Settings* const m_settings;
	Plugin* m_plugin;

	GtkWidget* m_window;

	// Appearance
	GtkWidget* m_show_as_icons;
	GtkWidget* m_show_as_list;
	GtkWidget* m_show_as_tree;
	GtkWidget* m_show_generic_names;
	GtkWidget* m_show_category_names;
	GtkWidget* m_show_descriptions;
	GtkWidget* m_show_tooltips;
	GtkWidget* m_category_icon_size;
	GtkWidget* m_item_icon_size;

	// Preset hub
	GtkWidget* m_preset_combo;
	GtkWidget* m_preset_description;
	GtkWidget* m_preset_customized;
	GtkWidget* m_preset_rename_btn;
	GtkWidget* m_preset_delete_btn;
	GtkWidget* m_preset_export_btn;
	bool m_loading_preset = false;

	// Tracks Xfconf "property-changed" subscription that mirrors live
	// menu_width/menu_height updates (e.g. drag-resize) into the spin buttons.
	gulong m_size_change_slot = 0;

	// Appearance customization (T070)
	GtkWidget* m_corner_radius;
	GtkWidget* m_categories_opacity;
	GtkWidget* m_apps_opacity;
	GtkWidget* m_sidebar_position_combo;
	GtkWidget* m_search_bar_position_combo;
	GtkWidget* m_profile_position_combo;
	GtkWidget* m_commands_position_combo;

	// Behavior layout (T071)
	GtkWidget* m_panel_gap;
	GtkWidget* m_layout_mode_combo;
	GtkWidget* m_grid_auto_size;
	GtkWidget* m_grid_columns;
	GtkWidget* m_grid_rows;
	GtkWidget* m_grid_density_combo;
	GtkWidget* m_grid_section;

	// Layout
	GtkWidget* m_position_categories_horizontal;
	GtkWidget* m_profile_shape;
	GtkWidget* m_menu_width;
	GtkWidget* m_menu_height;

	// Panel Button
	GtkWidget* m_button_style;
	GtkWidget* m_title;
	GtkWidget* m_icon;
	GtkWidget* m_icon_button;
	GtkWidget* m_button_single_row;

	// Behavior
	GtkWidget* m_hover_switch_category;
	GtkWidget* m_stay_on_focus_out;
	GtkWidget* m_sort_categories;

	// Default Display
	GtkWidget* m_display_favorites;
	GtkWidget* m_display_recent;
	GtkWidget* m_display_applications;

	// Recently Used
	GtkWidget* m_remember_favorites;
	GtkWidget* m_recent_items_max;

	// Session Commands
	GtkWidget* m_confirm_session_command;

	std::vector<CommandEdit*> m_commands;

	// Search Actions
	GtkTreeView* m_actions_view;
	GtkListStore* m_actions_model;
	GtkWidget* m_action_add;
	GtkWidget* m_action_remove;
	GtkWidget* m_action_name;
	GtkWidget* m_action_pattern;
	GtkWidget* m_action_command;
	GtkWidget* m_action_regex;

	// Search Ranking 2.0 tab
	GtkWidget* m_fuzzy_enabled;
	GtkWidget* m_fuzzy_threshold;
	GtkWidget* m_favorites_boost_enabled;
	GtkWidget* m_favorites_boost_level;
	GtkWidget* m_frecency_alpha;
	GtkTreeView* m_aliases_view;
	GtkListStore* m_aliases_model;
	GtkWidget* m_alias_add;
	GtkWidget* m_alias_remove;
};

}

#endif // WHISKERMENU_SETTINGS_DIALOG_H
