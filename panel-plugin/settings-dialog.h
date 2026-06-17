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

#include "core/user-session-layout.h"

#include <functional>
#include <string>
#include <utility>
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
	void edit_search_action_modal(SearchAction* action);

	void response(int response_id);
	void refresh_customized_indicator();
	void refresh_preset_combo(const std::string& select_id = {});
	void sync_preset_widgets();
	void update_grid_controls_state();

	// Per-tab builders for the Properties dialog. Each returns a vertically
	// scrolled container ready to be added to the dialog's stack.
	GtkWidget* init_general_tab();
	GtkWidget* init_user_session_tab();
	GtkWidget* init_search_bar_tab();
	GtkWidget* init_app_grid_tab();
	GtkWidget* init_sidebar_tab();
	GtkWidget* init_places_tab();

	// Sub-section helpers used by init_search_bar_tab(); each appends one
	// frame to @page. Split out of the tab builder to keep its translation
	// unit small and topic-focused.
	void build_search_bar_aliases_section(GtkBox* page);
	void build_search_bar_actions_section(GtkBox* page);

	// Layout-mode-driven live sensitivity (FR-006 / data-model E-4).
	// The five matrix controls (width/height/gap/corner-radius/full-screen
	// opacity) register a (widget, LayoutControl) pair in m_layout_controls;
	// apply_layout_mode_sensitivity() drives them through the pure
	// control_enabled() matrix. The two legacy vectors carry the remaining
	// out-of-matrix per-region opacity controls (sidebar / results-view tabs),
	// which are enabled in both windowed modes and greyed in full-screen. The
	// shared handler in install_layout_mode_handler() refreshes all of them on
	// every layout-mode change.
	void install_layout_mode_handler();
	void apply_layout_mode_sensitivity();
	std::vector<std::pair<GtkWidget*, WhiskerMenu::LayoutControl>> m_layout_controls;
	std::vector<GtkWidget*> m_layout_enable_when_docked;
	std::vector<GtkWidget*> m_layout_enable_when_fullscreen;
	gulong m_layout_mode_slot = 0;

	// Unified-bar (spec 004) — live sensitivity & tooltip rules in
	// contracts/settings-dialog.md. Updated whenever /layout-mode or any
	// of /search-bar-position, /profile-position, /commands-position changes.
	GtkWidget* m_unified_bar = nullptr;
	gulong m_unified_bar_slot = 0;
	void apply_unified_bar_sensitivity();

	// User/Session coupling (feature 027) — per-row greying of the Profile and
	// Commands position combos plus reflecting any persisted auto-snap, driven
	// by the shared normalize_user_session() helper. Refreshed whenever
	// /layout-mode, /search-bar-position, /profile-position or /commands-position
	// changes. The combo widget itself disambiguates Profile from Commands.
	gulong m_user_session_coupling_slot = 0;
	void apply_user_session_coupling();
	void apply_user_session_combo_sensitivity(GtkCellLayout* layout,
			GtkCellRenderer* cell, GtkTreeModel* model, GtkTreeIter* iter);
	// GtkCellLayoutDataFunc trampoline: forwards to the member above (a static
	// member can reach the private combo/settings state the C callback needs).
	static void on_user_session_cell_data(GtkCellLayout* layout,
			GtkCellRenderer* cell, GtkTreeModel* model, GtkTreeIter* iter,
			gpointer data);

private:
	Settings* const m_settings;
	Plugin* m_plugin;

	GtkWidget* m_window;

	// All widget pointers are nullptr-initialized so cross-tab helpers
	// (sync_preset_widgets, refresh_preset_combo) can null-guard widgets
	// that belong to tabs not yet filled in by their owning user story.

	// Appearance
	GtkWidget* m_show_as_icons = nullptr;
	GtkWidget* m_show_as_list = nullptr;
	GtkWidget* m_show_as_tree = nullptr;
	GtkWidget* m_show_generic_names = nullptr;
	GtkWidget* m_show_category_names = nullptr;
	GtkWidget* m_show_descriptions = nullptr;
	GtkWidget* m_show_tooltips = nullptr;
	GtkWidget* m_category_icon_size = nullptr;
	GtkWidget* m_item_icon_size = nullptr;

public:
	// Preset hub. The selector is a model-driven GtkComboBox (not the convenience
	// GtkComboBoxText) so each row can carry its own Pango weight/style: built-ins
	// render bold, saved customs standard, and the transient "Unsaved custom"
	// placeholder italic. m_preset_model is the backing store; its columns are
	// indexed by the PresetCol enum below. The enum is public so the file-local
	// model helpers in settings-dialog.cpp can name the columns.
	enum PresetCol
	{
		PRESET_COL_ID = 0,     // G_TYPE_STRING — selection key (gtk_combo_box_set_active_id)
		PRESET_COL_LABEL,      // G_TYPE_STRING — translated display text
		PRESET_COL_WEIGHT,     // G_TYPE_INT    — PANGO_WEIGHT_* for the row
		PRESET_COL_STYLE,      // G_TYPE_INT    — PANGO_STYLE_* for the row
		PRESET_N_COLS
	};

