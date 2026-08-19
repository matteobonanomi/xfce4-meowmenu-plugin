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

#include "grid-cell-metrics.h"

#include <climits>

namespace WhiskerMenu
{

namespace
{

const int k_grid_label_line_height = 14;

}

/* meow_grid_cell_metrics:
 * @padding: per-cell renderer padding in px.
 * @icon_size: requested icon size in px.
 * @spacing: reserved icon-to-label gap in px.
 * @stretch: true when the grid stretches cells across available width.
 * @label_lines: stable label-line count to reserve for every source.
 *
 * Calculates the shared icon-grid cell geometry without consulting GTK state or
 * item text. The width formula is the historical renderer contract; the height
 * formula adds a compact deterministic label allowance so one-line Places
 * labels still occupy the same application-style tile height as application
 * labels without over-reserving vertical space.
 *
 * Returns: the renderer's minimum and natural grid-cell metrics.
 */
GridCellMetrics meow_grid_cell_metrics(int padding, int icon_size, int spacing,
		bool stretch, int label_lines)
{
	int width = (padding * 2) + icon_size;
	if (stretch)
	{
		// Stretch cells widen the base by a size-tapered slack so a justified
		// grid has room to distribute; the minimum is the widened base and the
		// natural is twice that minus one.
		width += 76 - (icon_size / 4);
	}

	if (label_lines < 1)
	{
		label_lines = 1;
	}
	if (spacing < 0)
	{
		spacing = 0;
	}

	const int label_allowance = k_grid_label_line_height * label_lines;
	const int height = (padding * 2) + icon_size + spacing + label_allowance;
	return GridCellMetrics{
		width,
		stretch ? ((width * 2) - 1) : width,
		height,
		height,
		label_allowance
	};
}

GridCellWidth meow_grid_cell_width(int padding, int icon_size, bool stretch)
{
	const GridCellMetrics cell = meow_grid_cell_metrics(padding, icon_size, 0, stretch, 1);
	return GridCellWidth{ cell.minimum_width, cell.natural_width };
}

/* meow_grid_column_layout:
 * @viewport_width: current visible grid width in logical pixels.
 * @margin: grid margin on one side in logical pixels.
 * @spacing: gap between adjacent columns in logical pixels.
 * @item_padding: padding GTK adds on each side of item-width.
 * @minimum_item_width: smallest complete cell width in logical pixels.
 *
 * Uses the minimum complete cell width to select the maximum whole-column
 * count, then removes GTK's external item padding from the item-width property.
 * Invalid theme geometry is clamped so live resize always has a usable result.
 *
 * Returns: at least one column and a positive item width.
 */
GridColumnLayout meow_grid_column_layout(int viewport_width, int margin,
		int spacing, int item_padding, int minimum_item_width)
{
	if (viewport_width < 1)
		viewport_width = 1;
	if (margin < 0)
		margin = 0;
	if (spacing < 0)
		spacing = 0;
	if (item_padding < 0)
		item_padding = 0;
	if (minimum_item_width < 1)
		minimum_item_width = 1;
	const int usable_width = viewport_width > (margin * 2)
			? viewport_width - (margin * 2) : 1;
	int columns = (usable_width + spacing)
			/ (minimum_item_width + spacing);
	if (columns < 1)
		columns = 1;
	const int gaps = (columns - 1) * spacing;
	const int complete_item_width = (usable_width - gaps) / columns;
	int item_width = complete_item_width - (item_padding * 2);
	if (item_width < 1)
		item_width = 1;
	return GridColumnLayout{ columns, item_width };
}

/* meow_grid_queue_frame_width:
 * @viewport_width: latest positive viewport width from resize motion.
 * @pending_width: retained latest width; zero means no frame is queued.
 *
 * Keeps raw input delivery cheap while preserving the newest geometry. The
 * caller schedules at most one GTK frame and consumes the value there.
 *
 * Returns: true only when a new frame callback is required.
 */
bool meow_grid_queue_frame_width(int viewport_width, int* pending_width)
{
	if (!pending_width || viewport_width < 1)
		return false;
	const bool schedule = *pending_width < 1;
	*pending_width = viewport_width;
	return schedule;
}

/* meow_grid_take_frame_width:
 * @pending_width: retained latest width, reset to zero on return.
 *
 * Returns: the latest queued positive width, or zero when none is pending.
 */
int meow_grid_take_frame_width(int* pending_width)
{
	if (!pending_width)
		return 0;
	const int width = *pending_width;
	*pending_width = 0;
	return width;
}

/* meow_grid_effective_viewport_width:
 * @scroller_width: current allocated width of the Results scroller.
 * @toplevel_width: current allocated launcher toplevel width.
 * @requested_toplevel_width: current explicit launcher width, or a non-positive
 * value when no windowed width cap applies.
 *
 * Removes GTK natural-size overshoot already added to the toplevel from the
 * apparent Results allocation. This keeps a child requisition from becoming a
 * larger viewport input on the next category-switch allocation.
 *
 * Returns: a positive effective Results viewport width.
 */
int meow_grid_effective_viewport_width(int scroller_width,
		int toplevel_width, int requested_toplevel_width)
{
	if (scroller_width < 1)
		scroller_width = 1;
	if (requested_toplevel_width <= 0
			|| toplevel_width <= requested_toplevel_width)
	{
		return scroller_width;
	}
	const int overshoot = toplevel_width - requested_toplevel_width;
	return scroller_width > overshoot ? scroller_width - overshoot : 1;
}

/* meow_grid_resized_viewport_width:
 * @current_viewport_width: last effective Results width.
 * @current_toplevel_width: launcher width before the resize step.
 * @requested_toplevel_width: launcher width requested by the resize step.
 *
 * Predicts the Results allocation by applying the current toplevel delta. The
 * value is deliberately independent of child requisitions, which are updated
 * from this result before GTK performs the matching allocation.
 *
 * Returns: the positive predicted Results viewport width.
 */
int meow_grid_resized_viewport_width(int current_viewport_width,
		int current_toplevel_width, int requested_toplevel_width)
{
	if (current_viewport_width < 1
			|| current_toplevel_width < 1
			|| requested_toplevel_width < 1)
	{
		return current_viewport_width;
	}

	const long long resized = static_cast<long long>(current_viewport_width)
			+ requested_toplevel_width - current_toplevel_width;
	if (resized < 1)
		return 1;
	if (resized > INT_MAX)
		return INT_MAX;
	return static_cast<int>(resized);
}

/* meow_grid_release_resize_minimum:
 * @frame_minimum_width: GTK minimum width of the complete launcher frame.
 * @current_viewport_width: last effective Results width.
 * @minimum_viewport_width: smallest complete Results grid width.
 *
 * Releases the current grid's expandable portion from GTK's frame minimum.
 * The remaining requisition still contains the sidebar, search, margins, and
 * the grid's smallest complete column.
 *
 * Returns: a positive launcher minimum width.
 */
int meow_grid_release_resize_minimum(int frame_minimum_width,
		int current_viewport_width, int minimum_viewport_width)
{
	if (frame_minimum_width < 1)
		frame_minimum_width = 1;
	if (current_viewport_width <= minimum_viewport_width
			|| minimum_viewport_width < 1)
	{
		return frame_minimum_width;
	}

	const long long released = static_cast<long long>(frame_minimum_width)
			- (current_viewport_width - minimum_viewport_width);
	return released > 1 ? static_cast<int>(released) : 1;
}

} // namespace WhiskerMenu
