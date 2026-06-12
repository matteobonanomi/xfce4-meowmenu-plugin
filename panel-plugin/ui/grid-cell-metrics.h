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

/* GridCellWidth:
 *
 * The minimum and natural width an icon-grid cell reports from the renderer,
 * in device-independent pixels. Both depend only on the cell padding, the
 * requested icon pixel size, and whether the cell stretches — never on the
 * label text — so the value is identical for an Applications item and a Places
 * item at the same icon size and density (INV-6).
 */
struct GridCellWidth
{
	int minimum;
	int natural;
};

/* meow_grid_cell_width:
 * @padding: the per-cell padding in px (one side; the cell adds it twice).
 * @icon_size: the requested icon pixel size in px.
 * @stretch: whether the cell stretches to fill extra width (justified grid).
 *
 * Pure reproduction of the icon renderer's preferred-width arithmetic. For a
 * non-stretch cell the minimum and natural width are both (padding*2 +
 * icon_size). For a stretch cell the base width gains (76 - icon_size/4), that
 * sum becomes the minimum, and the natural width is (sum*2 - 1) so the grid can
 * distribute slack. The result is label-independent by construction.
 *
 * Returns: the cell's minimum and natural width in px.
 */
GridCellWidth meow_grid_cell_width(int padding, int icon_size, bool stretch);

} // namespace WhiskerMenu

#endif // MEOWMENU_UI_GRID_CELL_METRICS_H
