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
#include "core/user-session-layout.h"
#include "settings.h"
#include "ui/slot.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

class SettingsDialog::UserSessionTransactionGuard
{
public:
	explicit UserSessionTransactionGuard(SettingsDialog* dialog) :
		m_dialog(dialog),
		m_active(dialog && dialog->m_settings)
	{
		if (!m_active)
			return;

		m_dialog->m_user_session_transaction_active = true;
		m_dialog->m_user_session_defer_coupling = true;
		m_dialog->m_user_session_defer_unified_bar = false;
		m_dialog->m_settings->begin_property_update();
	}

	~UserSessionTransactionGuard()
	{
		if (!m_active)
			return;

		m_dialog->m_settings->end_property_update();
		m_dialog->m_user_session_transaction_active = false;
		const bool refresh_coupling = m_dialog->m_user_session_defer_coupling;
		const bool refresh_unified_bar = m_dialog->m_user_session_defer_unified_bar;
		m_dialog->m_user_session_defer_coupling = false;
		m_dialog->m_user_session_defer_unified_bar = false;

		if (refresh_coupling)
			m_dialog->apply_user_session_coupling();
		if (refresh_unified_bar)
			m_dialog->apply_unified_bar_sensitivity();
	}

	UserSessionTransactionGuard(const UserSessionTransactionGuard&) = delete;
	UserSessionTransactionGuard& operator=(const UserSessionTransactionGuard&) = delete;

private:
	SettingsDialog* m_dialog;
	bool m_active;
};

//-----------------------------------------------------------------------------

