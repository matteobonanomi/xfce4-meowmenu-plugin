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

#include "window-frame.h"

namespace meow
{

// Supported corner-radius range. Mirrors the /corner-radius control range so
// the visible rounding always tracks what the GUI can request.
namespace
{
const int kMinCornerRadius = 0;
const int kMaxCornerRadius = 24;
} // namespace

int meowmenu_clamp_corner_radius(int radius)
{
	if (radius < kMinCornerRadius)
		return kMinCornerRadius;
	if (radius > kMaxCornerRadius)
		return kMaxCornerRadius;
	return radius;
}

bool meowmenu_frame_draws_border(bool is_fullscreen, bool supports_alpha)
{
	// Full-screen reads as one seamless surface (no outline); the composited
	// rounded stroke also needs an RGBA visual to be drawn at all.
	return !is_fullscreen && supports_alpha;
}

} // namespace meow
