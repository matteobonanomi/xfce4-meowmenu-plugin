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

} // namespace WhiskerMenu
