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

#include "search/search-action.h"
#include "settings.h"
#include "ui/slot.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

namespace
{

// NOTE: column indices for the search-actions list-store. Must stay in sync
// with the matching anonymous-namespace enum in settings-dialog.cpp, which
// owns add_action()/remove_action()/edit_search_action_modal() and shares
// the same GtkListStore layout.
enum
{
	COLUMN_NAME,
	COLUMN_PATTERN,
	COLUMN_ACTION,
	N_COLUMNS
};

}

//-----------------------------------------------------------------------------

/* build_search_bar_actions_section:
 * @page: the Search Bar tab's vertical container; must not be NULL.
 *
 * Appends the "Search actions" frame to @page. The frame shows the user's
 * search actions (name + pattern) in a tree-view alongside an Add / Remove /
 * Edit button column; Edit and row-activate open the per-action modal in
 * place of the legacy inline detail panel.
 *
 * Initializes the SettingsDialog members m_actions_model, m_actions_view,
 * m_action_add and m_action_remove, and explicitly nulls the legacy
 * inline-detail widget pointers so destructor null-guards stay valid.
 */
void SettingsDialog::build_search_bar_actions_section(GtkBox* page)
{
	GtkGrid* actions_grid = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(actions_grid, 6);
	gtk_grid_set_row_spacing(actions_grid, 6);

	m_actions_model = gtk_list_store_new(N_COLUMNS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	for (auto action : m_settings->search_actions)
	{
		gtk_list_store_insert_with_values(m_actions_model,
			nullptr, G_MAXINT,
			COLUMN_NAME, action->get_name(),
			COLUMN_PATTERN, action->get_pattern(),
			COLUMN_ACTION, action,
			-1);
	}

	m_actions_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_actions_model)));

	GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Name"),
		renderer, "text", COLUMN_NAME, nullptr);
	gtk_tree_view_column_set_expand(column, true);
	gtk_tree_view_append_column(m_actions_view, column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Pattern"),
		renderer, "text", COLUMN_PATTERN, nullptr);
	gtk_tree_view_column_set_expand(column, true);
	gtk_tree_view_append_column(m_actions_view, column);

	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_actions_view);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(m_actions_view));
	gtk_widget_set_hexpand(sw, true);
	gtk_widget_set_vexpand(sw, true);
	gtk_widget_set_size_request(sw, -1, 160);
	gtk_grid_attach(actions_grid, sw, 0, 0, 1, 1);

	// Add / Remove / Edit buttons
	m_action_add = gtk_button_new();
	gtk_widget_set_tooltip_text(m_action_add, _("Add action"));
	gtk_container_add(GTK_CONTAINER(m_action_add),
		gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON));
	connect(m_action_add, "clicked",
		[this](GtkButton*) { add_action(); });

	m_action_remove = gtk_button_new();
	gtk_widget_set_tooltip_text(m_action_remove, _("Remove selected action"));
	gtk_container_add(GTK_CONTAINER(m_action_remove),
		gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_BUTTON));
	connect(m_action_remove, "clicked",
		[this](GtkButton*) { remove_action(); });

	// Edit button — the "hamburger" of the actions row; opens the per-
	// action modal. open-menu-symbolic matches the GTK convention for the
	// hamburger glyph.
	GtkWidget* edit_btn = gtk_button_new();
	gtk_widget_set_tooltip_text(edit_btn, _("Edit selected action…"));
	gtk_container_add(GTK_CONTAINER(edit_btn),
		gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
	connect(edit_btn, "clicked",
		[this](GtkButton*)
		{
			SearchAction* action = get_selected_action();
			if (!action)
				return;
			edit_search_action_modal(action);
		});

	// Double-clicking a row should also open the modal (UX shortcut).
	connect(m_actions_view, "row-activated",
		[this](GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*)
		{
			SearchAction* action = get_selected_action();
			if (action)
				edit_search_action_modal(action);
		});

	GtkBox* actions_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	gtk_widget_set_halign(GTK_WIDGET(actions_box), GTK_ALIGN_START);
	gtk_box_pack_start(actions_box, m_action_add, false, false, 0);
	gtk_box_pack_start(actions_box, m_action_remove, false, false, 0);
	gtk_box_pack_start(actions_box, edit_btn, false, false, 0);
	gtk_grid_attach(actions_grid, GTK_WIDGET(actions_box), 1, 0, 1, 1);

	// The legacy inline-detail panel is replaced by the modal; keep the
	// detail entry widgets nullptr so callers that null-guard them stay
	// safe and the destructor's unused-widget paths remain valid.
	m_action_name = nullptr;
	m_action_pattern = nullptr;
	m_action_command = nullptr;
	m_action_regex = nullptr;

	const bool has_rows = !m_settings->search_actions.empty();
	gtk_widget_set_sensitive(m_action_remove, has_rows);
	gtk_widget_set_sensitive(edit_btn, has_rows);
	if (has_rows)
	{
		GtkTreePath* path = gtk_tree_path_new_first();
		gtk_tree_view_set_cursor(m_actions_view, path, nullptr, false);
		gtk_tree_path_free(path);
	}

	// Re-evaluate Edit-button sensitivity whenever the selection toggles.
	connect(selection, "changed",
		[edit_btn](GtkTreeSelection* sel)
		{
			GtkTreeIter iter;
			GtkTreeModel* mdl;
			const bool sel_ok = gtk_tree_selection_get_selected(sel, &mdl, &iter);
			gtk_widget_set_sensitive(edit_btn, sel_ok);
		});

	gtk_box_pack_start(page,
		make_aligned_frame(_("Search actions"), GTK_WIDGET(actions_grid)),
		true, true, 0);
}
