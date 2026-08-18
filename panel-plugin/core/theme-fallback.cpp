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

#include <algorithm>
#include <cmath>
#include <vector>

namespace WhiskerMenu
{

namespace
{

// The two fixed neutral greys the chooser may return. Dark matches the
// historical hard-coded seed exactly, so the dark-theme and undeterminable
// paths are bit-for-bit unchanged; light is its counterpart for light themes.
const GdkRGBA MEOW_FALLBACK_DARK  = { 0.12,  0.12,  0.12,  1.0 }; // rgb(31,31,31)
const GdkRGBA MEOW_FALLBACK_LIGHT = { 0.961, 0.961, 0.961, 1.0 }; // rgb(245,245,245)
const GdkRGBA MEOW_CONTENT_DARK = { 0.20, 0.20, 0.20, 1.0 };

bool usable_colour(const GdkRGBA* colour)
{
	return colour && std::isfinite(colour->red)
			&& std::isfinite(colour->green)
			&& std::isfinite(colour->blue)
			&& colour->red >= 0.0 && colour->red <= 1.0
			&& colour->green >= 0.0 && colour->green <= 1.0
			&& colour->blue >= 0.0 && colour->blue <= 1.0;
}

GdkRGBA opaque(const GdkRGBA& colour)
{
	GdkRGBA result = colour;
	result.alpha = 1.0;
	return result;
}

bool distinguishable(const GdkRGBA& first, const GdkRGBA& second)
{
	return std::fabs(meow_relative_luminance(&first)
			- meow_relative_luminance(&second)) >= 0.06;
}

GdkRGBA derive_counterpart(const GdkRGBA& source)
{
	const double amount = meow_relative_luminance(&source) < 0.5 ? 0.12 : -0.12;
	GdkRGBA result = {
			std::max(0.0, std::min(1.0, source.red + amount)),
			std::max(0.0, std::min(1.0, source.green + amount)),
			std::max(0.0, std::min(1.0, source.blue + amount)),
			1.0
	};
	return result;
}

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
		// colour keeps the safe dark surface.
		return meow_relative_luminance(fg) >= 0.5 ? MEOW_FALLBACK_DARK
		                                           : MEOW_FALLBACK_LIGHT;
	}

	// NOTE: prefer_dark is a one-way dark confirmation, never a light signal.
	// TRUE selects dark; FALSE/unknown both fall through to the dark default
	// below, because FALSE is the GTK default for most themes regardless of
	// their real lightness and so is not trustworthy (research R4). The light
	// fallback is reachable only via the foreground-luminance branch above.
	if (prefer_dark)
	{
		return MEOW_FALLBACK_DARK;
	}

	// When no signal is usable, keep the established safe dark neutral.
	return MEOW_FALLBACK_DARK;
}

ThemeSurfacePalette meow_resolve_surface_palette(bool have_background,
		const GdkRGBA* background, bool have_base, const GdkRGBA* base,
		const GdkRGBA& fallback)
{
	const bool background_ok = have_background && usable_colour(background);
	const bool base_ok = have_base && usable_colour(base);
	ThemeSurfacePalette result = {};
	result.fullscreen = opaque(fallback);

	if (background_ok && base_ok && distinguishable(*background, *base))
	{
		result.chrome = opaque(*background);
		result.content = opaque(*base);
		result.source = ThemePaletteSource::ThemePair;
	}
	else if (background_ok)
	{
		result.chrome = opaque(*background);
		result.content = derive_counterpart(result.chrome);
		result.source = ThemePaletteSource::DerivedFromChrome;
	}
	else if (base_ok)
	{
		result.content = opaque(*base);
		result.chrome = derive_counterpart(result.content);
		result.source = ThemePaletteSource::DerivedFromContent;
	}
	else
	{
		result.chrome = MEOW_FALLBACK_DARK;
		result.content = MEOW_CONTENT_DARK;
		result.source = ThemePaletteSource::DarkFallback;
	}

	result.distinguishable = distinguishable(result.chrome, result.content);
	return result;
}

ThemeLayoutMetrics meow_resolve_layout_metrics(const int* padding_values,
		std::size_t count)
{
	std::vector<int> usable;
	if (padding_values)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			if (padding_values[i] >= 0)
				usable.push_back(padding_values[i]);
		}
	}
	if (usable.empty())
		return { 6, ThemeMetricsSource::SafeFallback };

	std::sort(usable.begin(), usable.end());
	const std::size_t middle = usable.size() / 2;
	int median = usable[middle];
	if ((usable.size() % 2) == 0)
		median = (usable[middle - 1] + usable[middle] + 1) / 2;
	median = std::max(2, std::min(16, median));
	return { median, ThemeMetricsSource::ThemeMedian };
}

int meow_resolve_boundary_gap(bool first_visible, bool second_visible,
		int region_gap_px)
{
	return first_visible && second_visible ? std::max(0, region_gap_px) : 0;
}

bool meow_style_refresh_should_schedule(unsigned int source_id,
		bool refresh_running)
{
	return source_id == 0 && !refresh_running;
}

} // namespace WhiskerMenu
