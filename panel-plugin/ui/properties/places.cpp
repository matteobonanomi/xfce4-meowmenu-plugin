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

#include "core/plugin.h"
#include "settings.h"
#include "ui/slot.h"

#include <vector>

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* init_places_tab:
 *
 * Builds the Places panel (current behavior). Eight controls bound directly to
 * the /places-prefixed Xfconf-backed Settings members. Sensitivity is gated by
 * /places/enabled at the panel level and by /places/favourites-enabled for
 * the sync dropdown (supported behavior, supported behavior).
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_places_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	std::vector<GtkWidget*> places_dependents;

	// Places mode section (supported behavior) — Enable Places switch C1 / Show icons C2.
	GtkWidget* enable_grid = make_two_column_section();
	gtk_box_pack_start(page, make_aligned_frame(_("Places mode"), enable_grid), false, false, 0);

	GtkWidget* enable_switch = make_form_switch();
	m_places_enabled_switch = enable_switch;
	GtkWidget* enable_label = gtk_label_new_with_mnemonic(_("Enable _Places"));
	gtk_switch_set_active(GTK_SWITCH(enable_switch), m_settings->places_enabled);
	add_form_row(enable_grid, COLUMN_C1, 0, enable_label, enable_switch, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(enable_label), enable_switch);

	// "Show icons" — renders the Apps/Places switch as two themed icon buttons
	// instead of text (supported behavior). Bound to /places/switch-show-icons with the
	// binding's reset-to-default; greyed (forced ON, value unchanged) when the
	// sidebar is Horizontal or disabled.
	GtkWidget* show_icons_switch = make_form_switch();
	GtkWidget* show_icons_label = gtk_label_new_with_mnemonic(_("Show _icons"));
	gtk_switch_set_active(GTK_SWITCH(show_icons_switch), m_settings->places_switch_show_icons);
	add_form_row(enable_grid, COLUMN_C2, 0, show_icons_label, show_icons_switch, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(show_icons_label), show_icons_switch);
	m_places_switch_show_icons = show_icons_switch;

	// Sections section (supported behavior) — history switch C1 / favourites switch C2;
	// favourite-sync combo C1 / maximum-items spin C2.
	GtkWidget* sections_grid = make_two_column_section();
	GtkWidget* sections_frame = make_aligned_frame(_("Sections"), sections_grid);
	gtk_box_pack_start(page, sections_frame, false, false, 0);
	places_dependents.push_back(sections_frame);

	GtkWidget* history_switch = make_form_switch();
	GtkWidget* history_label = gtk_label_new_with_mnemonic(_("Enable _History section"));
	gtk_switch_set_active(GTK_SWITCH(history_switch), m_settings->places_history_enabled);
	add_form_row(sections_grid, COLUMN_C1, 0, history_label, history_switch, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(history_label), history_switch);

	GtkWidget* fav_switch = make_form_switch();
	GtkWidget* fav_label = gtk_label_new_with_mnemonic(_("Enable _Favourites section"));
	gtk_switch_set_active(GTK_SWITCH(fav_switch), m_settings->places_favourites_enabled);
	add_form_row(sections_grid, COLUMN_C2, 0, fav_label, fav_switch, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(fav_label), fav_switch);

	GtkWidget* sync_label = gtk_label_new_with_mnemonic(_("Favourite _sync:"));
	GtkWidget* sync_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sync_combo), "meowmenu", _("MeowMenu only"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sync_combo), "thunar",   _("Thunar bookmarks (read-only)"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(sync_combo),
			static_cast<const gchar*>(m_settings->places_favourite_sync));
	add_form_row(sections_grid, COLUMN_C1, 1, sync_label, sync_combo, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(sync_label), sync_combo);

	GtkWidget* max_label = gtk_label_new_with_mnemonic(_("Maximum places _items:"));
	GtkWidget* max_spin = gtk_spin_button_new_with_range(0, 30, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_spin), m_settings->places_max_items);
	add_form_row(sections_grid, COLUMN_C2, 1, max_label, max_spin, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(max_label), max_spin);

	// Behaviour section (supported behavior): remember-last-mode and switch shape share one
	// row so the mode memory and its visual presentation are configured together.
	GtkWidget* behaviour_grid = make_two_column_section();
	GtkWidget* behaviour_frame = make_aligned_frame(_("Behaviour"), behaviour_grid);
	gtk_box_pack_start(page, behaviour_frame, false, false, 0);
	places_dependents.push_back(behaviour_frame);

	GtkWidget* remember_check = gtk_check_button_new_with_mnemonic(_("_Remember last selected mode"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(remember_check),
			m_settings->places_remember_last_mode);
	add_form_row(behaviour_grid, COLUMN_C1, 0, nullptr, remember_check, false, nullptr);

	GtkWidget* shape_label = gtk_label_new_with_mnemonic(_("Switch button _shape:"));
	GtkWidget* shape_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(shape_combo),
			PLACES_SWITCH_SHAPE_GTK_THEME, _("GTK theme"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(shape_combo),
			PLACES_SWITCH_SHAPE_ROUNDED, _("Rounded"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(shape_combo),
			places_switch_shape_or_default(m_settings->places_switch_button_shape));
	add_form_row(behaviour_grid, COLUMN_C2, 0, shape_label, shape_combo, false, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(shape_label), shape_combo);
	m_places_switch_button_shape = shape_combo;

	// Sensitivity helpers (supported behavior, supported behavior).
	auto refresh_sensitivity = [=]()
	{
		const bool enabled = gtk_switch_get_active(GTK_SWITCH(enable_switch));
		for (GtkWidget* w : places_dependents)
			gtk_widget_set_sensitive(w, enabled);
		const bool fav_enabled = enabled && gtk_switch_get_active(GTK_SWITCH(fav_switch));
		gtk_widget_set_sensitive(sync_combo, fav_enabled);
		gtk_widget_set_sensitive(sync_label, fav_enabled);

		// "Show icons" is forced ON (greyed, value unchanged) when the sidebar
		// is Horizontal or disabled; it is also moot when
		// Places is off. The stored value is never rewritten here.
		const gchar* sp = static_cast<const gchar*>(m_settings->sidebar_position);
		const bool strip = sp && g_strcmp0(sp, "horizontal") == 0;
		const bool forced = strip || !m_settings->sidebar_enabled;
		gtk_widget_set_sensitive(show_icons_switch, enabled && !forced);
		gtk_widget_set_sensitive(show_icons_label, enabled && !forced);
	};
	refresh_sensitivity();

	// Expose this tab's sensitivity recompute so sync_preset_widgets() can
	// re-evaluate the Places greying after a preset switch (supported behavior). This also
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
			m_plugin->refresh_layout();
			return FALSE; // let the switch update its visual state
		});
	connect(show_icons_switch, "state-set",
		[this](GtkSwitch*, gboolean state) -> gboolean
		{
			if (m_programmatic_update)
				return FALSE;
			// Stored intent only; the switch re-renders on the next menu open
			// when update_layout() reads the new value (supported behavior, render-time).
			m_settings->places_switch_show_icons = state;
			m_plugin->refresh_layout();
			return FALSE;
		});
	connect(history_switch, "state-set",
		[this](GtkSwitch*, gboolean state) -> gboolean
		{
			m_settings->places_history_enabled = state;
			m_plugin->refresh_layout();
			return FALSE;
		});
	connect(fav_switch, "state-set",
		[this, refresh_sensitivity](GtkSwitch*, gboolean state) -> gboolean
		{
			m_settings->places_favourites_enabled = state;
			refresh_sensitivity();
			m_plugin->refresh_layout();
			return FALSE;
		});
	connect(sync_combo, "changed",
		[this](GtkComboBox* combo)
		{
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (val)
			{
				m_settings->places_favourite_sync = val;
				m_plugin->refresh_layout();
			}
		});
	connect(max_spin, "value-changed",
		[this](GtkSpinButton* btn)
		{
			m_settings->places_max_items = gtk_spin_button_get_value_as_int(btn);
			m_plugin->refresh_layout();
		});
	connect(remember_check, "toggled",
		[this](GtkToggleButton* btn)
		{
			m_settings->places_remember_last_mode = gtk_toggle_button_get_active(btn);
		});
	connect(shape_combo, "changed",
		[this](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			m_settings->places_switch_button_shape = places_switch_shape_or_default(val);
			m_plugin->refresh_layout();
		});

	return wrap_in_scrolled(GTK_WIDGET(page));
}
