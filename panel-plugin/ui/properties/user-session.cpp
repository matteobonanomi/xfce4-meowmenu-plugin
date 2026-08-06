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

#include "ui/command-edit.h"
#include "core/plugin.h"
#include "settings.h"
#include "ui/slot.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* init_user_session_tab:
 *
 * Builds the Session tab with one visibility/confirmation section followed by
 * the existing per-action editors.
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_user_session_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	// Session visibility and confirmation
	GtkGrid* commands_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(commands_table, 12);
	gtk_grid_set_row_spacing(commands_table, 6);

	GtkWidget* commands_frame = make_aligned_frame(
			_("Session controls"), GTK_WIDGET(commands_table));
	gtk_box_pack_start(page, commands_frame, false, false, 0);

	GtkWidget* show_session_label =
			gtk_label_new_with_mnemonic(_("Show _session controls:"));
	gtk_widget_set_halign(show_session_label, GTK_ALIGN_START);
	gtk_grid_attach(commands_table, show_session_label, 0, 0, 1, 1);
	m_show_session = make_form_switch();
	gtk_switch_set_active(GTK_SWITCH(m_show_session),
			static_cast<bool>(m_settings->show_session));
	gtk_grid_attach(commands_table, m_show_session, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(show_session_label), m_show_session);

	connect(m_show_session, "notify::active",
		[this](GObject* object, GParamSpec*)
		{
			if (m_programmatic_update)
				return;
			m_settings->show_session =
					gtk_switch_get_active(GTK_SWITCH(object));
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	m_confirm_session_command = gtk_check_button_new_with_mnemonic(_("Show c_onfirmation dialog"));
	gtk_grid_attach(commands_table, m_confirm_session_command, 0, 1, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_confirm_session_command),
		m_settings->confirm_session_command);

	connect(m_confirm_session_command, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->confirm_session_command = gtk_toggle_button_get_active(button);
		});

	// =========================================================================
	// 2. Session commands list (per-slot CommandEdit, reused from legacy)
	// =========================================================================
	GtkBox* commands_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	GtkWidget* session_frame = make_aligned_frame(_("Session commands"), GTK_WIDGET(commands_vbox));
	gtk_box_pack_start(page, session_frame, true, true, 0);

	GtkSizeGroup* cmd_label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	for (auto command : m_settings->command)
	{
		CommandEdit* command_edit = new CommandEdit(command, cmd_label_size_group);
		gtk_box_pack_start(commands_vbox, command_edit->get_widget(), false, false, 0);
		m_commands.push_back(command_edit);
	}

	return wrap_in_scrolled(GTK_WIDGET(page));
}
