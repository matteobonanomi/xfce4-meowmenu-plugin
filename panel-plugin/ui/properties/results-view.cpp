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

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* init_app_grid_tab:
 *
 * Builds the App Grid tab in the 003-properties-refactor 5-tab dictionary.
 * Sections (top-to-bottom, the documented behavior):
 *   1. View              — Show applications as (Icons / List / Tree) icon-radios.
 *   2. Layout            — Grid density, application icon size, show flags.
 *   3. Opacity           — App box opacity (enable-when-docked).
 *
 * Sub-enable rules per view-mode (the documented behavior / the documented behavior): grid-density and
 * launcher-icon-size are sensitive only when view-mode == icons;
 * launcher-show-description only when view-mode == list.
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_app_grid_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	// =========================================================================
	// 1. View section — three exclusive icon-buttons (the documented behavior).
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
	// 2. Layout section — grid density, icon size, show flags (the documented behavior).
	// =========================================================================
	// density C1 / icon-size C2; generic-names C1 / tooltips C2; descriptions C1
	// with C2 left empty.
	GtkWidget* layout_grid = make_two_column_section();
	GtkWidget* layout_frame = make_aligned_frame(_("Layout"), layout_grid);
	gtk_box_pack_start(page, layout_frame, false, false, 0);

	// Grid density (icons-only sub-enable)
	GtkWidget* density_label = gtk_label_new_with_mnemonic(_("Grid _density:"));
	m_grid_density_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "low", _("Low"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "medium", _("Medium"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_grid_density_combo), "high", _("High"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_grid_density_combo),
		static_cast<const gchar*>(m_settings->grid_density));
	add_form_row(layout_grid, COLUMN_C1, 0, density_label, m_grid_density_combo, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(density_label), m_grid_density_combo);

	connect(m_grid_density_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			m_settings->grid_density = val;
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// Application icon size (icons-only sub-enable)
	GtkWidget* icon_size_label = gtk_label_new_with_mnemonic(_("Application icon si_ze:"));
	m_item_icon_size = gtk_combo_box_text_new();
	const auto icon_sizes = IconSize::get_strings();
	for (const auto& icon_size : icon_sizes)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_item_icon_size), icon_size.c_str());
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_item_icon_size), m_settings->launcher_icon_size + 1);
	add_form_row(layout_grid, COLUMN_C2, 0, icon_size_label, m_item_icon_size, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(icon_size_label), m_item_icon_size);

	connect(m_item_icon_size, "changed",
		[this](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			m_settings->launcher_icon_size = gtk_combo_box_get_active(combo) - 1;
			m_plugin->refresh_layout();
		});

	// NOTE: launcher_show_name stores "show the real (non-generic) name"; the
	// checkbox is therefore inverted — checked means "Show generic" (false).
	m_show_generic_names = gtk_check_button_new_with_mnemonic(_("Show generic application _names"));
	add_form_row(layout_grid, COLUMN_C1, 1, nullptr, m_show_generic_names, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_generic_names), !m_settings->launcher_show_name);

	connect(m_show_generic_names, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_name = !gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	m_show_tooltips = gtk_check_button_new_with_mnemonic(_("Show application too_ltips"));
	add_form_row(layout_grid, COLUMN_C2, 1, nullptr, m_show_tooltips, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_tooltips), m_settings->launcher_show_tooltip);

	connect(m_show_tooltips, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_tooltip = gtk_toggle_button_get_active(button);
			m_plugin->refresh_layout();
		});

	// Show descriptions — list-only sub-enable. C1 with C2 left empty (the documented behavior).
	m_show_descriptions = gtk_check_button_new_with_mnemonic(_("Show application _descriptions"));
	add_form_row(layout_grid, COLUMN_C1, 2, nullptr, m_show_descriptions, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_show_descriptions), m_settings->launcher_show_description);

	connect(m_show_descriptions, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->launcher_show_description = gtk_toggle_button_get_active(button);
			m_plugin->reload_menu();
		});

	m_transparent_grid = gtk_check_button_new_with_mnemonic(_("Transparent grid"));
	add_form_row(layout_grid, COLUMN_C2, 2, nullptr, m_transparent_grid, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_transparent_grid), m_settings->transparent_grid);

	connect(m_transparent_grid, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->transparent_grid = gtk_toggle_button_get_active(button);
			m_plugin->refresh_layout();
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
		gtk_widget_set_sensitive(m_transparent_grid, is_icons);
		gtk_widget_set_sensitive(m_show_descriptions, is_list);
	};
	apply_view_mode_sub_enables();

	connect(m_show_as_icons, "toggled",
		[this, apply_view_mode_sub_enables](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
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
			if (m_programmatic_update)
				return;
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
			if (m_programmatic_update)
				return;
			if (!gtk_toggle_button_get_active(button))
				return;
			m_settings->view_mode = Settings::ViewAsTree;
			apply_view_mode_sub_enables();
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	return wrap_in_scrolled(GTK_WIDGET(page));
}
