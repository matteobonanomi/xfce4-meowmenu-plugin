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

#include "launcher-tree-view.h"

#include "launcher/category.h"
#include "launcher/launcher-safety.h"
#include "icon-renderer.h"
#include "settings.h"
#include "slot.h"

#include <libxfce4ui/libxfce4ui.h>
#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <vector>

using namespace WhiskerMenu;

namespace
{

struct DisplayedTreeRow
{
	GtkTreePath* path;
	GdkRectangle rectangle;
};

void collect_displayed_rows(GtkTreeView* view, GtkTreeModel* model,
		GtkTreeIter* parent, std::vector<DisplayedTreeRow>* rows)
{
	GtkTreeIter iter;
	bool has_iter = parent
			? gtk_tree_model_iter_children(model, &iter, parent)
			: gtk_tree_model_get_iter_first(model, &iter);
	while (has_iter)
	{
		GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
		GtkTreeViewColumn* column = gtk_tree_view_get_column(view, 0);
		GdkRectangle rectangle = {};
		gtk_tree_view_get_background_area(view, path, column, &rectangle);

		bool selectable = rectangle.height > 0;
		if (selectable && gtk_tree_model_get_n_columns(model)
				> LauncherView::COLUMN_TEXT)
		{
			gchar* text = nullptr;
			gtk_tree_model_get(model, &iter, LauncherView::COLUMN_TEXT,
					&text, -1);
			selectable = text && *text != '\0';
			g_free(text);
		}
		if (selectable)
			rows->push_back({gtk_tree_path_copy(path), rectangle});

		if (gtk_tree_view_row_expanded(view, path))
			collect_displayed_rows(view, model, &iter, rows);

		gtk_tree_path_free(path);
		has_iter = gtk_tree_model_iter_next(model, &iter);
	}
}

void free_displayed_rows(std::vector<DisplayedTreeRow>* rows)
{
	for (const DisplayedTreeRow& row : *rows)
		gtk_tree_path_free(row.path);
	rows->clear();
}

} // namespace

/* launcher_tree_view_find_directional_path:
 * @view: the live tree view whose displayed rows are inspected.
 * @origin: currently selected or focused model path.
 * @direction: physical direction; only Up and Down have list neighbours.
 *
 * Returns the adjacent displayed selectable row without wrapping. The caller
 * owns the returned path.
 */
GtkTreePath* WhiskerMenu::launcher_tree_view_find_directional_path(
		GtkTreeView* view, GtkTreePath* origin,
		Keyboard::PhysicalDirection direction)
{
	if (!view || !origin || (direction != Keyboard::PhysicalDirection::Up
			&& direction != Keyboard::PhysicalDirection::Down))
		return nullptr;
	GtkTreeModel* model = gtk_tree_view_get_model(view);
	if (!model)
		return nullptr;

	std::vector<DisplayedTreeRow> rows;
	collect_displayed_rows(view, model, nullptr, &rows);
	std::size_t index = rows.size();
	for (std::size_t i = 0; i < rows.size(); ++i)
	{
		if (gtk_tree_path_compare(rows[i].path, origin) == 0)
		{
			index = i;
			break;
		}
	}
	GtkTreePath* result = nullptr;
	if (index != rows.size())
	{
		if (direction == Keyboard::PhysicalDirection::Up && index > 0)
			result = gtk_tree_path_copy(rows[index - 1].path);
		else if (direction == Keyboard::PhysicalDirection::Down
				&& index + 1 < rows.size())
			result = gtk_tree_path_copy(rows[index + 1].path);
	}
	free_displayed_rows(&rows);
	return result;
}

/* launcher_tree_view_get_path_rectangle:
 * @view: live tree view containing @path.
 * @path: current displayed model path.
 * @rectangle: output rectangle in the menu toplevel coordinates.
 *
 * Translates the current row allocation on every call so scrolling and live
 * layout changes cannot leave a cached navigation edge behind.
 */
bool WhiskerMenu::launcher_tree_view_get_path_rectangle(GtkTreeView* view,
		GtkTreePath* path, Keyboard::NavigationRect* rectangle)
{
	if (!view || !path || !rectangle)
		return false;
	GtkTreeModel* model = gtk_tree_view_get_model(view);
	GtkTreeIter iter;
	if (!model || !gtk_tree_model_get_iter(model, &iter, path))
		return false;
	gchar* text = nullptr;
	gtk_tree_model_get(model, &iter, LauncherView::COLUMN_TEXT, &text, -1);
	const bool selectable = text && *text != '\0';
	g_free(text);
	if (!selectable)
		return false;
	GtkTreeViewColumn* column = gtk_tree_view_get_column(view, 0);
	GdkRectangle local = {};
	gtk_tree_view_get_background_area(view, path, column, &local);
	if (local.width <= 0 || local.height <= 0)
		return false;
	GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
	int x = local.x;
	int y = local.y;
	if (toplevel && toplevel != GTK_WIDGET(view))
	{
		int translated_x = 0;
		int translated_y = 0;
		if (!gtk_widget_translate_coordinates(GTK_WIDGET(view), toplevel,
				local.x, local.y, &translated_x, &translated_y))
			return false;
		x = translated_x;
		y = translated_y;
	}
	*rectangle = Keyboard::NavigationRect(x, y, local.width, local.height);
	return true;
}

