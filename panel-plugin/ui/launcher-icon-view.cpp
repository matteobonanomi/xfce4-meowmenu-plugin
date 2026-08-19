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
#include <vector>

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

/* launcher_icon_view_apply_grid_width:
 * @view: icon grid receiving a complete whole-column layout.
 * @icon_size: current launcher icon size in logical pixels.
 * @viewport_width: current visible Results width in logical pixels.
 *
 * Derives the minimum complete cell from the live density properties, fits the
 * maximum whole-column count, and distributes remaining width evenly. Explicit
 * columns prevent GtkIconView's natural-width preference from leaving a large
 * trailing void at intermediate menu widths.
 */
void WhiskerMenu::launcher_icon_view_apply_grid_width(GtkIconView* view,
		int icon_size, int viewport_width)
{
	if (!GTK_IS_ICON_VIEW(view) || viewport_width <= 0)
		return;
	const GridCellMetrics cell = meow_grid_cell_metrics(
			gtk_icon_view_get_item_padding(view), std::max(0, icon_size),
			gtk_icon_view_get_row_spacing(view), true, 2);
	const GridColumnLayout layout = meow_grid_column_layout(viewport_width,
			gtk_icon_view_get_margin(view),
			gtk_icon_view_get_column_spacing(view),
			gtk_icon_view_get_item_padding(view), cell.minimum_width);
	gtk_widget_set_hexpand(GTK_WIDGET(view), TRUE);
	if (gtk_icon_view_get_columns(view) != layout.columns)
		gtk_icon_view_set_columns(view, layout.columns);
	if (gtk_icon_view_get_item_width(view) != layout.item_width)
		gtk_icon_view_set_item_width(view, layout.item_width);
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
	m_viewport_width(0),
	m_pending_viewport_width(0),
	m_resize_tick_id(0),
	m_interactive_resize(false),
	m_grid_density(),
	m_layout_mode(),
	m_transparent_grid(false)
{
	// Create the view
	m_view = GTK_ICON_VIEW(gtk_icon_view_new());
	gtk_widget_set_hexpand(GTK_WIDGET(m_view), TRUE);

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
	if (m_resize_tick_id != 0)
	{
		gtk_widget_remove_tick_callback(GTK_WIDGET(m_view), m_resize_tick_id);
		m_resize_tick_id = 0;
	}
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

/* launcher_icon_view_find_directional_path:
 * @view: live icon view whose realized cells are inspected.
 * @renderer: renderer identifying the cell geometry to score.
 * @origin: current model path.
 * @direction: physical arrow direction.
 * @rtl: reverses only the stable horizontal tie order.
 *
 * Scores actual visible cells for one internal grid move. Empty final-row
 * positions and model-order wraparound are never synthesized.
 *
 * Returns: a newly allocated current model path, or NULL at an edge.
 */
GtkTreePath* WhiskerMenu::launcher_icon_view_find_directional_path(
		GtkIconView* view, GtkCellRenderer* renderer, GtkTreePath* origin,
		Keyboard::PhysicalDirection direction, bool rtl)
{
	if (!view || !origin)
		return nullptr;
	GdkRectangle origin_rect = {};
	if (!gtk_icon_view_get_cell_rect(view, origin, renderer,
			&origin_rect))
		return nullptr;
	Keyboard::NavigationRect source(origin_rect.x, origin_rect.y,
			origin_rect.width, origin_rect.height);
	if (!source.is_valid())
		return nullptr;

	std::vector<Keyboard::FocusTarget> candidates;
	std::vector<GtkTreePath*> paths;
	GtkTreeModel* model = gtk_icon_view_get_model(view);
	if (!model)
		return nullptr;
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_first(model, &iter))
	{
		do
		{
			GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
			GdkRectangle rectangle = {};
			if (gtk_icon_view_get_cell_rect(view, path, renderer,
					&rectangle) && rectangle.width > 0 && rectangle.height > 0)
			{
				const std::size_t id = paths.size();
				paths.push_back(path);
				Keyboard::FocusTarget candidate;
				candidate.target_id = id;
				candidate.region = Keyboard::NavigationRegion::Results;
				candidate.kind = Keyboard::FocusTargetKind::ResultItem;
				candidate.rectangle = Keyboard::NavigationRect(rectangle.x,
						rectangle.y, rectangle.width, rectangle.height);
				candidate.visual_ordinal = static_cast<unsigned>(id);
				candidate.usable = true;
				candidates.push_back(candidate);
			}
			else
			{
				gtk_tree_path_free(path);
			}
		} while (gtk_tree_model_iter_next(model, &iter));
	}

	const std::size_t selected = Keyboard::choose_spatial_target(source,
			direction, candidates, rtl);
	GtkTreePath* result = selected == Keyboard::NO_TARGET
			|| selected >= paths.size() ? nullptr
			: gtk_tree_path_copy(paths[selected]);
	for (GtkTreePath* path : paths)
		gtk_tree_path_free(path);
	return result;
}

/* launcher_icon_view_get_path_rectangle:
 * @view: live icon view containing @path.
 * @renderer: renderer whose cell bounds represent the item.
 * @path: current model path.
 * @rectangle: output toplevel-coordinate geometry.
 *
 * Reads and translates one current cell allocation without caching it.
 *
 * Returns: true when the cell is currently visible and translatable.
 */
bool WhiskerMenu::launcher_icon_view_get_path_rectangle(GtkIconView* view,
		GtkCellRenderer* renderer, GtkTreePath* path,
		Keyboard::NavigationRect* rectangle)
{
	if (!view || !path || !rectangle)
		return false;
	GdkRectangle local = {};
	if (!gtk_icon_view_get_cell_rect(view, path, renderer, &local)
			|| local.width <= 0 || local.height <= 0)
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

/* launcher_icon_view_apply_keyboard_target:
 * @view: icon view receiving the keyboard target.
 * @model: current model owned by the view.
 * @path: current selectable path.
 *
 * Applies cursor, single selection, reveal, and focus as one keyboard move.
 *
 * Returns: true when the view received focus.
 */
bool WhiskerMenu::launcher_icon_view_apply_keyboard_target(GtkIconView* view,
		GtkTreeModel* model, GtkTreePath* path)
{
	if (!view || !model || !path)
		return false;
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter(model, &iter, path))
		return false;
	gtk_icon_view_unselect_all(view);
	gtk_icon_view_set_cursor(view, path, nullptr, false);
	gtk_icon_view_select_path(view, path);
	gtk_icon_view_scroll_to_path(view, path, FALSE, 0, 0);
	gtk_widget_grab_focus(GTK_WIDGET(view));
	return gtk_widget_is_focus(GTK_WIDGET(view));
}

GtkTreePath* LauncherIconView::get_directional_path(GtkTreePath* origin,
		Keyboard::PhysicalDirection direction) const
{
	return launcher_icon_view_find_directional_path(m_view, m_icon_renderer,
			origin, direction, gtk_widget_get_direction(GTK_WIDGET(m_view))
				== GTK_TEXT_DIR_RTL);
}

bool LauncherIconView::get_path_rectangle(GtkTreePath* path,
		Keyboard::NavigationRect* rectangle) const
{
	return launcher_icon_view_get_path_rectangle(m_view, m_icon_renderer,
			path, rectangle);
}

bool LauncherIconView::apply_keyboard_target(GtkTreePath* path)
{
	return launcher_icon_view_apply_keyboard_target(m_view, m_model, path);
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
	request_content_redraw();
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_model()
{
	m_model = nullptr;
	gtk_icon_view_set_model(m_view, nullptr);
	request_content_redraw();
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

	// Clear stale density geometry before applying the externally owned Results
	// viewport. The icon view's own allocation is deliberately never used here:
	// its requisition includes these columns and would create positive feedback.
	gtk_icon_view_set_columns(m_view, -1);
	gtk_icon_view_set_item_width(m_view, -1);
	if (m_viewport_width > 0)
		launcher_icon_view_apply_grid_width(m_view, m_icon_size,
				m_viewport_width);
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

/* set_viewport_width:
 * @viewport_width: current visible Results width in logical pixels.
 *
 * Caches and reapplies the complete grid-width decision supplied by the owning
 * scroller. Keeping this authority outside GtkIconView prevents its requested
 * column width from becoming the input to the next allocation.
 */
void LauncherIconView::set_viewport_width(int viewport_width)
{
	if (viewport_width <= 0)
		return;
	m_viewport_width = viewport_width;
	if (m_interactive_resize)
	{
		meow_grid_queue_frame_width(m_viewport_width,
				&m_pending_viewport_width);
		schedule_grid_width_frame();
	}
	else
		launcher_icon_view_apply_grid_width(m_view, m_icon_size,
				m_viewport_width);
}

//-----------------------------------------------------------------------------

/* schedule_grid_width_frame:
 *
 * Registers one one-shot update on the icon view's frame clock. More pointer
 * samples only replace m_pending_viewport_width, so a populated model performs
 * at most one exact GtkIconView relayout per display frame.
 */
void LauncherIconView::schedule_grid_width_frame()
{
	if (m_resize_tick_id != 0)
		return;
	m_resize_tick_id = gtk_widget_add_tick_callback(GTK_WIDGET(m_view),
			+[](GtkWidget*, GdkFrameClock*, gpointer data) -> gboolean
			{
				auto* self = static_cast<LauncherIconView*>(data);
				self->m_resize_tick_id = 0;
				const int width = meow_grid_take_frame_width(
						&self->m_pending_viewport_width);
				if (width > 0)
				{
					launcher_icon_view_apply_grid_width(self->m_view,
							self->m_icon_size, width);
				}
				return G_SOURCE_REMOVE;
			}, this, nullptr);
}

//-----------------------------------------------------------------------------

/* flush_grid_width_frame:
 *
 * Cancels a pending frame callback and synchronously applies its latest width.
 * Resize completion and cancellation therefore expose exact terminal spacing
 * before persistence or restored geometry can become visible.
 */
void LauncherIconView::flush_grid_width_frame()
{
	if (m_resize_tick_id != 0)
	{
		gtk_widget_remove_tick_callback(GTK_WIDGET(m_view), m_resize_tick_id);
		m_resize_tick_id = 0;
	}
	const int width = meow_grid_take_frame_width(&m_pending_viewport_width);
	launcher_icon_view_apply_grid_width(m_view, m_icon_size,
			width > 0 ? width : m_viewport_width);
}

//-----------------------------------------------------------------------------

/* set_interactive_resize:
 * @active: true during an X11 live-resize gesture.
 *
 * Bounds exact item-width changes to one latest update per display frame while
 * the toplevel continues to follow every accepted X11 pointer sample.
 */
void LauncherIconView::set_interactive_resize(bool active)
{
	if (m_interactive_resize == active)
		return;
	m_interactive_resize = active;
	if (m_viewport_width < 1)
		return;
	if (!m_interactive_resize)
		flush_grid_width_frame();
}

//-----------------------------------------------------------------------------

/* get_minimum_viewport_width:
 *
 * Computes the narrowest viewport that contains one complete grid cell. GTK's
 * current multi-column requisition must not become the interactive resize
 * floor after the user has enlarged the launcher.
 *
 * Returns: the one-column Results viewport width in logical pixels.
 */
int LauncherIconView::get_minimum_viewport_width() const
{
	const GridCellMetrics cell = meow_grid_cell_metrics(
			gtk_icon_view_get_item_padding(m_view),
			std::max(0, m_icon_size),
			gtk_icon_view_get_row_spacing(m_view), true, 2);
	return (gtk_icon_view_get_margin(m_view) * 2) + cell.minimum_width;
}

//-----------------------------------------------------------------------------
