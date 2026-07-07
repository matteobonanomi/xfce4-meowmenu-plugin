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

/* set_menu_opacity_compositing_state:
 * @scale: the Menu-opacity scale.
 * @label: its mnemonic label.
 *
 * Reflects whether the control's screen is composited. Opacity has no visible
 * effect without a compositor (the renderer paints fully solid in that case), so
 * the control is made insensitive with a tooltip explaining why; with a
 * compositor present it is sensitive and carries no tooltip. Called once when the
 * tab builds and again from the screen's composited-changed signal.
 */
static void set_menu_opacity_compositing_state(GtkWidget* scale, GtkWidget* label)
{
	GdkScreen* screen = gtk_widget_get_screen(scale);
	const gboolean composited = screen && gdk_screen_is_composited(screen);
	gtk_widget_set_sensitive(scale, composited);
	gtk_widget_set_sensitive(label, composited);
	const gchar* tip = composited
		? nullptr
		: _("Opacity requires a running compositor; without one the menu is always solid.");
	gtk_widget_set_tooltip_text(scale, tip);
	gtk_widget_set_tooltip_text(label, tip);
}

//-----------------------------------------------------------------------------

/* init_general_tab:
 *
 * Builds the General tab. Each of the three section frames lays its controls
 * across two equal-width columns (C1 | C2) built with make_two_column_section();
 * the homogeneous columns share one midpoint across all three sections. Sections
 * (top-to-bottom):
 *   1. Preset          — select/save/import/export presets; a "?" help control
 *                        reveals the active preset's description on hover/focus.
 *   2. Panel plugin    — button title/icon visibility, title, icon picker,
 *                        single-row toggle.
 *   3. General menu    — layout-mode, menu-width/height, panel-gap,
 *                        corner-radius, menu-opacity, stay-on-focus-out.
 *
 * Width, height, panel gap and corner radius register a (widget, LayoutControl)
 * pair in m_layout_controls so the live handler installed by
 * install_layout_mode_handler() can flip their state through the
 * control_enabled() matrix on /layout-mode change without a dialog reopen. Menu
 * opacity is sensitive in every layout mode and so registers no such pair.
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
	// The section content is a vertical box so the full-width Wayland warning
	// info-bar can sit above the two-column control grid.
	GtkWidget* preset_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	// FullScreen warning InfoBar — shown when fullscreen is active but
	// gtk-layer-shell is unavailable. HACK: at build time we don't know whether
	// the running compositor will honour layer-shell, so we conservatively show
	// the warning only when the dependency isn't compiled in. It stays full-width
	// at the top of the Preset section.
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
		gtk_box_pack_start(GTK_BOX(preset_content), infobar, false, false, 0);
	}
#endif

	GtkWidget* preset_grid = make_two_column_section();
	gtk_box_pack_start(GTK_BOX(preset_content), preset_grid, false, false, 0);

	GtkWidget* preset_frame = make_aligned_frame(_("Preset"), preset_content);
	gtk_box_pack_start(page, preset_frame, false, false, 0);

	// --- Selector row (row 0): combo in C1; "?" help control in C2. ---
	GtkWidget* preset_label = gtk_label_new_with_mnemonic(_("_Preset:"));

	// Model-driven selector: an explicit GtkListStore lets each row carry its own
	// Pango weight/style so refresh_preset_combo() can render built-ins bold,
	// saved customs standard, and the "Unsaved custom" placeholder italic. The
	// id column keeps gtk_combo_box_set_active_id()-based selection working.
	m_preset_model = gtk_list_store_new(PRESET_N_COLS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
	m_preset_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(m_preset_model));
	g_object_unref(m_preset_model); // combo holds the only owning reference
	gtk_combo_box_set_id_column(GTK_COMBO_BOX(m_preset_combo), PRESET_COL_ID);
	{
		GtkCellRenderer* cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(m_preset_combo), cell, true);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(m_preset_combo), cell,
			"text",   PRESET_COL_LABEL,
			"weight", PRESET_COL_WEIGHT,
			"style",  PRESET_COL_STYLE,
			nullptr);
	}
	add_form_row(preset_grid, COLUMN_C1, 0, preset_label, m_preset_combo, true, nullptr);
	gtk_label_set_mnemonic_widget(GTK_LABEL(preset_label), m_preset_combo);

	// "?" help control: reveals the active preset's description on hover/focus,
	// replacing the former always-visible inline description paragraph. Its
	// tooltip is refreshed by the preset-sync path (refresh_customized_indicator
	// / sync_preset_widgets) and by the combo "changed" handler below.
	m_preset_help = make_help_button(_("Show the selected preset's description"));
	add_form_row(preset_grid, COLUMN_C2, 0, nullptr, m_preset_help, false, nullptr);

	// --- Custom-preset actions (row 1): Save/Rename in C1, Delete in C2. ---
	GtkWidget* custom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	add_form_row(preset_grid, COLUMN_C1, 1, nullptr, custom_box, false, nullptr);

	GtkWidget* delete_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	add_form_row(preset_grid, COLUMN_C2, 1, nullptr, delete_box, false, nullptr);

	// --- Reset/IO actions (row 2): Reset preset/defaults in C1, Export/Import in C2. ---
	GtkWidget* reset_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	add_form_row(preset_grid, COLUMN_C1, 2, nullptr, reset_box, false, nullptr);

	GtkWidget* io_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	add_form_row(preset_grid, COLUMN_C2, 2, nullptr, io_box, false, nullptr);

	GtkWidget* reset_preset_btn = gtk_button_new_with_mnemonic(_("_Reset preset"));
	gtk_box_pack_start(GTK_BOX(reset_box), reset_preset_btn, false, false, 0);

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
	gtk_box_pack_start(GTK_BOX(custom_box), save_btn, false, false, 0);

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
					// save_current_as_user_preset set /current-preset-id to the new
					// uuid and re-enumerated, so the rebuilt combo's recompute lands
					// on the new row (settings match it → not diverged), selected and
					// named, with no dialog reopen (FR-003/004/005, SC-001).
					refresh_preset_combo();
					m_plugin->reload_menu();
				}
			}
			gtk_widget_destroy(dlg);
		});

	m_preset_rename_btn = gtk_button_new_with_mnemonic(_("Re_name…"));
	gtk_widget_set_sensitive(m_preset_rename_btn, false);
	gtk_box_pack_start(GTK_BOX(custom_box), m_preset_rename_btn, false, false, 0);

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
	gtk_box_pack_start(GTK_BOX(delete_box), m_preset_delete_btn, false, false, 0);

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
				// delete_user_preset clears /current-preset-id when the removed
				// preset was the active one (the only deletable case from this UI,
				// since Delete is enabled only for the applied custom). Apply Modern
				// as the fallback BEFORE rebuilding the combo so the recompute lands
				// directly on Modern instead of flashing "Unsaved custom" (FR-009).
				delete_user_preset(uuid, *m_settings);
				apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_preset_combo(); // drops the deleted row; recompute selects Modern
			}
		});

	// Export / Import actions populate io_box (C2 row 2, declared above).
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
				// Apply the imported preset so it both appears in the dropdown and
				// takes effect (AC US4#2): import writes the /presets/<uuid>/ subtree
				// but not /current-preset-id, so without this the freshly imported
				// preset would list but not become the active selection.
				const LayoutPreset* imported = find_preset_by_id(result.new_uuid);
				if (imported)
				{
					apply_preset(*imported, *m_settings);
					m_plugin->reload_menu();
					sync_preset_widgets();
				}
				refresh_preset_combo();
			}
			else if (result.status != ImportStatus::ParseError)
			{
				xfce_dialog_show_error(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
					nullptr, "%s: %s", _("Could not import preset"),
					result.error_message.c_str());
			}
		});

	// Reset to defaults — full channel reset, kept here as the most logical
	// action button for the Preset hub. Sits beside Reset preset in C1.
	GtkWidget* defaults_btn = gtk_button_new_with_mnemonic(_("Reset to _defaults"));
	gtk_box_pack_start(GTK_BOX(reset_box), defaults_btn, false, false, 0);

	connect(defaults_btn, "clicked",
		[this](GtkButton*)
		{
			if (xfce_dialog_confirm(GTK_WINDOW(gtk_widget_get_toplevel(m_window)),
				"edit-undo", _("_Reset"),
				_("All settings will be reset to defaults and the Modern preset will be applied."),
				_("Reset all settings to defaults?")))
			{
				// Hard reset: clear all plugin properties except saved user
				// presets. The channel is property-base-anchored, so reset must
				// run on base-relative paths — reset_settings_to_defaults() strips
				// the base get_properties() embeds (see that helper for why a
				// full path would silently no-op here).
				reset_settings_to_defaults(m_settings->channel, m_plugin->get_property_base());

				apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *m_settings);
				m_plugin->reload_menu();
				sync_preset_widgets();
				refresh_preset_combo("modern");
				refresh_customized_indicator();
			}
		});

	// Populate the preset combo. refresh_preset_combo() rebuilds the rows and then
	// the divergence recompute selects the applied preset's row (or the italic
	// "Unsaved custom" placeholder when the live settings already diverge on
	// open) and sets the description — the field is never blank (FR-005).
	refresh_preset_combo();

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
			// Reveal the selected preset's description via the "?" help control's
			// hover/focus tooltip (the always-visible inline paragraph is gone).
			gtk_widget_set_tooltip_text(m_preset_help, _(preset->description.c_str()));
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
			// Recompute drives the active row, the description, and Rename/Delete/
			// Export sensitivity from the freshly-applied /current-preset-id.
			refresh_customized_indicator();
		});

	// =========================================================================
	// 2. Panel plugin section
	// =========================================================================
	// Per-column label size group lines the C1 / C2 label colons up within a
	// half; it must not span the two halves (the homogeneous grid owns the 50/50
	// split). Only the Title/Icon labels need it here.
	GtkSizeGroup* panel_c1_labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* panel_c2_labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	GtkWidget* panel_grid = make_two_column_section();
	GtkWidget* panel_frame = make_aligned_frame(_("Panel plugin"), panel_grid);
	gtk_box_pack_start(page, panel_frame, false, false, 0);

	// Row 0: visibility toggles — title in C1, icon in C2.
	m_button_title_visible = gtk_check_button_new_with_mnemonic(_("Show panel button _title"));
	add_form_row(panel_grid, COLUMN_C1, 0, nullptr, m_button_title_visible, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_title_visible),
		static_cast<bool>(m_settings->button_title_visible));

	// Row 1 C1: Title entry (wide-fill).
	GtkWidget* title_label = gtk_label_new_with_mnemonic(_("T_itle:"));
	m_title = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(m_title), m_settings->button_title);
	add_form_row(panel_grid, COLUMN_C1, 1, title_label, m_title, true, panel_c1_labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(title_label), m_title);
	gtk_widget_set_sensitive(m_title, static_cast<bool>(m_settings->button_title_visible));

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

	// Row 0 C2: Show panel button icon toggle.
	m_button_icon_visible = gtk_check_button_new_with_mnemonic(_("Show panel button _icon"));
	add_form_row(panel_grid, COLUMN_C2, 0, nullptr, m_button_icon_visible, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button_icon_visible),
		static_cast<bool>(m_settings->button_icon_visible));

	// Row 1 C2: Icon picker (narrow).
	GtkWidget* icon_label = gtk_label_new_with_mnemonic(_("Ic_on:"));
	m_icon_button = gtk_button_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(icon_label), m_icon_button);
	add_form_row(panel_grid, COLUMN_C2, 1, icon_label, m_icon_button, false, panel_c2_labels);

	connect(m_icon_button, "clicked",
		[this](GtkButton*)
		{
			choose_icon();
		});

	m_icon = gtk_image_new_from_icon_name(m_settings->button_icon_name, GTK_ICON_SIZE_DIALOG);
	gtk_container_add(GTK_CONTAINER(m_icon_button), m_icon);
	gtk_widget_set_sensitive(m_icon_button, static_cast<bool>(m_settings->button_icon_visible));

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

	// Row 2 C1: single-row panel layout (C2 stays empty; C1 holds half width).
	m_button_single_row = gtk_check_button_new_with_mnemonic(_("Use a single panel _row"));
	add_form_row(panel_grid, COLUMN_C1, 2, nullptr, m_button_single_row, false, nullptr);
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
	// 3. General menu settings section
	// =========================================================================
	// Per-column label size groups align the colons within each half.
	GtkSizeGroup* menu_c1_labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* menu_c2_labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	GtkWidget* menu_grid = make_two_column_section();
	GtkWidget* menu_frame = make_aligned_frame(_("General menu settings"), menu_grid);
	gtk_box_pack_start(page, menu_frame, false, false, 0);

	// Row 0 C1: Layout mode (narrow); C2 left empty.
	GtkWidget* layout_label = gtk_label_new_with_mnemonic(_("_Layout mode:"));

	m_layout_mode_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_layout_mode_combo), "docked", _("Docked"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_layout_mode_combo), "centered", _("Centered"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_layout_mode_combo), "fullscreen", _("FullScreen"));
	// Select the active entry from the classified mode so a stale/unknown stored
	// value falls back to Docked instead of leaving the combo blank (C-5).
	const gchar* active_id = "docked";
	switch (WhiskerMenu::layout_mode_from_key(m_settings->layout_mode))
	{
	case WhiskerMenu::LayoutMode::Centered:   active_id = "centered";   break;
	case WhiskerMenu::LayoutMode::FullScreen: active_id = "fullscreen"; break;
	case WhiskerMenu::LayoutMode::Docked:     active_id = "docked";     break;
	}
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(m_layout_mode_combo), active_id);
	add_form_row(menu_grid, COLUMN_C1, 0, layout_label, m_layout_mode_combo, false, menu_c1_labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(layout_label), m_layout_mode_combo);

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
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// Row 2 C1: Panel gap (narrow).
	GtkWidget* panel_gap_label = gtk_label_new_with_mnemonic(_("_Panel gap:"));
	m_panel_gap = gtk_spin_button_new_with_range(0, 50, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_panel_gap), m_settings->panel_gap);
	add_form_row(menu_grid, COLUMN_C1, 2, panel_gap_label, m_panel_gap, false, menu_c1_labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(panel_gap_label), m_panel_gap);

	connect(m_panel_gap, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->panel_gap = gtk_spin_button_get_value_as_int(button);
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// Row 1 C1: Menu width (narrow, enable-when-docked).
	GtkWidget* width_label = gtk_label_new_with_mnemonic(_("Menu _width:"));
	m_menu_width = gtk_spin_button_new_with_range(10, SHRT_MAX, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_width), m_settings->menu_width);
	add_form_row(menu_grid, COLUMN_C1, 1, width_label, m_menu_width, false, menu_c1_labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(width_label), m_menu_width);

	connect(m_menu_width, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->menu_width = gtk_spin_button_get_value_as_int(button);
			m_plugin->refresh_layout();
		});

	// Row 1 C2: Menu height (narrow, enable-when-docked).
	GtkWidget* height_label = gtk_label_new_with_mnemonic(_("Menu _height:"));
	m_menu_height = gtk_spin_button_new_with_range(10, SHRT_MAX, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_menu_height), m_settings->menu_height);
	add_form_row(menu_grid, COLUMN_C2, 1, height_label, m_menu_height, false, menu_c2_labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(height_label), m_menu_height);

	connect(m_menu_height, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->menu_height = gtk_spin_button_get_value_as_int(button);
			m_plugin->refresh_layout();
		});

	// Row 2 C2: Corner radius (narrow).
	GtkWidget* corner_label = gtk_label_new_with_mnemonic(_("_Corner radius:"));
	m_corner_radius = gtk_spin_button_new_with_range(0, 24, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_corner_radius), m_settings->corner_radius);
	add_form_row(menu_grid, COLUMN_C2, 2, corner_label, m_corner_radius, false, menu_c2_labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(corner_label), m_corner_radius);

	connect(m_corner_radius, "value-changed",
		[this](GtkSpinButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->corner_radius = gtk_spin_button_get_value_as_int(button);
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// Row 3 C1: Menu opacity (wide-fill). One control fades the whole menu
	// background uniformly in every layout mode, so it is sensitive in all modes
	// (no LayoutControl registration); compositing availability gates it instead.
	GtkWidget* mo_label = gtk_label_new_with_mnemonic(_("_Menu opacity:"));
	m_menu_opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_scale_set_value_pos(GTK_SCALE(m_menu_opacity), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(m_menu_opacity), m_settings->menu_opacity);
	add_form_row(menu_grid, COLUMN_C1, 3, mo_label, m_menu_opacity, true, menu_c1_labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(mo_label), m_menu_opacity);

	connect(m_menu_opacity, "value-changed",
		[this](GtkRange* range)
		{
			if (m_programmatic_update)
				return;
			m_settings->menu_opacity = static_cast<int>(gtk_range_get_value(range));
			m_plugin->refresh_layout();
			refresh_customized_indicator();
		});

	// HACK: opacity is a compositor feature. Without a compositor the menu always
	// renders solid (the window guards force it), so the control would otherwise
	// look functional while doing nothing. Gate it on compositing — disabled with
	// an explanatory tooltip when absent — and track the screen's
	// composited-changed so it re-enables live if a compositor starts. Best-effort:
	// X11 is the verified path; the renderer fallback is independent of this.
	set_menu_opacity_compositing_state(m_menu_opacity, mo_label);
	if (GdkScreen* mo_screen = gtk_widget_get_screen(m_menu_opacity))
		connect(mo_screen, "composited-changed",
			[this, mo_label](GdkScreen*)
			{
				set_menu_opacity_compositing_state(m_menu_opacity, mo_label);
			});

	// Row 3 C2: Stay visible when focus is lost (control-only).
	m_stay_on_focus_out = gtk_check_button_new_with_mnemonic(_("Stay _visible when focus is lost"));
	add_form_row(menu_grid, COLUMN_C2, 3, nullptr, m_stay_on_focus_out, false, nullptr);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_stay_on_focus_out), m_settings->stay_on_focus_out);

	connect(m_stay_on_focus_out, "toggled",
		[this](GtkToggleButton* button)
		{
			if (m_programmatic_update)
				return;
			m_settings->stay_on_focus_out = gtk_toggle_button_get_active(button);
		});

	// Layout-mode-driven live sensitivity (FR-006). Each control and its label
	// register the same LayoutControl so they grey together; the pure
	// control_enabled() matrix decides each state per mode. Panel gap and corner
	// radius — previously always enabled — are now governed here too (FR-007 greys
	// both in Full-Screen; FR-008 greys the gap in Centered).
	m_layout_controls.push_back({m_menu_width,         WhiskerMenu::LayoutControl::MenuWidth});
	m_layout_controls.push_back({width_label,          WhiskerMenu::LayoutControl::MenuWidth});
	m_layout_controls.push_back({m_menu_height,        WhiskerMenu::LayoutControl::MenuHeight});
	m_layout_controls.push_back({height_label,         WhiskerMenu::LayoutControl::MenuHeight});
	m_layout_controls.push_back({m_panel_gap,          WhiskerMenu::LayoutControl::PanelGap});
	m_layout_controls.push_back({panel_gap_label,      WhiskerMenu::LayoutControl::PanelGap});
	m_layout_controls.push_back({m_corner_radius,      WhiskerMenu::LayoutControl::CornerRadius});
	m_layout_controls.push_back({corner_label,         WhiskerMenu::LayoutControl::CornerRadius});

	return wrap_in_scrolled(GTK_WIDGET(page));
}
