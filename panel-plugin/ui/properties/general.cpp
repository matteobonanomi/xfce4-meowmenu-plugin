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
#include "presets/preset.h"
#include "presets/preset-io.h"
#include "settings.h"
#include "ui/slot.h"

#include <climits>
#include <cstring>
#include <string>

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* init_general_tab:
 *
 * Builds the General tab in the 003-properties-refactor 5-tab dictionary.
 * Sections (top-to-bottom, FR-010):
 *   1. Preset          — select/save/import/export presets (T014).
 *   2. Panel plugin    — button title/icon visibility, title, icon picker,
 *                        single-row toggle (T015).
 *   3. General menu    — layout-mode, panel-gap, menu-width, menu-height,
 *                        corner-radius, full-screen-opacity, stay-on-focus-out
 *                        (T016).
 *
 * Widgets that are sensitive only in one layout mode are pushed onto
 * m_layout_enable_when_docked / m_layout_enable_when_fullscreen so the live
 * handler installed by install_layout_mode_handler() can flip their state
 * on /layout-mode change without a dialog reopen (T017, FR-003).
 *
 * Returns: a scrolled container ready to be packed into the dialog's stack.
 */
GtkWidget* SettingsDialog::init_general_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	// =========================================================================
	// 1. Preset section
	// =========================================================================
	GtkGrid* preset_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(preset_table, 12);
	gtk_grid_set_row_spacing(preset_table, 6);

	GtkWidget* preset_frame = make_aligned_frame(_("Preset"), GTK_WIDGET(preset_table));
	gtk_box_pack_start(page, preset_frame, false, false, 0);

	GtkWidget* preset_label = gtk_label_new_with_mnemonic(_("_Preset:"));
	gtk_widget_set_halign(preset_label, GTK_ALIGN_START);
	gtk_grid_attach(preset_table, preset_label, 0, 0, 1, 1);

	m_preset_combo = gtk_combo_box_text_new();
	gtk_widget_set_hexpand(m_preset_combo, true);
	gtk_grid_attach(preset_table, m_preset_combo, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(preset_label), m_preset_combo);

	m_preset_description = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(m_preset_description), true);
	gtk_label_set_xalign(GTK_LABEL(m_preset_description), 0.0f);
	gtk_widget_set_hexpand(m_preset_description, true);
	gtk_grid_attach(preset_table, m_preset_description, 0, 1, 2, 1);

	m_preset_customized = gtk_label_new(_("● Customized"));
	gtk_label_set_xalign(GTK_LABEL(m_preset_customized), 0.0f);
	gtk_grid_attach(preset_table, m_preset_customized, 0, 2, 2, 1);
	gtk_widget_hide(m_preset_customized);

	// Preset action buttons row.
	GtkWidget* action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_top(action_box, 4);
	gtk_grid_attach(preset_table, action_box, 0, 3, 2, 1);

	GtkWidget* reset_preset_btn = gtk_button_new_with_mnemonic(_("_Reset preset"));
	gtk_box_pack_start(GTK_BOX(action_box), reset_preset_btn, false, false, 0);

	connect(reset_preset_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* pid = static_cast<const gchar*>(m_settings->current_preset_id);
			const LayoutPreset* preset = find_preset_by_id(pid ? std::string(pid) : std::string());
			if (!preset)
				return;
			if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				"edit-undo", _("_Reset"),
				_("All customizations will be discarded and the preset values restored."),
				_("Reset preset \"%s\"?"), _(preset->display_name.c_str())))
			{
				apply_preset(*preset, *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_customized_indicator();
			}
		});

	GtkWidget* save_btn = gtk_button_new_with_mnemonic(_("_Save as new…"));
	gtk_box_pack_start(GTK_BOX(action_box), save_btn, false, false, 0);

	connect(save_btn, "clicked",
		[this](GtkButton*)
		{
			GtkWidget* dlg = gtk_dialog_new_with_buttons(_("Save as new preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_DIALOG_MODAL,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Save"),   GTK_RESPONSE_OK,
				nullptr);
			gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

			GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
			GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
			gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
			gtk_box_pack_start(GTK_BOX(content), hbox, false, false, 0);

			GtkWidget* lbl = gtk_label_new_with_mnemonic(_("_Name:"));
			gtk_box_pack_start(GTK_BOX(hbox), lbl, false, false, 0);
			GtkWidget* entry = gtk_entry_new();
			gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
			gtk_widget_set_hexpand(entry, true);
			gtk_box_pack_start(GTK_BOX(hbox), entry, true, true, 0);
			gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), entry);

			gtk_widget_show_all(dlg);
			if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
			{
				const gchar* name = gtk_entry_get_text(GTK_ENTRY(entry));
				std::string uuid = save_current_as_user_preset(name ? name : "", *m_settings);
				if (uuid.empty())
				{
					xfce_dialog_show_warning(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
						_("A preset with that name already exists or the name is empty."),
						"%s", _("Could not save preset"));
				}
				else
				{
					refresh_preset_combo(uuid);
					m_plugin->reload_menu();
					refresh_customized_indicator();
				}
			}
			gtk_widget_destroy(dlg);
		});

	m_preset_rename_btn = gtk_button_new_with_mnemonic(_("Re_name…"));
	gtk_widget_set_sensitive(m_preset_rename_btn, false);
	gtk_box_pack_start(GTK_BOX(action_box), m_preset_rename_btn, false, false, 0);

	connect(m_preset_rename_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
			if (!active_id)
				return;
			const LayoutPreset* preset = find_preset_by_id(std::string(active_id));
			if (!preset || preset->is_builtin)
				return;

			GtkWidget* dlg = gtk_dialog_new_with_buttons(_("Rename preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_DIALOG_MODAL,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Rename"), GTK_RESPONSE_OK,
				nullptr);
			gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

			GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
			GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
			gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
			gtk_box_pack_start(GTK_BOX(content), hbox, false, false, 0);

			GtkWidget* lbl = gtk_label_new_with_mnemonic(_("_Name:"));
			gtk_box_pack_start(GTK_BOX(hbox), lbl, false, false, 0);
			GtkWidget* entry = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(entry), preset->display_name.c_str());
			gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
			gtk_widget_set_hexpand(entry, true);
			gtk_box_pack_start(GTK_BOX(hbox), entry, true, true, 0);
			gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), entry);

			gtk_widget_show_all(dlg);
			if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
			{
				const gchar* name = gtk_entry_get_text(GTK_ENTRY(entry));
				std::string uuid = preset->id;
				if (!rename_user_preset(uuid, name ? name : "", *m_settings))
				{
					xfce_dialog_show_warning(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
						_("A preset with that name already exists or the name is empty."),
						"%s", _("Could not rename preset"));
				}
				else
				{
					refresh_preset_combo(uuid);
				}
			}
			gtk_widget_destroy(dlg);
		});

	m_preset_delete_btn = gtk_button_new_with_mnemonic(_("_Delete"));
	gtk_widget_set_sensitive(m_preset_delete_btn, false);
	gtk_box_pack_start(GTK_BOX(action_box), m_preset_delete_btn, false, false, 0);

	connect(m_preset_delete_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
			if (!active_id)
				return;
			const LayoutPreset* preset = find_preset_by_id(std::string(active_id));
			if (!preset || preset->is_builtin)
				return;

			if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				"edit-delete", _("_Delete"),
				_("This preset will be permanently removed."),
				_("Delete preset \"%s\"?"), preset->display_name.c_str()))
			{
				std::string uuid = preset->id;
				delete_user_preset(uuid, *m_settings);
				refresh_preset_combo("modern");
				apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_customized_indicator();
			}
		});

	// Export / Import row.
	GtkWidget* io_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_top(io_box, 2);
	gtk_grid_attach(preset_table, io_box, 0, 4, 2, 1);

	m_preset_export_btn = gtk_button_new_with_mnemonic(_("E_xport…"));
	gtk_widget_set_sensitive(m_preset_export_btn, false);
	gtk_box_pack_start(GTK_BOX(io_box), m_preset_export_btn, false, false, 0);

	connect(m_preset_export_btn, "clicked",
		[this](GtkButton*)
		{
			const gchar* active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
			if (!active_id)
				return;
			const LayoutPreset* preset = find_preset_by_id(std::string(active_id));
			if (!preset || preset->is_builtin)
				return;

			GtkFileChooserNative* chooser = gtk_file_chooser_native_new(
				_("Export preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				_("_Export"), _("_Cancel"));

			std::string suggested = preset->display_name + ".meowpreset";
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), suggested.c_str());

			GtkFileFilter* filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter, _("MeowMenu preset (*.meowpreset)"));
			gtk_file_filter_add_pattern(filter, "*.meowpreset");
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

			if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
			{
				gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
				if (path)
				{
					if (!export_user_preset(preset->id, std::string(path), *m_settings))
					{
						xfce_dialog_show_error(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
							nullptr, "%s", _("Could not export preset."));
					}
					g_free(path);
				}
			}
			g_object_unref(chooser);
		});

	GtkWidget* import_btn = gtk_button_new_with_mnemonic(_("_Import…"));
	gtk_box_pack_start(GTK_BOX(io_box), import_btn, false, false, 0);

	connect(import_btn, "clicked",
		[this](GtkButton*)
		{
			GtkFileChooserNative* chooser = gtk_file_chooser_native_new(
				_("Import preset"),
				GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				_("_Import"), _("_Cancel"));

			GtkFileFilter* filter = gtk_file_filter_new();
			gtk_file_filter_set_name(filter, _("MeowMenu preset (*.meowpreset)"));
			gtk_file_filter_add_pattern(filter, "*.meowpreset");
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

			if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT)
			{
				g_object_unref(chooser);
				return;
			}
			gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
			g_object_unref(chooser);
			if (!path)
				return;

			std::string file_path(path);
			g_free(path);

			ImportResult result = import_user_preset(file_path, *m_settings);

			if (result.status == ImportStatus::ConflictUser)
			{
				GtkWidget* dlg = gtk_message_dialog_new(
					GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					_("A preset named \"%s\" already exists. What would you like to do?"),
					result.display_name.c_str());
				gtk_dialog_add_buttons(GTK_DIALOG(dlg),
					_("_Overwrite"), 1,
					_("Re_name"),   2,
					_("_Cancel"),   GTK_RESPONSE_CANCEL,
					nullptr);
				int resp = gtk_dialog_run(GTK_DIALOG(dlg));
				gtk_widget_destroy(dlg);

				if (resp == 1)
				{
					result = import_user_preset(file_path, *m_settings, {}, result.conflict_uuid);
				}
				else if (resp == 2)
				{
					GtkWidget* ndlg = gtk_dialog_new_with_buttons(_("Rename imported preset"),
						GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
						GTK_DIALOG_MODAL,
						_("_Cancel"), GTK_RESPONSE_CANCEL,
						_("_Import"), GTK_RESPONSE_OK,
						nullptr);
					gtk_dialog_set_default_response(GTK_DIALOG(ndlg), GTK_RESPONSE_OK);
					GtkWidget* ncontent = gtk_dialog_get_content_area(GTK_DIALOG(ndlg));
					GtkWidget* nhbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
					gtk_container_set_border_width(GTK_CONTAINER(nhbox), 12);
					gtk_box_pack_start(GTK_BOX(ncontent), nhbox, false, false, 0);
					GtkWidget* nlbl = gtk_label_new_with_mnemonic(_("_Name:"));
					gtk_box_pack_start(GTK_BOX(nhbox), nlbl, false, false, 0);
					GtkWidget* nentry = gtk_entry_new();
					gtk_entry_set_text(GTK_ENTRY(nentry), result.display_name.c_str());
					gtk_entry_set_activates_default(GTK_ENTRY(nentry), true);
					gtk_widget_set_hexpand(nentry, true);
					gtk_box_pack_start(GTK_BOX(nhbox), nentry, true, true, 0);
					gtk_label_set_mnemonic_widget(GTK_LABEL(nlbl), nentry);
					gtk_widget_show_all(ndlg);

					if (gtk_dialog_run(GTK_DIALOG(ndlg)) == GTK_RESPONSE_OK)
					{
						const gchar* nname = gtk_entry_get_text(GTK_ENTRY(nentry));
						result = import_user_preset(file_path, *m_settings,
							nname ? std::string(nname) : std::string());
					}
					else
					{
						result.status = ImportStatus::ParseError; // signal: cancelled
					}
					gtk_widget_destroy(ndlg);
				}
				else
				{
					return; // user cancelled
				}
			}

			if (result.status == ImportStatus::Ok)
			{
				refresh_preset_combo(result.new_uuid);
			}
			else if (result.status != ImportStatus::ParseError)
			{
				xfce_dialog_show_error(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
					nullptr, "%s: %s", _("Could not import preset"),
					result.error_message.c_str());
			}
		});

	// Reset to defaults — full channel reset, kept here as the most logical
	// action button for the Preset hub.
	GtkWidget* defaults_btn = gtk_button_new_with_mnemonic(_("Reset to _defaults"));
	gtk_widget_set_margin_top(defaults_btn, 8);
	gtk_widget_set_halign(defaults_btn, GTK_ALIGN_START);
	gtk_grid_attach(preset_table, defaults_btn, 0, 5, 2, 1);

	connect(defaults_btn, "clicked",
		[this](GtkButton*)
		{
			if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				"edit-undo", _("_Reset"),
				_("All settings will be reset to defaults and the Modern preset will be applied."),
				_("Reset all settings to defaults?")))
			{
				// Hard reset: clear all plugin properties except saved user presets.
				GHashTable* props = xfconf_channel_get_properties(m_settings->channel, nullptr);
				if (props)
				{
					GHashTableIter iter;
					gpointer key_ptr, value_ptr;
					g_hash_table_iter_init(&iter, props);
					while (g_hash_table_iter_next(&iter, &key_ptr, &value_ptr))
					{
						(void)value_ptr;
						const gchar* path = static_cast<const gchar*>(key_ptr);
						if (g_str_has_prefix(path, "/presets"))
							continue;
						xfconf_channel_reset_property(m_settings->channel, path, FALSE);
					}
					g_hash_table_unref(props);
				}

				apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_preset_combo("modern");
				refresh_customized_indicator();
			}
		});

	// Populate preset combo and initial description.
	refresh_preset_combo(static_cast<const gchar*>(m_settings->current_preset_id)
		? std::string(static_cast<const gchar*>(m_settings->current_preset_id))
		: std::string());
	{
		const gchar* pid = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_preset_combo));
		const LayoutPreset* preset = find_preset_by_id(pid ? std::string(pid) : std::string());
		if (preset)
			gtk_label_set_text(GTK_LABEL(m_preset_description), _(preset->description.c_str()));
	}

	connect(m_preset_combo, "changed",
		[this](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			const gchar* id = gtk_combo_box_get_active_id(combo);
			if (!id)
				return;
			const LayoutPreset* preset = find_preset_by_id(std::string(id));
			if (!preset)
				return;
			gtk_label_set_text(GTK_LABEL(m_preset_description), _(preset->description.c_str()));
			// Applying a built-in preset only writes Settings fields and
			// current_preset_id; it never touches any /presets/<uuid>/ entry, so
			// custom presets stay immutable and distinct (FR-007/011b). Re-running
			// the apply chain for the active id acts as "reset to this preset"
			// (FR-006) — the "Reset preset" button drives the same path for the
			// already-selected entry, which the combo's "changed" signal cannot.
			apply_preset(*preset, *m_settings);
			m_last_applied_preset_id = preset->id;
			m_plugin->reload_menu();
			sync_preset_widgets();
			refresh_customized_indicator();
			const bool is_user = !preset->is_builtin;
			gtk_widget_set_sensitive(m_preset_rename_btn, is_user);
			gtk_widget_set_sensitive(m_preset_delete_btn, is_user);
			gtk_widget_set_sensitive(m_preset_export_btn, is_user);
		});

	// FullScreen warning InfoBar — shown when fullscreen is active but
	// gtk-layer-shell is unavailable. HACK: at build time we don't know whether
	// the running compositor will honour layer-shell, so we conservatively show
	// the warning only when the dependency isn't compiled in.
