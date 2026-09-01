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

#ifndef MEOWMENU_CORE_THEME_FALLBACK_H
#define MEOWMENU_CORE_THEME_FALLBACK_H

#include <gdk/gdk.h>

#include <cstddef>

namespace WhiskerMenu
{

enum class ThemePaletteSource
{
	ThemePair,
	DerivedFromChrome,
	DerivedFromContent,
	DarkFallback
};

struct ThemeSurfacePalette
{
	GdkRGBA fullscreen;
	GdkRGBA chrome;
	GdkRGBA content;
	ThemePaletteSource source;
	bool distinguishable;
};

enum class ThemeMetricsSource
{
	ThemeMedian,
	SafeFallback
};

struct ThemeLayoutMetrics
{
	int region_gap_px;
	ThemeMetricsSource source;
};

/* meow_relative_luminance:
 * @colour: sRGB components in [0,1]; alpha ignored. Must not be NULL.
 *
 * Computes the perceived lightness of a colour so a theme can be classified
 * as light or dark from its text colour.
 *
 * Returns: Rec. 709 relative luminance Y = 0.2126R + 0.7152G + 0.0722B over
 * the [0,1] components (no re-clamp).
 */
double meow_relative_luminance(const GdkRGBA* colour);

/* meow_choose_background_fallback:
 * @have_fg: whether a foreground/text colour was resolved from the theme.
 * @fg: the foreground colour; read only when @have_fg is TRUE (may be NULL
 *      otherwise).
 * @prefer_dark: value of the gtk-application-prefer-dark-theme setting; used
 *               only as a last-but-one signal when @have_fg is FALSE.
 *
 * Picks the base background colour to use when the theme's own background
 * colour lookup failed. The result is one of exactly two fixed neutral greys
 * and is always opaque, so the menu is never left unpainted.
 *
 * Returns: GdkRGBA dark rgb(31,31,31) or light rgb(245,245,245), alpha 1.0.
 */
GdkRGBA meow_choose_background_fallback(gboolean have_fg, const GdkRGBA* fg,
                                        gboolean prefer_dark);

/* meow_resolve_surface_palette:
 * @have_background: whether @background contains a usable GTK background role.
 * @background: theme chrome candidate; may be NULL when unavailable.
 * @have_base: whether @base contains a usable GTK content/base role.
 * @base: theme content candidate; may be NULL when unavailable.
 * @fallback: established single-surface fallback for the active theme.
 *
 * Keeps a distinguishable GTK pair when possible and derives a bounded
 * counterpart when the theme exposes only one useful role. Complete lookup
 * failure returns two stable dark neutrals while Full Screen keeps @fallback.
 *
 * Returns: an opaque, distinguishable palette by value.
 */
ThemeSurfacePalette meow_resolve_surface_palette(bool have_background,
		const GdkRGBA* background, bool have_base, const GdkRGBA* base,
		const GdkRGBA& fallback);

/* meow_resolve_layout_metrics:
 * @padding_values: normal-state GTK padding sides in logical pixels.
 * @count: number of values; negative entries are ignored.
 *
 * Reduces usable Search and Session padding to one bounded median rhythm.
 * An empty usable sample receives the established six-pixel safe fallback.
 *
 * Returns: the resolved region gap and its source by value.
 */
ThemeLayoutMetrics meow_resolve_layout_metrics(const int* padding_values,
		std::size_t count);

/* meow_resolve_boundary_gap:
 * @first_visible: whether the first neighboring region is allocated.
 * @second_visible: whether the second neighboring region is allocated.
 * @region_gap_px: resolved theme rhythm.
 *
 * Gives one owner the complete boundary gap and removes it when either region
 * is hidden, preventing doubled and orphaned spacing.
 *
 * Returns: @region_gap_px when both regions are visible, otherwise zero.
 */
int meow_resolve_boundary_gap(bool first_visible, bool second_visible,
		int region_gap_px);

/* meow_style_refresh_should_schedule:
 * @source_id: current queued idle source, or zero when none is queued.
 * @refresh_running: true while the refresh transaction is applying CSS/layout.
 *
 * Coalesces style callbacks and blocks reentrant callbacks caused by loading
 * the application CSS provider.
 *
 * Returns: true only when one new idle refresh may be queued.
 */
bool meow_style_refresh_should_schedule(unsigned int source_id,
		bool refresh_running);

} // namespace WhiskerMenu

#endif // MEOWMENU_CORE_THEME_FALLBACK_H