//-----------------------------------------------------------------------------

static gboolean is_separator(GtkTreeModel* model, GtkTreeIter* iter, gpointer)
{
	gchar* text = nullptr;
	gtk_tree_model_get(model, iter, LauncherView::COLUMN_TEXT, &text, -1);
	gboolean is_empty = xfce_str_is_empty(text);
	g_free(text);

	return is_empty;
}

//-----------------------------------------------------------------------------

LauncherTreeView::LauncherTreeView(Settings* settings) :
	m_settings(settings),
	m_icon_size(0)
{
	// Create the view
	m_view = GTK_TREE_VIEW(gtk_tree_view_new());
	gtk_tree_view_set_activate_on_single_click(m_view, true);
	gtk_tree_view_set_headers_visible(m_view, false);
	gtk_tree_view_set_enable_tree_lines(m_view, false);
	gtk_tree_view_set_enable_search(m_view, false);
	gtk_tree_view_set_fixed_height_mode(m_view, true);
	gtk_tree_view_set_row_separator_func(m_view, &is_separator, nullptr, nullptr);
	create_column();

	// Only select launcher when moving mouse, not when menu is shown
	enable_hover_selection(GTK_WIDGET(m_view));

	// Only allow up to one selected item
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	// Keyboard cursor moves change the selection without passing through the
	// hover chokepoint, so recomposite the whole surface on every selection
	// change while the background is translucent (no-op when opaque). This is the
	// keyboard half of the single-highlight safeguard (the documented behavior).
	connect(selection, "changed",
		[this](GtkTreeSelection*)
		{
			queue_full_redraw_safeguard();
		});

	g_object_ref_sink(m_view);

	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_view)), "launchers");

	// the documented behavior: bring the focused row into view without snap-scrolling on
	// programmatic cursor moves (Tab into Results, arrow exit out of the
	// sidebar). use_align=FALSE keeps the existing scroll position when
	// the cursor row is already visible.
	connect(m_view, "focus-in-event",
		[](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			GtkTreeView* tv = GTK_TREE_VIEW(widget);
			GtkTreePath* path = nullptr;
			gtk_tree_view_get_cursor(tv, &path, nullptr);
			if (!path)
			{
				GtkTreeSelection* sel = gtk_tree_view_get_selection(tv);
				GtkTreeIter iter;
				if (gtk_tree_selection_get_selected(sel, nullptr, &iter))
				{
					path = gtk_tree_model_get_path(
							gtk_tree_view_get_model(tv), &iter);
				}
			}
			if (path)
			{
				gtk_tree_view_scroll_to_cell(tv, path, nullptr, FALSE, 0, 0);
				gtk_tree_path_free(path);
			}
			return GDK_EVENT_PROPAGATE;
		});

	// Expand on click
	connect(m_view, "row-activated",
		[this](GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn*)
		{
			Element* element = nullptr;
			GtkTreeIter iter;
			if (!launcher_model_get_iter(m_model, path, &iter))
			{
				return;
			}
			gtk_tree_model_get(m_model, &iter, COLUMN_LAUNCHER, &element, -1);
			if (element && !dynamic_cast<Category*>(element))
			{
				return;
			}

			if (gtk_tree_view_row_expanded(tree_view, path))
			{
				gtk_tree_view_collapse_row(tree_view, path);
			}
			else
			{
				gtk_tree_view_expand_row(tree_view, path, false);
			}
		});
}

//-----------------------------------------------------------------------------

