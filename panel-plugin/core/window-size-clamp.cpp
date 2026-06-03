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

#include "window-size-clamp.h"

namespace meow
{

namespace
{

/* clamp_axis:
 * @desired: intended logical size on this axis.
 * @workarea: the monitor's usable size on this axis; <= 0 means "no
 *            constraint" (the monitor could not be resolved).
 *
 * Returns the largest size <= @desired that still leaves a small edge margin
 * inside @workarea. Shrink-only: when @desired already fits, it is returned
 * unchanged, so the standard-DPI layout never moves. The result is floored at
 * 1 so a tiny or garbage work area can never collapse the window to zero.
 */
int clamp_axis(int desired, int workarea)
{
	// No usable constraint on this axis: keep the caller's intent verbatim.
	if (workarea <= 0)
		return desired;

	// Reserve ~4% of the work area as an edge margin so the window is not
	// flush to the screen edge. The exact fraction is not contractual; the
	// guarantees the test pins are shrink-only, on-screen, and never-zero.
	const int margin = workarea / 25;
	const int available = workarea - margin;

	int out = desired < available ? desired : available;
	if (out < 1)
		out = 1;
	return out;
}

} // namespace

void clamp_default_size(int desired_w, int desired_h,
                        int workarea_w, int workarea_h,
                        int* out_w, int* out_h)
{
	*out_w = clamp_axis(desired_w, workarea_w);
	*out_h = clamp_axis(desired_h, workarea_h);
}

} // namespace meow
