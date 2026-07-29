/*
 * Copyright (C) 2019 Graeme Gott <graeme@gottcode.org>
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

#include "launcher-icon-view.h"

#include "grid-cell-metrics.h"
#include "icon-renderer.h"
#include "settings.h"
#include "slot.h"

#include <algorithm>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* launcher_icon_view_set_transparent_grid_style:
 * @view: icon-view widget receiving the scoped style class.
 * @enabled: whether transparent resting cells are enabled.
 *
 * Mirrors the preference as a widget-local CSS class and queues a redraw. The
 * helper keeps style scoping directly testable without a Settings instance.
 */
void WhiskerMenu::launcher_icon_view_set_transparent_grid_style(
		GtkWidget* view, bool enabled)
{
	GtkStyleContext* context = gtk_widget_get_style_context(view);
	if (enabled)
	{
		gtk_style_context_add_class(context, "transparent-grid");
	}
	else
	{
		gtk_style_context_remove_class(context, "transparent-grid");
	}
	gtk_widget_queue_draw(view);
}

//-----------------------------------------------------------------------------

/* launcher_icon_view_complete_empty_click:
 * @view: concrete icon view after GTK default button processing.
 * @transparent_grid: whether transparent resting cells are enabled.
 * @event: completed pointer event in widget coordinates.
 *
 * Enforces the empty-primary-click postcondition without consuming the event:
 * no selection remains and the complete grid is queued for redraw. Widget
 * focus and GTK's cursor are intentionally retained for keyboard continuation.
 *
 * Returns: true when an empty primary click was handled.
 */
bool WhiskerMenu::launcher_icon_view_complete_empty_click(GtkIconView* view,
		bool transparent_grid, const GdkEventButton* event)
{
	if (!view || !transparent_grid || !event
			|| event->type != GDK_BUTTON_RELEASE || event->button != 1)
	{
		return false;
	}

	GtkTreePath* path = nullptr;
	if (event->x >= 0 && event->y >= 0)
	{
		path = gtk_icon_view_get_path_at_pos(view,
				static_cast<int>(event->x), static_cast<int>(event->y));
	}
	if (path)
	{
		gtk_tree_path_free(path);
		return false;
	}

	gtk_icon_view_unselect_all(view);
	gtk_widget_queue_draw(GTK_WIDGET(view));
	return true;
}

//-----------------------------------------------------------------------------

LauncherIconView::LauncherIconView(Settings* settings) :
	m_settings(settings),
	m_icon_renderer(nullptr),
	m_icon_size(-1),
	m_grid_density(),
	m_layout_mode(),
	m_transparent_grid(false)
{
	// Create the view
	m_view = GTK_ICON_VIEW(gtk_icon_view_new());

	m_icon_renderer = whiskermenu_icon_renderer_new();
	g_object_set(m_icon_renderer,
			"stretch", true,
			"xalign", 0.5,
			"yalign", 1.0,
			nullptr);
	GtkCellLayout* cell_layout = GTK_CELL_LAYOUT(m_view);
	gtk_cell_layout_pack_start(cell_layout, m_icon_renderer, false);
	gtk_cell_layout_set_attributes(cell_layout, m_icon_renderer, "gicon", COLUMN_ICON, "launcher", COLUMN_LAUNCHER, nullptr);

	gtk_icon_view_set_markup_column(m_view, COLUMN_TEXT);

	reload_icon_size();

	// Use single clicks to activate items
	gtk_icon_view_set_activate_on_single_click(m_view, true);

	// Only allow up to one selected item
	gtk_icon_view_set_selection_mode(m_view, GTK_SELECTION_SINGLE);

	// Keyboard cursor moves change the selection without passing through the
	// hover chokepoint, so recomposite the whole surface on every selection
	// change while the background is translucent (no-op when opaque). This is the
	// keyboard half of the single-highlight safeguard (the documented behavior).
	connect(m_view, "selection-changed",
		[this](GtkIconView*)
		{
			queue_full_redraw_safeguard();
		});

	// Observe the completed event so GTK has already applied its own focus and
	// selection behavior. The callback does not consume activation or drag
	// events and acts only on primary clicks with no model path.
	connect(m_view, "button-release-event",
		[this](GtkWidget*, GdkEventButton* event) -> gboolean
		{
			launcher_icon_view_complete_empty_click(m_view,
					m_transparent_grid, event);
			return GDK_EVENT_PROPAGATE;
		},
		Connect::After);

	g_object_ref_sink(m_view);

	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_view)), "launchers");
	sync_transparent_grid_style();

	// Handle hover selection
	enable_hover_selection(GTK_WIDGET(m_view));

	// the documented behavior: keep the focused item visible on programmatic cursor moves
	// (Tab into Results, sidebar arrow exit). use_align=FALSE so an
	// already-visible item does not jump.
	connect(m_view, "focus-in-event",
		[](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			GtkIconView* iv = GTK_ICON_VIEW(widget);
			GtkTreePath* path = nullptr;
			gtk_icon_view_get_cursor(iv, &path, nullptr);
			if (!path)
			{
				GList* sel = gtk_icon_view_get_selected_items(iv);
				if (sel)
				{
					path = gtk_tree_path_copy(
							static_cast<GtkTreePath*>(sel->data));
				}
				g_list_free_full(sel,
						reinterpret_cast<GDestroyNotify>(&gtk_tree_path_free));
			}
			if (path)
			{
				gtk_icon_view_scroll_to_path(iv, path, FALSE, 0, 0);
				gtk_tree_path_free(path);
			}
			return GDK_EVENT_PROPAGATE;
		});
}

