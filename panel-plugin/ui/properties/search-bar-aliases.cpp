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

#include "applications-page.h"
#include "launcher.h"
#include "plugin.h"
#include "settings.h"
#include "slot.h"
#include "window.h"

#include <string>
#include <vector>

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

namespace
{

// Column indices for the per-application aliases list-store. Local to this
// translation unit; the same scheme is re-declared inside the edit/add/remove
// lambdas to keep them self-contained.
enum { ALIAS_COL_NAME, ALIAS_COL_TERMS, ALIAS_COL_ID, ALIAS_N_COLS };

}

//-----------------------------------------------------------------------------

/* build_search_bar_aliases_section:
 * @page: the Search Bar tab's vertical container; must not be NULL.
 *
 * Appends the Aliases frame to @page. The frame contains a tree-view of
 * desktop-id → comma-separated alias terms (editable in place) and an
 * Add/Remove button row. Edits round-trip through Settings::set_aliases().
 *
 * Initializes the SettingsDialog members m_aliases_model, m_aliases_view,
 * m_alias_add and m_alias_remove. Safe to call once per dialog construction.
 */
void SettingsDialog::build_search_bar_aliases_section(GtkBox* page)
{
	m_aliases_model = gtk_list_store_new(ALIAS_N_COLS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	if (m_plugin->get_window())
	{
		const auto launchers = m_plugin->get_window()->get_applications()->find_all();
		for (const auto* launcher : launchers)
		{
			const char* id = launcher->get_desktop_id();
			const auto& terms = m_settings->get_aliases(id);
			if (terms.empty())
				continue;
			std::string joined;
			for (size_t i = 0; i < terms.size(); ++i)
			{
				if (i) joined += ", ";
				joined += terms[i];
			}
			gtk_list_store_insert_with_values(m_aliases_model, nullptr, G_MAXINT,
				ALIAS_COL_NAME, launcher->get_display_name(),
				ALIAS_COL_TERMS, joined.c_str(),
				ALIAS_COL_ID, id,
				-1);
		}
	}

	m_aliases_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_aliases_model)));

	GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes(
		_("Application"), renderer, "text", ALIAS_COL_NAME, nullptr);
	gtk_tree_view_column_set_expand(col, true);
	gtk_tree_view_append_column(m_aliases_view, col);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, nullptr);
	col = gtk_tree_view_column_new_with_attributes(
		_("Aliases (comma-separated)"), renderer, "text", ALIAS_COL_TERMS, nullptr);
	gtk_tree_view_column_set_expand(col, true);
	gtk_tree_view_append_column(m_aliases_view, col);

	connect(renderer, "edited",
		[this](GtkCellRendererText*, const gchar* path_str, const gchar* new_text)
		{
			GtkTreeIter iter;
			if (!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(m_aliases_model),
					&iter, path_str))
				return;
			gtk_list_store_set(m_aliases_model, &iter, ALIAS_COL_TERMS, new_text, -1);
			gchar* id_val = nullptr;
			gtk_tree_model_get(GTK_TREE_MODEL(m_aliases_model), &iter,
				ALIAS_COL_ID, &id_val, -1);
			if (id_val)
			{
				std::vector<std::string> terms;
				gchar** parts = g_strsplit(new_text, ",", -1);
				for (int i = 0; parts[i]; ++i)
				{
					gchar* stripped = g_strstrip(parts[i]);
					if (*stripped)
						terms.emplace_back(stripped);
				}
				g_strfreev(parts);
				m_settings->set_aliases(std::string(id_val), terms);
				g_free(id_val);
			}
		});

	GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_widget_set_size_request(scrolled, -1, 120);
	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(m_aliases_view));

	m_alias_add = gtk_button_new_with_mnemonic(_("_Add"));
	m_alias_remove = gtk_button_new_with_mnemonic(_("_Remove"));

	connect(m_alias_add, "clicked",
		[this](GtkButton*)
		{
			if (!m_plugin->get_window())
				return;
			GtkWidget* dialog = gtk_dialog_new_with_buttons(
				_("Choose Application"),
				GTK_WINDOW(m_window),
				GTK_DIALOG_MODAL,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Add"),    GTK_RESPONSE_ACCEPT,
				nullptr);
			GtkListStore* app_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
			const auto launchers = m_plugin->get_window()->get_applications()->find_all();
			for (const auto* launcher : launchers)
			{
				gtk_list_store_insert_with_values(app_store, nullptr, G_MAXINT,
					0, launcher->get_display_name(),
					1, launcher->get_desktop_id(),
					-1);
			}
			GtkWidget* app_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app_store));
			g_object_unref(app_store);
			GtkCellRenderer* rend = gtk_cell_renderer_text_new();
			gtk_tree_view_append_column(GTK_TREE_VIEW(app_view),
				gtk_tree_view_column_new_with_attributes(
					_("Application"), rend, "text", 0, nullptr));
			GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
			gtk_widget_set_size_request(sw, 300, 240);
			gtk_container_add(GTK_CONTAINER(sw), app_view);
			gtk_box_pack_start(
				GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
				sw, true, true, 6);
			gtk_widget_show_all(dialog);
			if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
			{
				GtkTreeSelection* sel = gtk_tree_view_get_selection(
					GTK_TREE_VIEW(app_view));
				GtkTreeIter it;
				GtkTreeModel* mdl;
				if (gtk_tree_selection_get_selected(sel, &mdl, &it))
				{
					gchar* name = nullptr;
					gchar* id   = nullptr;
					gtk_tree_model_get(mdl, &it, 0, &name, 1, &id, -1);
					gtk_list_store_insert_with_values(m_aliases_model,
						nullptr, G_MAXINT,
						ALIAS_COL_NAME, name,
						ALIAS_COL_TERMS, "",
						ALIAS_COL_ID, id,
						-1);
					g_free(name);
					g_free(id);
				}
			}
			gtk_widget_destroy(dialog);
			gtk_widget_set_sensitive(m_alias_remove, true);
		});

	connect(m_alias_remove, "clicked",
		[this](GtkButton*)
		{
			GtkTreeSelection* sel = gtk_tree_view_get_selection(m_aliases_view);
			GtkTreeIter iter;
			GtkTreeModel* mdl;
			if (!gtk_tree_selection_get_selected(sel, &mdl, &iter))
				return;
			gchar* id_val = nullptr;
			gtk_tree_model_get(mdl, &iter, ALIAS_COL_ID, &id_val, -1);
			if (id_val)
			{
				m_settings->set_aliases(std::string(id_val), {});
				g_free(id_val);
			}
			gtk_list_store_remove(m_aliases_model, &iter);
			const bool has_rows = gtk_tree_model_iter_n_children(
				GTK_TREE_MODEL(m_aliases_model), nullptr) > 0;
			gtk_widget_set_sensitive(m_alias_remove, has_rows);
		});

	const bool has_rows = gtk_tree_model_iter_n_children(
		GTK_TREE_MODEL(m_aliases_model), nullptr) > 0;
	gtk_widget_set_sensitive(m_alias_remove, has_rows);

	GtkWidget* btn_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(btn_box), 6);
	gtk_box_pack_start(GTK_BOX(btn_box), m_alias_add, false, false, 0);
	gtk_box_pack_start(GTK_BOX(btn_box), m_alias_remove, false, false, 0);

	GtkBox* alias_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	gtk_box_pack_start(alias_vbox, scrolled, true, true, 0);
	gtk_box_pack_start(alias_vbox, btn_box, false, false, 0);

	gtk_box_pack_start(page,
		make_aligned_frame(_("Aliases"), GTK_WIDGET(alias_vbox)),
		false, false, 0);
}