/* init_user_session_tab:
 *
 * Builds the User/Session tab in the 003-properties-refactor 5-tab dictionary.
 * Sections (top-to-bottom, FR-020):
 *   1. Profile         — profile-position, profile-shape (only when visible).
 *   2. Commands        — commands-position, confirm-session-command toggle.
 *   3. Session commands — per-slot Command editors via CommandEdit, reused
 *                         from the legacy Commands tab (no editor duplication).
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_user_session_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkSizeGroup* label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* control_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	// =========================================================================
	// 1. Profile section
	// =========================================================================
	GtkGrid* profile_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(profile_table, 12);
	gtk_grid_set_row_spacing(profile_table, 6);

	GtkWidget* profile_frame = make_aligned_frame(_("Profile"), GTK_WIDGET(profile_table));
	gtk_box_pack_start(page, profile_frame, false, false, 0);

	// Profile position
	GtkWidget* prof_pos_label = gtk_label_new_with_mnemonic(_("_Position:"));
	gtk_widget_set_halign(prof_pos_label, GTK_ALIGN_START);
	gtk_grid_attach(profile_table, prof_pos_label, 0, 0, 1, 1);

	m_profile_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profile_position_combo), "top-left", _("Top Left"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profile_position_combo), "bottom-left", _("Bottom Left"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_profile_position_combo), "hidden", _("Hidden"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_profile_position_combo),
		static_cast<const gchar*>(m_settings->profile_position));
	gtk_widget_set_halign(m_profile_position_combo, GTK_ALIGN_START);
	gtk_grid_attach(profile_table, m_profile_position_combo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(prof_pos_label), m_profile_position_combo);
	gtk_size_group_add_widget(label_size_group, prof_pos_label);
	gtk_size_group_add_widget(control_size_group, m_profile_position_combo);

	// Avatar shape (sub-enabled when profile-position != hidden).
	GtkWidget* shape_label = gtk_label_new_with_mnemonic(_("Avatar _shape:"));
	gtk_widget_set_halign(shape_label, GTK_ALIGN_START);
	gtk_grid_attach(profile_table, shape_label, 0, 1, 1, 1);

	m_profile_shape = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_profile_shape), _("Round Picture"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_profile_shape), _("Square Picture"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_profile_shape), m_settings->profile_shape);
	gtk_widget_set_halign(m_profile_shape, GTK_ALIGN_START);
	gtk_grid_attach(profile_table, m_profile_shape, 1, 1, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(shape_label), m_profile_shape);
	gtk_size_group_add_widget(label_size_group, shape_label);
	gtk_size_group_add_widget(control_size_group, m_profile_shape);

	auto apply_profile_visibility = [this, shape_label]()
	{
		const gchar* pos = static_cast<const gchar*>(m_settings->profile_position);
		const bool visible = pos && g_strcmp0(pos, "hidden") != 0;
		gtk_widget_set_sensitive(m_profile_shape, visible);
		gtk_widget_set_sensitive(shape_label, visible);
	};
	apply_profile_visibility();

	connect(m_profile_position_combo, "changed",
		[this, apply_profile_visibility](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			apply_user_session_selection(true, val);
			apply_profile_visibility();
			refresh_customized_indicator();
		});

	connect(m_profile_shape, "changed",
		[this](GtkComboBox* combo)
			{
				m_settings->profile_shape = gtk_combo_box_get_active(combo);
				m_plugin->refresh_layout();
				refresh_customized_indicator();
			});

	// =========================================================================
	// 2. Commands section (position + confirmation)
	// =========================================================================
	GtkGrid* commands_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(commands_table, 12);
	gtk_grid_set_row_spacing(commands_table, 6);

	GtkWidget* commands_frame = make_aligned_frame(_("Commands"), GTK_WIDGET(commands_table));
	gtk_box_pack_start(page, commands_frame, false, false, 0);

	GtkWidget* cmd_pos_label = gtk_label_new_with_mnemonic(_("_Commands position:"));
	gtk_widget_set_halign(cmd_pos_label, GTK_ALIGN_START);
	gtk_grid_attach(commands_table, cmd_pos_label, 0, 0, 1, 1);

	m_commands_position_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_commands_position_combo), "top-right", _("Top Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_commands_position_combo), "bottom-right", _("Bottom Right"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_commands_position_combo), "hidden", _("Hidden"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_commands_position_combo),
		static_cast<const gchar*>(m_settings->commands_position));
	gtk_widget_set_halign(m_commands_position_combo, GTK_ALIGN_START);
	gtk_grid_attach(commands_table, m_commands_position_combo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(cmd_pos_label), m_commands_position_combo);
	gtk_size_group_add_widget(label_size_group, cmd_pos_label);
	gtk_size_group_add_widget(control_size_group, m_commands_position_combo);

	connect(m_commands_position_combo, "changed",
		[this](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			apply_user_session_selection(false, val);
			refresh_customized_indicator();
		});

	// Per-row greying of the Profile/Commands position combos and live
	// reflection of any persisted auto-snap, both driven by the shared coupling
	// helper (FR-010/FR-017). A sensitivity data func on each combo's text
	// renderer greys disallowed options without removing them; the property
	// signal refreshes both whenever a governing key changes (e.g. a Profile
	// edge flip snaps Commands, or a layout-mode switch re-couples both).
	auto attach_sensitivity = [this](GtkWidget* combo)
	{
		GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combo));
		if (cells)
		{
			gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo),
					GTK_CELL_RENDERER(cells->data),
					&SettingsDialog::on_user_session_cell_data, this, nullptr);
			g_list_free(cells);
		}
	};
	attach_sensitivity(m_profile_position_combo);
	attach_sensitivity(m_commands_position_combo);

	if (m_settings && m_settings->channel)
	{
		m_user_session_coupling_slot = g_signal_connect(m_settings->channel,
			"property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue*, gpointer data) -> void
			{
				if (g_strcmp0(property, "/layout-mode") != 0
						&& g_strcmp0(property, "/search-bar-position") != 0
						&& g_strcmp0(property, "/profile-position") != 0
						&& g_strcmp0(property, "/commands-position") != 0)
					return;
				auto* self = static_cast<SettingsDialog*>(data);
				if (self->m_user_session_transaction_active)
				{
					self->m_user_session_defer_coupling = true;
					return;
				}
				self->apply_user_session_coupling();
			}), this);
	}

	apply_user_session_coupling();

	// Unified-bar toggle (spec 004). Sits immediately under the
	// commands-position combobox; live sensitivity drops in via
	// apply_unified_bar_sensitivity(), which is invoked on any of the four
	// position keys changing through the existing property-changed signal.
	m_unified_bar = gtk_check_button_new_with_mnemonic(
		_("Place profile, search and session on a single _line"));
	gtk_grid_attach(commands_table, m_unified_bar, 0, 1, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_unified_bar),
		static_cast<bool>(m_settings->unified_bar));
	apply_unified_bar_sensitivity();

	connect(m_unified_bar, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->unified_bar = gtk_toggle_button_get_active(button);
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	if (m_settings && m_settings->channel)
	{
		m_unified_bar_slot = g_signal_connect(m_settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue*, gpointer data) -> void
			{
				if (g_strcmp0(property, "/layout-mode") != 0
						&& g_strcmp0(property, "/search-bar-position") != 0
						&& g_strcmp0(property, "/profile-position") != 0
						&& g_strcmp0(property, "/commands-position") != 0
						&& g_strcmp0(property, "/unified-bar") != 0)
					return;
				auto* self = static_cast<SettingsDialog*>(data);
				if (self->m_user_session_transaction_active)
				{
					self->m_user_session_defer_unified_bar = true;
					return;
				}
				self->apply_unified_bar_sensitivity();
			}), this);
	}

	m_confirm_session_command = gtk_check_button_new_with_mnemonic(_("Show c_onfirmation dialog"));
	gtk_grid_attach(commands_table, m_confirm_session_command, 0, 2, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_confirm_session_command),
		m_settings->confirm_session_command);

	connect(m_confirm_session_command, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->confirm_session_command = gtk_toggle_button_get_active(button);
		});

	// =========================================================================
	// 3. Session commands list (per-slot CommandEdit, reused from legacy)
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

//-----------------------------------------------------------------------------

/* SettingsDialog::on_user_session_cell_data:
 *
 * GtkCellLayoutDataFunc trampoline. @data is the owning SettingsDialog.
 */
