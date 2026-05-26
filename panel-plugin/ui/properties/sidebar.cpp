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

#include "icon-size.h"
#include "plugin.h"
#include "settings.h"
#include "slot.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

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