//-----------------------------------------------------------------------------

LauncherIconView::~LauncherIconView()
{
	gtk_widget_destroy(GTK_WIDGET(m_view));
	g_object_unref(m_view);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_cursor() const
{
	GtkTreePath* path = nullptr;
	gtk_icon_view_get_cursor(m_view, &path, nullptr);
	return path;
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_path_at_pos(int x, int y) const
{
	return gtk_icon_view_get_path_at_pos(m_view, x, y);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_selected_path() const
{
	GtkTreePath* path = nullptr;
	GList* selection = gtk_icon_view_get_selected_items(m_view);
	if (selection)
	{
		path = gtk_tree_path_copy(static_cast<GtkTreePath*>(selection->data));
	}
	g_list_free_full(selection, reinterpret_cast<GDestroyNotify>(&gtk_tree_path_free));
	return path;
}

//-----------------------------------------------------------------------------

bool LauncherIconView::is_path_selected(GtkTreePath* path) const
{
	return gtk_icon_view_path_is_selected(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::activate_path(GtkTreePath* path)
{
	gtk_icon_view_item_activated(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::scroll_to_path(GtkTreePath* path)
{
	gtk_icon_view_scroll_to_path(m_view, path, true, 0.5f, 0.5f);
}

//-----------------------------------------------------------------------------

void LauncherIconView::select_path(GtkTreePath* path)
{
	gtk_icon_view_select_path(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_cursor(GtkTreePath* path)
{
	gtk_icon_view_set_cursor(m_view,path, nullptr, false);
}

//-----------------------------------------------------------------------------

bool LauncherIconView::is_first_visual_row(GtkTreePath* path) const
{
	return path && gtk_icon_view_get_item_row(m_view, path) == 0;
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_fixed_height_mode(bool)
{
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_selection_mode(GtkSelectionMode mode)
{
	gtk_icon_view_set_selection_mode(m_view, mode);
}

//-----------------------------------------------------------------------------

void LauncherIconView::hide_tooltips()
{
	gtk_icon_view_set_tooltip_column(m_view, -1);
}

//-----------------------------------------------------------------------------

void LauncherIconView::show_tooltips()
{
	gtk_icon_view_set_tooltip_column(m_view, COLUMN_TOOLTIP);
}

//-----------------------------------------------------------------------------

void LauncherIconView::clear_selection()
{
	gtk_icon_view_unselect_all(m_view);
}

//-----------------------------------------------------------------------------

void LauncherIconView::collapse_all()
{
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_model(GtkTreeModel* model)
{
	m_model = model;
	gtk_icon_view_set_model(m_view, model);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_model()
{
	m_model = nullptr;
	gtk_icon_view_set_model(m_view, nullptr);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_icon_view_enable_model_drag_source(m_view, start_button_mask, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_icon_view_enable_model_drag_dest(m_view, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_drag_source()
{
	gtk_icon_view_unset_model_drag_source(m_view);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_drag_dest()
{
	gtk_icon_view_unset_model_drag_dest(m_view);
}

//-----------------------------------------------------------------------------

void LauncherIconView::reload_icon_size()
{
	sync_transparent_grid_style();

	// Fetch icon size
	const int icon_size = m_settings->launcher_icon_size.get_size();
	const char* density = m_settings->grid_density;
	const std::string density_value = density ? density : "";
	const char* layout = m_settings->layout_mode;
	const std::string layout_value = layout ? layout : "";
	if ((m_icon_size == icon_size)
			&& (m_grid_density == density_value)
			&& (m_layout_mode == layout_value))
	{
		return;
	}
	m_icon_size = icon_size;
	m_grid_density = density_value;
	m_layout_mode = layout_value;

	// Configure icon renderer
	if (m_icon_size > 1)
	{
		g_object_set(m_icon_renderer, "size", m_icon_size, "visible", true, nullptr);
	}
	else
	{
		g_object_set(m_icon_renderer, "visible", false, nullptr);
	}

	// Reset padding to fix icon clipping
	gtk_icon_view_set_item_padding(m_view, 0);

	// Adjust item size based on icon size
	int base_padding = 2;
	switch (m_settings->launcher_icon_size)
	{
	case IconSize::Smallest:
	case IconSize::Smaller:
		base_padding = 2;
		break;

	case IconSize::Small:
	case IconSize::Normal:
	case IconSize::Large:
		base_padding = 4;
		break;

	case IconSize::Larger:
	case IconSize::Largest:
		base_padding = 6;
		break;

	default:
		break;
	}

	// the implementation step: adjust padding/spacing from grid-density (low/medium/high)
	int padding = base_padding;
	if (g_strcmp0(density, "low") == 0)
	{
		padding = base_padding + 4;
		gtk_icon_view_set_column_spacing(m_view, base_padding + 4);
		gtk_icon_view_set_row_spacing(m_view, base_padding + 4);
	}
	else if (g_strcmp0(density, "high") == 0)
	{
		padding = std::max(0, base_padding - 2);
		gtk_icon_view_set_column_spacing(m_view, std::max(0, base_padding - 2));
		gtk_icon_view_set_row_spacing(m_view, std::max(0, base_padding - 2));
	}
	else // medium (default)
	{
		gtk_icon_view_set_column_spacing(m_view, base_padding);
		gtk_icon_view_set_row_spacing(m_view, base_padding);
	}
	gtk_icon_view_set_item_padding(m_view, padding);
	g_object_set(m_icon_renderer,
			"spacing", gtk_icon_view_get_row_spacing(m_view),
			"label-lines", 2,
			nullptr);

	// Let GtkIconView adapt the number of columns to the available width.
	gtk_icon_view_set_columns(m_view, -1);
}

//-----------------------------------------------------------------------------

/* get_item_height:
 *
 * Returns one deterministic grid-row height from the renderer's configured
 * icon, padding, spacing, and label allowance. Live cell rectangles are not
 * used because a transient single-item model can stretch them across the
 * results area, which would incorrectly move external rows such as Calculator.
 */
int LauncherIconView::get_item_height() const
{
	const int padding = gtk_icon_view_get_item_padding(m_view);
	const int spacing = gtk_icon_view_get_row_spacing(m_view);
	const GridCellMetrics cell = meow_grid_cell_metrics(padding,
			std::max(0, m_icon_size), spacing, true, 2);
	return std::max(32, cell.natural_height);
}

//-----------------------------------------------------------------------------

/* sync_transparent_grid_style:
 *
 * Mirrors the live /transparent-grid setting onto the icon view's CSS class.
 * The class is used only by the window-scoped CSS provider, keeping list/tree
 * views and non-resting interaction states outside this visual preference.
 */
void LauncherIconView::sync_transparent_grid_style()
{
	const bool transparent_grid = m_settings->transparent_grid;
	if (m_transparent_grid == transparent_grid)
	{
		return;
	}
	m_transparent_grid = transparent_grid;

	launcher_icon_view_set_transparent_grid_style(GTK_WIDGET(m_view),
			transparent_grid);
}

//-----------------------------------------------------------------------------
