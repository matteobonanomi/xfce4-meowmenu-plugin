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

#include "settings.h"
#include "ui/slot.h"

#include <vector>

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

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
	m_places_enabled_switch = enable_switch;
	GtkWidget* enable_label = gtk_label_new_with_mnemonic(_("Enable _Places"));
	gtk_widget_set_halign(enable_label, GTK_ALIGN_START);
	gtk_widget_set_hexpand(enable_label, true);
	gtk_switch_set_active(GTK_SWITCH(enable_switch), m_settings->places_enabled);
	gtk_grid_attach(enable_grid, enable_label, 0, 0, 1, 1);
	gtk_grid_attach(enable_grid, enable_switch, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(enable_label), enable_switch);

	// "Show icons" — renders the Apps/Places switch as two themed icon buttons
	// instead of text (FR-001). Bound to /places/switch-show-icons with the
	// binding's reset-to-default; greyed (forced ON, value unchanged) when the
	// sidebar is on Top/Bottom or disabled (FR-015/018).
	GtkWidget* show_icons_switch = gtk_switch_new();
	GtkWidget* show_icons_label = gtk_label_new_with_mnemonic(_("Show _icons"));
	gtk_widget_set_halign(show_icons_label, GTK_ALIGN_START);
	gtk_widget_set_hexpand(show_icons_label, true);
	gtk_switch_set_active(GTK_SWITCH(show_icons_switch), m_settings->places_switch_show_icons);
	gtk_grid_attach(enable_grid, show_icons_label, 0, 1, 1, 1);
	gtk_grid_attach(enable_grid, show_icons_switch, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(show_icons_label), show_icons_switch);
	m_places_switch_show_icons = show_icons_switch;

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

	// Sensitivity helpers (FR-037, FR-038).
	auto refresh_sensitivity = [=]()
	{
		const bool enabled = gtk_switch_get_active(GTK_SWITCH(enable_switch));
		for (GtkWidget* w : places_dependents)
			gtk_widget_set_sensitive(w, enabled);
		const bool fav_enabled = enabled && gtk_switch_get_active(GTK_SWITCH(fav_switch));
		gtk_widget_set_sensitive(sync_combo, fav_enabled);
		gtk_widget_set_sensitive(sync_label, fav_enabled);

		// "Show icons" is forced ON (greyed, value unchanged) when the sidebar
		// is on Top/Bottom or disabled (FR-015/018); it is also moot when
		// Places is off. The stored value is never rewritten here.
		const gchar* sp = static_cast<const gchar*>(m_settings->sidebar_position);
		const bool strip = sp && (g_strcmp0(sp, "top") == 0 || g_strcmp0(sp, "bottom") == 0);
		const bool forced = strip || !m_settings->sidebar_enabled;
		gtk_widget_set_sensitive(show_icons_switch, enabled && !forced);
		gtk_widget_set_sensitive(show_icons_label, enabled && !forced);
	};
	refresh_sensitivity();

	// Expose this tab's sensitivity recompute so sync_preset_widgets() can
	// re-evaluate the Places greying after a preset switch (FR-002). This also
	// re-runs the "Show icons" forced-ON rule when a preset changes the sidebar
	// position from another tab.
	m_places_refresh_sensitivity = refresh_sensitivity;

	// Signal wiring
	connect(enable_switch, "state-set",
		[this, refresh_sensitivity](GtkSwitch*, gboolean state) -> gboolean
		{
			if (m_programmatic_update)
				return FALSE;
			m_settings->places_enabled = state;
			refresh_sensitivity();
			return FALSE; // let the switch update its visual state
		});
	connect(show_icons_switch, "state-set",
		[this](GtkSwitch*, gboolean state) -> gboolean
		{
			if (m_programmatic_update)
				return FALSE;
			// Stored intent only; the switch re-renders on the next menu open
			// when update_layout() reads the new value (FR-029, render-time).
			m_settings->places_switch_show_icons = state;
			return FALSE;
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

	return wrap_in_scrolled(GTK_WIDGET(page));
}