private:
	GtkWidget* m_preset_combo = nullptr;
	GtkListStore* m_preset_model = nullptr;
	// "?" help control beside the preset selector. Its hover/focus tooltip
	// reveals the active preset's description (set by the preset-sync path); it
	// replaces the former always-visible inline description label. Null-guarded
	// by cross-tab helpers per the note at lines 127-128.
	GtkWidget* m_preset_help = nullptr;
	GtkWidget* m_preset_rename_btn = nullptr;
	GtkWidget* m_preset_delete_btn = nullptr;
	GtkWidget* m_preset_export_btn = nullptr;

	// Dialog-wide "programmatic update in progress" guard. Set for the whole
	// sync_preset_widgets() body (and the combo rebuild) so every widget signal
	// handler early-returns before writing Settings — the cascade of set_active
	// calls during a preset switch cannot write a divergent value back (FR-004).
	bool m_programmatic_update = false;

	// Last preset id applied via the combo. Tracked so re-applying the active
	// preset behaves as "reset to this preset" (FR-006).
	std::string m_last_applied_preset_id;

	// Live sensitivity recompute hooks owned by the Places / Sidebar tab
	// builders. Invoked at the end of sync_preset_widgets() so a preset switch
	// refreshes every dependent control's enabled/greyed state across all tabs
	// without a dialog reopen (FR-002).
	std::function<void()> m_places_refresh_sensitivity;
	std::function<void()> m_sidebar_apply_sub_enable;

	// Switches owned by the Places / Sidebar tabs, driven during preset sync so
	// the "Enable Places" and "Enable sidebar" controls follow the active preset
	// (FR-001/003).
	GtkWidget* m_places_enabled_switch = nullptr;
	GtkWidget* m_enable_sidebar_switch = nullptr;

	// Tracks Xfconf "property-changed" subscription that mirrors live
	// menu_width/menu_height updates (e.g. drag-resize) into the spin buttons.
	gulong m_size_change_slot = 0;

	// Appearance customization (T070)
	GtkWidget* m_corner_radius = nullptr;
	GtkWidget* m_menu_opacity = nullptr;
	GtkWidget* m_sidebar_position_combo = nullptr;
	GtkWidget* m_search_bar_position_combo = nullptr;
	GtkWidget* m_profile_position_combo = nullptr;
	GtkWidget* m_commands_position_combo = nullptr;

	// Behavior layout (T071)
	GtkWidget* m_panel_gap = nullptr;
	GtkWidget* m_layout_mode_combo = nullptr;
	GtkWidget* m_grid_density_combo = nullptr;
	GtkWidget* m_grid_section = nullptr;

	// Layout
	GtkWidget* m_position_categories_horizontal = nullptr;
	GtkWidget* m_profile_shape = nullptr;
	GtkWidget* m_menu_width = nullptr;
	GtkWidget* m_menu_height = nullptr;

	// Panel Button — schema v2 splits visibility into two independent toggles
	// (button-title-visible / button-icon-visible). m_button_style is retained
	// only for the legacy builder; the new General tab does not use it.
	GtkWidget* m_button_style = nullptr;
	GtkWidget* m_button_title_visible = nullptr;
	GtkWidget* m_button_icon_visible = nullptr;
	GtkWidget* m_title = nullptr;
	GtkWidget* m_icon = nullptr;
	GtkWidget* m_icon_button = nullptr;
	GtkWidget* m_button_single_row = nullptr;

	// Places mode (feature 020) — Apps/Places switch "Show icons" toggle.
	// Held for preset sync and for the forced-ON greying applied when the
	// sidebar is on Top/Bottom or disabled.
	GtkWidget* m_places_switch_show_icons = nullptr;

	// Behavior
	GtkWidget* m_hover_switch_category = nullptr;
	GtkWidget* m_stay_on_focus_out = nullptr;
	GtkWidget* m_sort_categories = nullptr;

	// Default Display
	GtkWidget* m_display_favorites = nullptr;
	GtkWidget* m_display_recent = nullptr;
	GtkWidget* m_display_applications = nullptr;

	// Recently Used
	GtkWidget* m_remember_favorites = nullptr;
	GtkWidget* m_recent_items_max = nullptr;

	// Session Commands
	GtkWidget* m_confirm_session_command = nullptr;

	std::vector<CommandEdit*> m_commands;

	// Search Actions
	GtkTreeView* m_actions_view = nullptr;
	GtkListStore* m_actions_model = nullptr;
	GtkWidget* m_action_add = nullptr;
	GtkWidget* m_action_remove = nullptr;
	GtkWidget* m_action_name = nullptr;
	GtkWidget* m_action_pattern = nullptr;
	GtkWidget* m_action_command = nullptr;
	GtkWidget* m_action_regex = nullptr;

	// Search Ranking 2.0 tab
	GtkWidget* m_fuzzy_enabled = nullptr;
	GtkWidget* m_fuzzy_threshold = nullptr;
	GtkWidget* m_favorites_boost_enabled = nullptr;
	GtkWidget* m_favorites_boost_level = nullptr;
	GtkWidget* m_frecency_alpha = nullptr;
	GtkTreeView* m_aliases_view = nullptr;
	GtkListStore* m_aliases_model = nullptr;
	GtkWidget* m_alias_add = nullptr;
	GtkWidget* m_alias_remove = nullptr;
};

}

#endif // WHISKERMENU_SETTINGS_DIALOG_H
