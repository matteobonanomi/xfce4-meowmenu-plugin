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

} // namespace WhiskerMenu

#endif // MEOWMENU_UI_GRID_CELL_METRICS_H
