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

#include "plugin.h"
#include "settings.h"
#include "slot.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* init_search_bar_tab:
 *
 * Builds the Search Bar tab of the Properties dialog. Sections, top-to-bottom:
 *   1. Position        — search-bar-position combo.
 *   2. Ranking         — fuzzy matching, favorites boost, recency weight
 *                        (lifted from the legacy "Advanced Search" tab).
 *   3. Aliases         — built by build_search_bar_aliases_section().
 *   4. Search actions  — built by build_search_bar_actions_section().
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

	// 3 + 4: aliases and search-actions live in their own translation units
	// so this tab builder stays focused on the position + ranking widgets.
	build_search_bar_aliases_section(page);
	build_search_bar_actions_section(page);

	return wrap_in_scrolled(GTK_WIDGET(page));
}
