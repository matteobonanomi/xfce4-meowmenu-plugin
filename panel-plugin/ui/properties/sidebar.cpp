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

#include "ui/properties/common.h"

#include "ui/icon-size.h"
#include "core/plugin.h"
#include "settings.h"
#include "ui/slot.h"

#include <libxfce4ui/libxfce4ui.h>

#include <memory>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* init_sidebar_tab:
 *
 * Builds the Sidebar tab in the 003-properties-refactor 5-tab dictionary.
 * Sections (top-to-bottom, supported behavior):
 *   1. Visuals      — show category names, icon size, sidebar opacity.
 *   2. Position     — sidebar position (Left/Right/Horizontal).
 *   3. Behavior     — hover-switch, sort categories, default category.
 *   4. Recently used — max items, include favorites.
 *
 * Sub-enable rule for category-show-name: greyed when sidebar-position
 * is not one of {left, right} (behavior table §SidebarLeftRight).
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_sidebar_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	// =========================================================================
	// 0. Enable sidebar (supported behavior)
	// =========================================================================
	// "Enable sidebar" OFF removes the category sidebar entirely; the
	// Apps/Places switch relocates into the search-bar row. Bound to
	// /sidebar-enabled with the binding's reset-to-default. When OFF, every
	// other Sidebar section is greyed except the Default category section. The
	// switch sits in C1 with C2 left empty.
	GtkWidget* enable_grid = make_two_column_section();
	gtk_box_pack_start(page, make_aligned_frame(_("Sidebar"), enable_grid), false, false, 0);

	m_enable_sidebar_switch = make_form_switch();
	GtkWidget* enable_sidebar_switch = m_enable_sidebar_switch;
	GtkWidget* enable_sidebar_label = gtk_label_new_with_mnemonic(_("_Enable sidebar"));
	gtk_switch_set_active(GTK_SWITCH(enable_sidebar_switch), m_settings->sidebar_enabled);
	add_form_row(enable_grid, COLUMN_C1, 0, enable_sidebar_label, enable_sidebar_switch, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(enable_sidebar_label), enable_sidebar_switch);

	// Section frames greyed when the sidebar is disabled (supported behavior). Populated
	// as each section frame is created below. Held in a shared_ptr so the
	// sensitivity lambda (connected to signals that outlive this function)
	// observes every frame appended after it is defined, not just the frames
	// present at capture time.
	auto sidebar_section_frames = std::make_shared<std::vector<GtkWidget*>>();

	// =========================================================================
	// 1. Visuals section (supported behavior)
	// =========================================================================
	// Show category names C1 / icon size C2 share a row; the opacity slider
	// spans the full section width below the grid (Full), packed into the
	// section vbox so it has no phantom empty C2.
	GtkWidget* visuals_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget* visuals_grid = make_two_column_section();
	gtk_box_pack_start(GTK_BOX(visuals_content), visuals_grid, false, false, 0);

	GtkWidget* visuals_frame = make_aligned_frame(_("Visuals"), visuals_content);
	gtk_box_pack_start(page, visuals_frame, false, false, 0);
	sidebar_section_frames->push_back(visuals_frame);

	m_show_category_names = gtk_check_button_new_with_mnemonic(_("Show cate_gory names"));
	add_form_row(visuals_grid, COLUMN_C1, 0, nullptr, m_show_category_names, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_category_names),
		m_settings->category_show_name);

	connect(m_show_category_names, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->category_show_name = gtk_toggle_button_get_active(button);
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// Category icon size
	GtkWidget* cat_size_label = gtk_label_new_with_mnemonic(_("Categ_ory icon size:"));
	m_category_icon_size = gtk_combo_box_text_new();
	const auto cat_icon_sizes = IconSize::get_strings();
	for (const auto& s : cat_icon_sizes)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_category_icon_size), s.c_str());
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_category_icon_size),
		m_settings->category_icon_size + 1);
	add_form_row(visuals_grid, COLUMN_C2, 0, cat_size_label, m_category_icon_size, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(cat_size_label), m_category_icon_size);

	connect(m_category_icon_size, "changed",
		[this](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			m_settings->category_icon_size = gtk_combo_box_get_active(combo) - 1;
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// =========================================================================
	// 2. Position section (supported behavior) — combo in C1, C2 left empty.
	// =========================================================================
	GtkWidget* pos_grid = make_two_column_section();
	GtkWidget* pos_frame = make_aligned_frame(_("Position"), pos_grid);
	gtk_box_pack_start(page, pos_frame, false, false, 0);
	sidebar_section_frames->push_back(pos_frame);

	GtkWidget* side_pos_label = gtk_label_new_with_mnemonic(_("Sidebar _position:"));
	m_sidebar_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "left", _("Left"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo), "right", _("Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_sidebar_position_combo),
			"horizontal", _("Horizontal"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_sidebar_position_combo),
		static_cast<const gchar*>(m_settings->sidebar_position));
	add_form_row(pos_grid, COLUMN_C1, 0, side_pos_label, m_sidebar_position_combo, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(side_pos_label), m_sidebar_position_combo);

	// Sensitivity: when the sidebar is disabled every section is greyed except
	// the Default category section (supported behavior). When enabled, "Show category
	// names" is greyed for Horizontal because the strip is icon-only;
	// greying never changes the stored value (supported behavior).
	auto apply_sidebar_sub_enable = [this, sidebar_section_frames]()
	{
		const bool enabled = m_settings->sidebar_enabled;
		for (GtkWidget* frame : *sidebar_section_frames)
			gtk_widget_set_sensitive(frame, enabled);

		const gchar* p = static_cast<const gchar*>(m_settings->sidebar_position);
		const bool lr = p && (g_strcmp0(p, "left") == 0 || g_strcmp0(p, "right") == 0);
		gtk_widget_set_sensitive(m_show_category_names, enabled && lr);
	};

	// Expose this tab's sub-enable recompute so sync_preset_widgets() can refresh
	// the Sidebar greying after a preset switch (supported behavior).
	m_sidebar_apply_sub_enable = apply_sidebar_sub_enable;

	connect(enable_sidebar_switch, "state-set",
		[this, apply_sidebar_sub_enable](GtkSwitch*, gboolean state) -> gboolean
		{
			if (m_programmatic_update)
				return FALSE;
			m_settings->sidebar_enabled = state;
			apply_sidebar_sub_enable();
			if (m_places_refresh_sensitivity)
				m_places_refresh_sensitivity();
			m_plugin->refresh_layout();
			refresh_customized_indicator();
			return FALSE;
		});

	connect(m_sidebar_position_combo, "changed",
		[this, apply_sidebar_sub_enable](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			m_settings->sidebar_position = val;
			apply_sidebar_sub_enable();
			if (m_places_refresh_sensitivity)
				m_places_refresh_sensitivity();
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// =========================================================================
	// 3. Behavior section (supported behavior) — hover C1 / sort C2.
	// =========================================================================
	GtkWidget* behavior_grid = make_two_column_section();
	GtkWidget* behavior_frame = make_aligned_frame(_("Behavior"), behavior_grid);
	gtk_box_pack_start(page, behavior_frame, false, false, 0);
	sidebar_section_frames->push_back(behavior_frame);

	m_hover_switch_category = gtk_check_button_new_with_mnemonic(_("Switch categories by _hovering"));
	add_form_row(behavior_grid, COLUMN_C1, 0, nullptr, m_hover_switch_category, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_hover_switch_category),
		m_settings->category_hover_activate);

	connect(m_hover_switch_category, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->category_hover_activate = gtk_toggle_button_get_active(button);
		});

	m_sort_categories = gtk_check_button_new_with_mnemonic(_("Sort ca_tegories"));
	add_form_row(behavior_grid, COLUMN_C2, 0, nullptr, m_sort_categories, false, nullptr);
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
			if (m_programmatic_update)
				return;
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->default_category = Settings::CategoryFavorites;
				m_plugin->refresh_layout();
			}
		});

	connect(m_display_recent, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->default_category = Settings::CategoryRecent;
				m_plugin->refresh_layout();
			}
		});

	connect(m_display_applications, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			if (gtk_toggle_button_get_active(button))
			{
				m_settings->default_category = Settings::CategoryAll;
				m_plugin->refresh_layout();
			}
		});

	// =========================================================================
	// 4. Recently used section (supported behavior) — max items C1 / include favorites C2.
	// =========================================================================
	GtkWidget* recent_grid = make_two_column_section();
	GtkWidget* recent_frame = make_aligned_frame(_("Recently used"), recent_grid);
	gtk_box_pack_start(page, recent_frame, false, false, 0);
	sidebar_section_frames->push_back(recent_frame);

	GtkWidget* max_label = gtk_label_new_with_mnemonic(_("Maximum _items:"));
	m_recent_items_max = gtk_spin_button_new_with_range(0, 100, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_recent_items_max), m_settings->recent_items_max);
	add_form_row(recent_grid, COLUMN_C1, 0, max_label, m_recent_items_max, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(max_label), m_recent_items_max);

	connect(m_recent_items_max, "value-changed",
		[this](GtkSpinButton* button)
		{
			m_settings->recent_items_max = gtk_spin_button_get_value_as_int(button);
			const bool active = m_settings->recent_items_max;
			gtk_widget_set_sensitive(m_display_recent, active);
			if (!active && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_display_recent)))
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_display_favorites), true);
			m_plugin->refresh_layout();
		});

	m_remember_favorites = gtk_check_button_new_with_mnemonic(_("Include _favorites in \"Recent\""));
	add_form_row(recent_grid, COLUMN_C2, 0, nullptr, m_remember_favorites, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_remember_favorites),
		m_settings->favorites_in_recent);

	connect(m_remember_favorites, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->favorites_in_recent = gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	// Initial sensitivity pass: run once every section frame exists so a
	// sidebar that starts disabled greys Behaviour and Recently used too,
	// not only the frames present when the lambda was defined.
	apply_sidebar_sub_enable();

	return wrap_in_scrolled(GTK_WIDGET(page));
}