void SettingsDialog::on_user_session_cell_data(GtkCellLayout* layout,
		GtkCellRenderer* cell, GtkTreeModel* model, GtkTreeIter* iter, gpointer data)
{
	static_cast<SettingsDialog*>(data)
			->apply_user_session_combo_sensitivity(layout, cell, model, iter);
}

//-----------------------------------------------------------------------------

/* apply_user_session_combo_sensitivity:
 * @layout: the combo whose row is being rendered (Profile or Commands).
 * @cell:   the text renderer to set sensitive/insensitive.
 * @model:  the GtkComboBoxText model (text in column 0, id in column 1).
 * @iter:   the row being rendered.
 *
 * Greys a single combo row when the current layout coupling disallows that
 * option (FR-010). The decision comes from the shared helper so the dialog can
 * never disagree with the renderer; Hidden is always left selectable.
 */
void SettingsDialog::apply_user_session_combo_sensitivity(GtkCellLayout* layout,
		GtkCellRenderer* cell, GtkTreeModel* model, GtkTreeIter* iter)
{
	gchar* id = nullptr;
	// GtkComboBoxText keeps the option id in model column 1.
	gtk_tree_model_get(model, iter, 1, &id, -1);

	const LayoutMode mode = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0)
			? LayoutMode::FullScreen : LayoutMode::Docked;
	const UserSessionResolution res = normalize_user_session(mode,
			m_settings->search_bar_position, m_settings->profile_position,
			m_settings->commands_position);

	bool enabled = true;
	if (layout == GTK_CELL_LAYOUT(m_profile_position_combo))
	{
		if (g_strcmp0(id, "top-left") == 0)
			enabled = res.profile_top_left_enabled;
		else if (g_strcmp0(id, "bottom-left") == 0)
			enabled = res.profile_bottom_left_enabled;
		else
			enabled = res.profile_hidden_enabled;
	}
	else
	{
		if (g_strcmp0(id, "top-right") == 0)
			enabled = res.commands_top_right_enabled;
		else if (g_strcmp0(id, "bottom-right") == 0)
			enabled = res.commands_bottom_right_enabled;
		else
			enabled = res.commands_hidden_enabled;
	}

	g_object_set(cell, "sensitive", enabled, nullptr);
	g_free(id);
}

