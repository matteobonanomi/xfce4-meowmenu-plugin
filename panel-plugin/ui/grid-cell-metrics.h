/*
 * Copyright (C) 2026 MeowMenu contributors
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

#ifndef MEOWMENU_UI_GRID_CELL_METRICS_H
#define MEOWMENU_UI_GRID_CELL_METRICS_H

namespace WhiskerMenu
{

/* GridCellMetrics:
 *
 * The minimum and natural size an icon-grid cell reports from the renderer,
 * in device-independent pixels. Both width and height depend only on the view
 * geometry inputs — never on the label text — so the value is identical for an
 * Applications item and a Places item at the same icon size and density.
 */
struct GridCellMetrics
{
	int minimum_width;
	int natural_width;
	int minimum_height;
	int natural_height;
	int label_line_allowance;
};

/* GridCellWidth:
 *
 * Compatibility view of the width contract used before height became shared.
 */
struct GridCellWidth
{
	int minimum;
	int natural;
};

/* GridColumnLayout:
 *
 * Whole-column layout for one current icon-grid viewport. @item_width is the
 * GtkIconView item-width property, excluding the separate item padding GTK
 * adds on both sides. Any trailing remainder is smaller than one column.
 */
struct GridColumnLayout
{
	int columns;
	int item_width;
};

/* meow_grid_cell_metrics:
 * @padding: the per-cell padding in px (one side; the cell adds it twice).
 * @icon_size: the requested icon pixel size in px.
 * @spacing: the vertical gap reserved between icon and label in px.
 * @stretch: whether the cell stretches to fill extra width (justified grid).
 * @label_lines: number of label lines the grid reserves for every source.
 *
 * Pure reproduction of the icon renderer's preferred-size arithmetic. The
 * width side preserves the historical width contract. The height side reserves
 * a stable application-style label allowance for every result source so Places
 * and Applications cannot diverge just because their current labels differ.
 *
 * Returns: the cell's minimum and natural size in px.
 */
GridCellMetrics meow_grid_cell_metrics(int padding, int icon_size, int spacing,
		bool stretch, int label_lines);

/* meow_grid_cell_width:
 * @padding: the per-cell padding in px (one side; the cell adds it twice).
 * @icon_size: the requested icon pixel size in px.
 * @stretch: whether the cell stretches to fill extra width (justified grid).
 *
 * Compatibility helper for callers and tests that only need the width side of
 * meow_grid_cell_metrics().
 *
 * Returns: the cell's minimum and natural width in px.
 */
GridCellWidth meow_grid_cell_width(int padding, int icon_size, bool stretch);

/* meow_grid_column_layout:
 * @viewport_width: current visible grid width in logical pixels.
 * @margin: grid margin on one side in logical pixels.
 * @spacing: gap between adjacent columns in logical pixels.
 * @item_padding: padding GTK adds on each side of item-width.
 * @minimum_item_width: smallest complete cell width in logical pixels.
 *
 * Fits the maximum number of complete cells, then distributes the remaining
 * width evenly so no avoidable trailing void or partial column remains.
 *
 * Returns: at least one column and a positive item width.
 */
GridColumnLayout meow_grid_column_layout(int viewport_width, int margin,
		int spacing, int item_padding, int minimum_item_width);

/* meow_grid_queue_frame_width:
 * @viewport_width: latest positive viewport width from resize motion.
 * @pending_width: retained latest width; zero means no frame is queued.
 *
 * Replaces any older pending width. The first queued width tells the caller to
 * register one frame callback; later motion before that frame only updates the
 * retained value.
 *
 * Returns: true only when a new frame callback is required.
 */
bool meow_grid_queue_frame_width(int viewport_width, int* pending_width);

/* meow_grid_take_frame_width:
 * @pending_width: retained latest width, reset to zero on return.
 *
 * Returns: the latest queued positive width, or zero when none is pending.
 */
int meow_grid_take_frame_width(int* pending_width);

/* meow_grid_effective_viewport_width:
 * @scroller_width: allocated Results scroller width.
 * @toplevel_width: allocated launcher toplevel width.
 * @requested_toplevel_width: explicit launcher width, or non-positive when
 * uncapped.
 *
 * Removes toplevel natural-size overshoot from the apparent Results width so
 * child requisitions cannot feed a larger viewport back into themselves.
 *
 * Returns: a positive effective Results viewport width.
 */
int meow_grid_effective_viewport_width(int scroller_width,
		int toplevel_width, int requested_toplevel_width);

/* meow_grid_resized_viewport_width:
 * @current_viewport_width: last effective Results width.
 * @current_toplevel_width: launcher width before the resize step.
 * @requested_toplevel_width: launcher width requested by the resize step.
 *
 * Applies the toplevel width delta to the Results viewport before GTK has
 * allocated the new window. This lets an icon grid change columns during a
 * live drag instead of waiting for the next size allocation.
 *
 * Returns: the positive predicted Results viewport width.
 */
int meow_grid_resized_viewport_width(int current_viewport_width,
		int current_toplevel_width, int requested_toplevel_width);

/* meow_grid_release_resize_minimum:
 * @frame_minimum_width: GTK minimum width of the complete launcher frame.
 * @current_viewport_width: last effective Results width.
 * @minimum_viewport_width: smallest complete Results grid width.
 *
 * Removes only the grid's width above its one-column minimum from the frame
 * requisition. Other controls continue to define the interactive resize floor.
 *
 * Returns: a positive launcher minimum width.
 */
int meow_grid_release_resize_minimum(int frame_minimum_width,
		int current_viewport_width, int minimum_viewport_width);

} // namespace WhiskerMenu

#endif // MEOWMENU_UI_GRID_CELL_METRICS_H