LauncherTreeView::~LauncherTreeView()
{
	gtk_widget_destroy(GTK_WIDGET(m_view));
	g_object_unref(m_view);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherTreeView::get_cursor() const
{
	GtkTreePath* path = nullptr;
	gtk_tree_view_get_cursor(m_view, &path, nullptr);
	return path;
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherTreeView::get_path_at_pos(int x, int y) const
{
	GtkTreePath* path = nullptr;
	gtk_tree_view_get_path_at_pos(m_view, x, y, &path, nullptr, nullptr, nullptr);
	return path;
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherTreeView::get_selected_path() const
{
	GtkTreePath* path = nullptr;
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected(selection, nullptr, &iter))
	{
		path = gtk_tree_model_get_path(m_model, &iter);
	}
	return path;
}

//-----------------------------------------------------------------------------

bool LauncherTreeView::is_path_selected(GtkTreePath* path) const
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	return gtk_tree_selection_path_is_selected(selection, path);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::activate_path(GtkTreePath* path)
{
	GtkTreeViewColumn* column = gtk_tree_view_get_column(m_view, 0);
	gtk_tree_view_row_activated(m_view, path, column);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::scroll_to_path(GtkTreePath* path)
{
	gtk_tree_view_scroll_to_cell(m_view, path, nullptr, true, 0.5f, 0.5f);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::select_path(GtkTreePath* path)
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_select_path(selection, path);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_cursor(GtkTreePath* path)
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	GtkSelectionMode mode = gtk_tree_selection_get_mode(selection);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);
	gtk_tree_view_set_cursor(m_view, path, nullptr, false);
	gtk_tree_selection_set_mode(selection, mode);
}

//-----------------------------------------------------------------------------

bool LauncherTreeView::is_first_visual_row(GtkTreePath* path) const
{
	return path && gtk_tree_path_get_depth(path) == 1
			&& gtk_tree_path_get_indices(path)[0] == 0;
}

GtkTreePath* LauncherTreeView::get_directional_path(GtkTreePath* origin,
		Keyboard::PhysicalDirection direction) const
{
	return launcher_tree_view_find_directional_path(m_view, origin, direction);
}

bool LauncherTreeView::get_path_rectangle(GtkTreePath* path,
		Keyboard::NavigationRect* rectangle) const
{
	return launcher_tree_view_get_path_rectangle(m_view, path, rectangle);
}

bool LauncherTreeView::apply_keyboard_target(GtkTreePath* path)
{
	if (!path || !m_model)
		return false;
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter(m_model, &iter, path))
		return false;

	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	const GtkSelectionMode mode = gtk_tree_selection_get_mode(selection);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);
	gtk_tree_view_set_cursor(m_view, path, nullptr, false);
	gtk_tree_selection_set_mode(selection, mode);
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_view_scroll_to_cell(m_view, path, nullptr, FALSE, 0, 0);
	gtk_widget_grab_focus(GTK_WIDGET(m_view));
	return gtk_widget_is_focus(GTK_WIDGET(m_view));
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_fixed_height_mode(bool fixed_height)
{
	gtk_tree_view_set_fixed_height_mode(m_view, fixed_height);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_selection_mode(GtkSelectionMode mode)
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_set_mode(selection, mode);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::hide_tooltips()
{
	gtk_tree_view_set_tooltip_column(m_view, -1);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::show_tooltips()
{
	gtk_tree_view_set_tooltip_column(m_view, COLUMN_TOOLTIP);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::clear_selection()
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_unselect_all(selection);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::collapse_all()
{
	gtk_tree_view_collapse_all(m_view);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_model(GtkTreeModel* model)
{
	m_model = model;
	gtk_tree_view_set_model(m_view, model);
	gtk_tree_view_set_search_column(m_view, -1);
	request_content_redraw();
}

//-----------------------------------------------------------------------------

void LauncherTreeView::unset_model()
{
	m_model = nullptr;
	gtk_tree_view_set_model(m_view, nullptr);
	request_content_redraw();
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_tree_view_enable_model_drag_source(m_view, start_button_mask, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_tree_view_enable_model_drag_dest(m_view, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::unset_drag_source()
{
	gtk_tree_view_unset_rows_drag_source(m_view);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::unset_drag_dest()
{
	gtk_tree_view_unset_rows_drag_dest(m_view);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::reload_icon_size()
{
	// Force libxfce4ui to reload SVG icons
	if (m_icon_size != m_settings->launcher_icon_size.get_size())
	{
		gtk_tree_view_remove_column(m_view, m_column);
		create_column();
	}
}

//-----------------------------------------------------------------------------

/* get_item_height:
 *
 * Returns the current ordinary row height. A configured fallback is used while
 * the view has no model row, which keeps the external Calculator banner stable
 * even for a query with no application matches.
 */
int LauncherTreeView::get_item_height() const
{
	if (m_model)
	{
		GtkTreePath* path = gtk_tree_path_new_first();
		GdkRectangle rect;
		gtk_tree_view_get_background_area(m_view, path, m_column, &rect);
		gtk_tree_path_free(path);
		if (rect.height > 0)
			return rect.height;
	}
	return std::max(24, std::max(0, m_icon_size) + 6);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::create_column()
{
	m_icon_size = m_settings->launcher_icon_size.get_size();

	m_column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_expand(m_column, true);
	gtk_tree_view_column_set_visible(m_column, true);

	if (m_icon_size > 1)
	{
		GtkCellRenderer* icon_renderer = whiskermenu_icon_renderer_new();
		g_object_set(icon_renderer, "size", m_icon_size, nullptr);
		gtk_tree_view_column_pack_start(m_column, icon_renderer, false);
		gtk_tree_view_column_set_attributes(m_column, icon_renderer, "gicon", COLUMN_ICON, "launcher", COLUMN_LAUNCHER, nullptr);
	}

	GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
	gtk_tree_view_column_pack_start(m_column, text_renderer, true);
	gtk_tree_view_column_add_attribute(m_column, text_renderer, "markup", COLUMN_TEXT);

	gtk_tree_view_column_set_sizing(m_column, GTK_TREE_VIEW_COLUMN_FIXED);

	gtk_tree_view_append_column(m_view, m_column);
}

//-----------------------------------------------------------------------------