//-----------------------------------------------------------------------------

/* apply_user_session_coupling:
 *
 * Re-resolves the Profile/Commands coupling for the current layout and reflects
 * the result in the two position combos: their active id is set to the resolved
 * (possibly auto-snapped) value so the dialog matches what was persisted
 * (FR-017), and both are redrawn so the per-row greying re-evaluates. Invoked at
 * build time and whenever a governing key changes on the channel.
 */
void SettingsDialog::apply_user_session_coupling()
{
	if (!m_profile_position_combo || !m_commands_position_combo)
		return;

	const LayoutMode mode = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0)
			? LayoutMode::FullScreen : LayoutMode::Docked;
	const UserSessionResolution res = normalize_user_session(mode,
			m_settings->search_bar_position, m_settings->profile_position,
			m_settings->commands_position);

	// Guard the programmatic active-id updates so the combos' "changed" handlers
	// (which write back to Settings) do not re-fire for a value we are merely
	// mirroring.
	const bool was_programmatic_update = m_programmatic_update;
	m_programmatic_update = true;
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_profile_position_combo),
			res.profile_position);
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_commands_position_combo),
			res.commands_position);
	m_programmatic_update = was_programmatic_update;

	gtk_widget_queue_draw(m_profile_position_combo);
	gtk_widget_queue_draw(m_commands_position_combo);
}

//-----------------------------------------------------------------------------

/* apply_user_session_selection:
 * @profile_combo: true when the Profile combo triggered the change, false for
 *                 the Commands combo.
 * @requested_position: the newly selected combo id; canonical visible values
 *                      are paired to the matching opposite cluster row.
 *
 * Applies a live User/Session row edit as one coherent property update. When
 * the non-edited cluster is visible, a visible row change moves the whole pair
 * together; when the partner is hidden, it stays hidden. The final passive
 * normalization pass keeps full-screen search-bar parity and legacy-alias
 * cleanup intact without exposing a partially-applied intermediate state.
 */
void SettingsDialog::apply_user_session_selection(bool profile_combo,
		const char* requested_position)
{
	if (!requested_position)
		return;

	const LayoutMode mode = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0)
			? LayoutMode::FullScreen : LayoutMode::Docked;
	const UserSessionResolution current = normalize_user_session(mode,
			m_settings->search_bar_position, m_settings->profile_position,
			m_settings->commands_position);
	const bool requested_visible = profile_combo
			? !profile_position_is_hidden(requested_position)
			: !commands_position_is_hidden(requested_position);
	const UserSessionRowEdge requested_row = (profile_combo
			? profile_position_is_bottom(requested_position)
			: commands_position_is_bottom(requested_position))
			? UserSessionRowEdge::Bottom : UserSessionRowEdge::Top;

	{
		UserSessionTransactionGuard transaction(this);

		if (profile_combo)
		{
			m_settings->profile_position = requested_position;
			if ((mode == LayoutMode::Docked) && requested_visible && current.commands_visible)
				m_settings->commands_position = commands_position_for_row(requested_row);
		}
		else
		{
			m_settings->commands_position = requested_position;
			if ((mode == LayoutMode::Docked) && requested_visible && current.profile_visible)
				m_settings->profile_position = profile_position_for_row(requested_row);
		}

		const UserSessionResolution resolved = normalize_user_session(mode,
				m_settings->search_bar_position, m_settings->profile_position,
				m_settings->commands_position);
		if (resolved.profile_changed)
			m_settings->profile_position = resolved.profile_position;
		if (resolved.commands_changed)
			m_settings->commands_position = resolved.commands_position;
	}

	m_plugin->refresh_layout();
}