#if defined(HAVE_GTK_LAYER_SHELL)
	(void)0;
#else
	{
		GtkWidget* infobar = gtk_info_bar_new();
		gtk_info_bar_set_message_type(GTK_INFO_BAR(infobar), GTK_MESSAGE_WARNING);
		GtkWidget* infobar_content = gtk_info_bar_get_content_area(GTK_INFO_BAR(infobar));
		GtkWidget* infobar_label = gtk_label_new(
			_("FullScreen mode requires gtk-layer-shell on Wayland. "
			  "The menu will open as a regular window instead."));
		gtk_label_set_line_wrap(GTK_LABEL(infobar_label), true);
		gtk_label_set_xalign(GTK_LABEL(infobar_label), 0.0f);
		gtk_container_add(GTK_CONTAINER(infobar_content), infobar_label);

		const gchar* cur_id = static_cast<const gchar*>(m_settings->current_preset_id);
		gtk_widget_set_visible(infobar, cur_id && strcmp(cur_id, "fullscreen") == 0);
		gtk_box_pack_start(page, infobar, false, false, 0);
	}
#endif

	// =========================================================================
	// 2. Panel plugin section (T015)
	// =========================================================================
	GtkSizeGroup* label_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* control_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	GtkGrid* panel_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(panel_table, 12);
	gtk_grid_set_row_spacing(panel_table, 6);

	GtkWidget* panel_frame = make_aligned_frame(_("Panel plugin"), GTK_WIDGET(panel_table));
	gtk_box_pack_start(page, panel_frame, false, false, 0);

	int panel_row = 0;

	// Show panel button title
	m_button_title_visible = gtk_check_button_new_with_mnemonic(_("Show panel button _title"));
	gtk_grid_attach(panel_table, m_button_title_visible, 0, panel_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_title_visible),
		static_cast<bool>(m_settings->button_title_visible));
	++panel_row;

	// Title entry
	GtkWidget* title_label = gtk_label_new_with_mnemonic(_("T_itle:"));
	gtk_widget_set_halign(title_label, GTK_ALIGN_START);
	gtk_grid_attach(panel_table, title_label, 0, panel_row, 1, 1);

	m_title = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(m_title), m_settings->button_title);
	gtk_widget_set_hexpand(m_title, true);
	gtk_grid_attach(panel_table, m_title, 1, panel_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(title_label), m_title);
	gtk_size_group_add_widget(label_size_group, title_label);
	gtk_size_group_add_widget(control_size_group, m_title);
	gtk_widget_set_sensitive(m_title, static_cast<bool>(m_settings->button_title_visible));
	++panel_row;

	connect(m_title, "changed",
		[this](GtkEditable* editable)
		{
			const gchar* text = gtk_entry_get_text(GTK_ENTRY(editable));
			m_plugin->set_button_title(text ? text : "");
		});

	connect(m_button_title_visible, "toggled",
		[this](GtkToggleButton* button)
		{
			const bool active = gtk_toggle_button_get_active(button);
			m_settings->button_title_visible = active;
			m_plugin->set_button_style(m_plugin->get_button_style());
			gtk_widget_set_sensitive(m_title, active);
			if (m_button_single_row)
			{
				gtk_widget_set_sensitive(m_button_single_row,
					!active && static_cast<bool>(m_settings->button_icon_visible));
			}
			refresh_customized_indicator();
		});

	// Show panel button icon
	m_button_icon_visible = gtk_check_button_new_with_mnemonic(_("Show panel button _icon"));
	gtk_grid_attach(panel_table, m_button_icon_visible, 0, panel_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_icon_visible),
		static_cast<bool>(m_settings->button_icon_visible));
	++panel_row;

	// Icon picker
	GtkWidget* icon_label = gtk_label_new_with_mnemonic(_("Ic_on:"));
	gtk_widget_set_halign(icon_label, GTK_ALIGN_START);
	gtk_grid_attach(panel_table, icon_label, 0, panel_row, 1, 1);

	m_icon_button = gtk_button_new();
	gtk_widget_set_halign(m_icon_button, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget(GTK_LABEL(icon_label), m_icon_button);
	gtk_grid_attach(panel_table, m_icon_button, 1, panel_row, 1, 1);
	gtk_size_group_add_widget(label_size_group, icon_label);

	connect(m_icon_button, "clicked",
		[this](GtkButton*)
		{
			choose_icon();
		});

	m_icon = gtk_image_new_from_icon_name(m_settings->button_icon_name, GTK_ICON_SIZE_DIALOG);
	gtk_container_add(GTK_CONTAINER(m_icon_button), m_icon);
	gtk_widget_set_sensitive(m_icon_button, static_cast<bool>(m_settings->button_icon_visible));
	++panel_row;

	connect(m_button_icon_visible, "toggled",
		[this](GtkToggleButton* button)
		{
			const bool active = gtk_toggle_button_get_active(button);
			m_settings->button_icon_visible = active;
			m_plugin->set_button_style(m_plugin->get_button_style());
			gtk_widget_set_sensitive(m_icon_button, active);
			if (m_button_single_row)
			{
				gtk_widget_set_sensitive(m_button_single_row,
					active && !static_cast<bool>(m_settings->button_title_visible));
			}
			refresh_customized_indicator();
		});

	// Single-row panel layout
	m_button_single_row = gtk_check_button_new_with_mnemonic(_("Use a single panel _row"));
	gtk_grid_attach(panel_table, m_button_single_row, 0, panel_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_single_row),
		static_cast<bool>(m_settings->button_single_row));
	// Single-row is meaningful only for icon-only buttons (FR-010 §Panel plugin).
	gtk_widget_set_sensitive(m_button_single_row,
		static_cast<bool>(m_settings->button_icon_visible)
		&& !static_cast<bool>(m_settings->button_title_visible));

	connect(m_button_single_row, "toggled",
		[this](GtkToggleButton* button)
		{
			m_settings->button_single_row = gtk_toggle_button_get_active(button);
			m_plugin->set_button_style(m_plugin->get_button_style());
			refresh_customized_indicator();
		});

	// =========================================================================
	// 3. General menu settings section (T016 / T017)
	// =========================================================================
	GtkGrid* menu_table = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(menu_table, 12);
	gtk_grid_set_row_spacing(menu_table, 6);

	GtkWidget* menu_frame = make_aligned_frame(_("General menu settings"), GTK_WIDGET(menu_table));
	gtk_box_pack_start(page, menu_frame, false, false, 0);

	int menu_row = 0;

	// Layout mode
	GtkWidget* layout_label = gtk_label_new_with_mnemonic(_("_Layout mode:"));
	gtk_widget_set_halign(layout_label, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, layout_label, 0, menu_row, 1, 1);

	m_layout_mode_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_layout_mode_combo), "docked", _("Docked"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_layout_mode_combo), "fullscreen", _("FullScreen"));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_layout_mode_combo),
		static_cast<const gchar*>(m_settings->layout_mode));
	gtk_widget_set_halign(m_layout_mode_combo, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, m_layout_mode_combo, 1, menu_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(layout_label), m_layout_mode_combo);
	gtk_size_group_add_widget(label_size_group, layout_label);
	gtk_size_group_add_widget(control_size_group, m_layout_mode_combo);
	++menu_row;

	connect(m_layout_mode_combo, "changed",
		[this](GtkComboBox* combo)
		{
			if (m_programmatic_update)
				return;
			const gchar* val = gtk_combo_box_get_active_id(combo);
			if (!val)
				return;
			m_settings->layout_mode = val;
			// Live transition is driven by the shared property-changed handler
			// (apply_layout_mode_sensitivity); calling it here keeps the dialog
			// in sync even if the handler hasn't fired yet for the local channel.
			apply_layout_mode_sensitivity();
			update_grid_controls_state();
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Panel gap
	GtkWidget* panel_gap_label = gtk_label_new_with_mnemonic(_("_Panel gap:"));
	gtk_widget_set_halign(panel_gap_label, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, panel_gap_label, 0, menu_row, 1, 1);

	m_panel_gap = gtk_spin_button_new_with_range(0, 50, 1);
	gtk_widget_set_halign(m_panel_gap, GTK_ALIGN_START);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_panel_gap), m_settings->panel_gap);
	gtk_grid_attach(menu_table, m_panel_gap, 1, menu_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(panel_gap_label), m_panel_gap);
	gtk_size_group_add_widget(label_size_group, panel_gap_label);
	gtk_size_group_add_widget(control_size_group, m_panel_gap);
	++menu_row;

	connect(m_panel_gap, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->panel_gap = gtk_spin_button_get_value_as_int(button);
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Menu width (enable-when-docked)
	GtkWidget* width_label = gtk_label_new_with_mnemonic(_("Menu _width:"));
	gtk_widget_set_halign(width_label, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, width_label, 0, menu_row, 1, 1);

	m_menu_width = gtk_spin_button_new_with_range(10, SHRT_MAX, 1);
	gtk_widget_set_halign(m_menu_width, GTK_ALIGN_START);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_width), m_settings->menu_width);
	gtk_grid_attach(menu_table, m_menu_width, 1, menu_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(width_label), m_menu_width);
	gtk_size_group_add_widget(label_size_group, width_label);
	gtk_size_group_add_widget(control_size_group, m_menu_width);
	++menu_row;

	connect(m_menu_width, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->menu_width = gtk_spin_button_get_value_as_int(button);
		});

	// Menu height (enable-when-docked)
	GtkWidget* height_label = gtk_label_new_with_mnemonic(_("Menu _height:"));
	gtk_widget_set_halign(height_label, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, height_label, 0, menu_row, 1, 1);

	m_menu_height = gtk_spin_button_new_with_range(10, SHRT_MAX, 1);
	gtk_widget_set_halign(m_menu_height, GTK_ALIGN_START);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_height), m_settings->menu_height);
	gtk_grid_attach(menu_table, m_menu_height, 1, menu_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(height_label), m_menu_height);
	gtk_size_group_add_widget(label_size_group, height_label);
	gtk_size_group_add_widget(control_size_group, m_menu_height);
	++menu_row;

	connect(m_menu_height, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->menu_height = gtk_spin_button_get_value_as_int(button);
		});

	// Corner radius
	GtkWidget* corner_label = gtk_label_new_with_mnemonic(_("_Corner radius:"));
	gtk_widget_set_halign(corner_label, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, corner_label, 0, menu_row, 1, 1);

	m_corner_radius = gtk_spin_button_new_with_range(0, 24, 1);
	gtk_widget_set_halign(m_corner_radius, GTK_ALIGN_START);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_corner_radius), m_settings->corner_radius);
	gtk_grid_attach(menu_table, m_corner_radius, 1, menu_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(corner_label), m_corner_radius);
	gtk_size_group_add_widget(label_size_group, corner_label);
	gtk_size_group_add_widget(control_size_group, m_corner_radius);
	++menu_row;

	connect(m_corner_radius, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->corner_radius = gtk_spin_button_get_value_as_int(button);
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Full-screen opacity (enable-when-fullscreen) — schema v2 key.
	GtkWidget* fso_label = gtk_label_new_with_mnemonic(_("F_ull-screen opacity:"));
	gtk_widget_set_halign(fso_label, GTK_ALIGN_START);
	gtk_grid_attach(menu_table, fso_label, 0, menu_row, 1, 1);

	m_full_screen_opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_widget_set_hexpand(m_full_screen_opacity, true);
	gtk_scale_set_value_pos(GTK_SCALE(m_full_screen_opacity), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(m_full_screen_opacity), m_settings->full_screen_opacity);
	gtk_grid_attach(menu_table, m_full_screen_opacity, 1, menu_row, 1, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(fso_label), m_full_screen_opacity);
	gtk_size_group_add_widget(label_size_group, fso_label);
	++menu_row;

	connect(m_full_screen_opacity, "value-changed",
		[this](GtkRange* range)
		{
			if (m_programmatic_update)
				return;
			m_settings->full_screen_opacity = static_cast<int>(gtk_range_get_value(range));
			m_plugin->reload_menu();
			refresh_customized_indicator();
		});

	// Stay visible when focus is lost (FR-013: lives only in General).
	m_stay_on_focus_out = gtk_check_button_new_with_mnemonic(_("Stay _visible when focus is lost"));
	gtk_grid_attach(menu_table, m_stay_on_focus_out, 0, menu_row, 2, 1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_stay_on_focus_out), m_settings->stay_on_focus_out);
	++menu_row;

	connect(m_stay_on_focus_out, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->stay_on_focus_out = gtk_toggle_button_get_active(button);
		});

	// Layout-mode-driven live sensitivity (T017).
	m_layout_enable_when_docked.push_back(m_menu_width);
	m_layout_enable_when_docked.push_back(m_menu_height);
	m_layout_enable_when_docked.push_back(width_label);
	m_layout_enable_when_docked.push_back(height_label);
	m_layout_enable_when_fullscreen.push_back(m_full_screen_opacity);
	m_layout_enable_when_fullscreen.push_back(fso_label);

	return wrap_in_scrolled(GTK_WIDGET(page));
}
