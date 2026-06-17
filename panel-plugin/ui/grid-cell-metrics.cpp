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

GridCellWidth meow_grid_cell_width(int padding, int icon_size, bool stretch)
{
	int width = (padding * 2) + icon_size;
	if (stretch)
	{
		// Stretch cells widen the base by a size-tapered slack so a justified
		// grid has room to distribute; the minimum is the widened base and the
		// natural is twice that minus one.
		width += 76 - (icon_size / 4);
		return GridCellWidth{ width, (width * 2) - 1 };
	}
	return GridCellWidth{ width, width };
}

} // namespace WhiskerMenu
