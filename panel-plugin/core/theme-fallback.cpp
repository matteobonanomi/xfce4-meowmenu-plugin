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

#include "theme-fallback.h"

namespace WhiskerMenu
{

namespace
{

// The two fixed neutral greys the chooser may return. Dark matches the
// historical hard-coded seed exactly, so the dark-theme and undeterminable
// paths are bit-for-bit unchanged; light is its counterpart for light themes.
const GdkRGBA MEOW_FALLBACK_DARK  = { 0.12,  0.12,  0.12,  1.0 }; // rgb(31,31,31)
const GdkRGBA MEOW_FALLBACK_LIGHT = { 0.961, 0.961, 0.961, 1.0 }; // rgb(245,245,245)

} // namespace

double meow_relative_luminance(const GdkRGBA* colour)
{
	// Rec. 709 luma. Components are GTK-supplied in [0,1]; we deliberately do
	// not re-clamp, matching how the caller already trusts the lookup range.
	return 0.2126 * colour->red + 0.7152 * colour->green + 0.0722 * colour->blue;
}

GdkRGBA meow_choose_background_fallback(gboolean have_fg, const GdkRGBA* fg,
                                        gboolean prefer_dark)
{
	if (have_fg)
	{
		// Light text implies a dark theme and vice versa. The Y >= 0.5 tie
		// resolves to "light text" → dark background, so a mid-grey text
		// colour keeps the safe dark surface (research R3).
		return meow_relative_luminance(fg) >= 0.5 ? MEOW_FALLBACK_DARK
		                                           : MEOW_FALLBACK_LIGHT;
	}

	// NOTE: prefer_dark is a one-way dark confirmation, never a light signal.
	// TRUE selects dark; FALSE/unknown both fall through to the dark default
	// below, because FALSE is the GTK default for most themes regardless of
	// their real lightness and so is not trustworthy (research R4). The light
	// fallback is therefore reachable only via the foreground-luminance branch
	// above — prefer_dark can confirm dark but can never flip to light.
	if (prefer_dark)
	{
		return MEOW_FALLBACK_DARK;
	}

	// FR-004 safe default: when no signal is usable, keep today's dark grey so
	// the menu is always painted with a known-good, opaque colour.
	return MEOW_FALLBACK_DARK;
}

} // namespace WhiskerMenu
