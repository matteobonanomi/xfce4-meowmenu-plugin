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

#include "opacity-model.h"

#include <glib.h>

/* meowmenu_opacity_alpha:
 * Linear, clamped percentage->alpha map. Kept separate so the renderer and the
 * unit test share one definition of the endpoint contract.
 */
double meowmenu_opacity_alpha(int value)
{
	if (value <= 0)
		return 0.0;
	if (value >= 100)
		return 1.0;
	return value / 100.0;
}

/* meowmenu_region_alphas:
 * Implements the two complementary single-alpha models so no region ever
 * carries two backgrounds that compound:
 *   - Full-screen: the window shell owns the one full-screen alpha; the
 *     categories and applications regions paint nothing (transparent), so the
 *     whole surface — results area included — reads at exactly alpha(full).
 *   - Docked: the window shell paints nothing; the categories region and the
 *     applications region each own their absolute alpha over the transparent
 *     window, so apps_opacity can reach a true 0 with no categories floor
 *     showing through underneath.
 */
OpacityRegionAlphas meowmenu_region_alphas(bool fullscreen,
                                           int categories_opacity,
                                           int apps_opacity,
                                           int full_screen_opacity)
{
	OpacityRegionAlphas result;
	if (fullscreen)
	{
		result.window = meowmenu_opacity_alpha(full_screen_opacity);
		result.categories = 0.0;
		result.apps = 0.0;
	}
	else
	{
		result.window = 0.0;
		result.categories = meowmenu_opacity_alpha(categories_opacity);
		result.apps = meowmenu_opacity_alpha(apps_opacity);
	}
	return result;
}

/* meowmenu_format_css_alpha:
 * g_ascii_formatd formats the double exactly as printf would under the "C"
 * locale, so the separator is always '.' regardless of the active LC_NUMERIC,
 * and it touches no global locale state — both required because this CSS feeds
 * GTK's parser (a comma would make rgba(...) invalid) and the panel process is
 * shared with other plugins. Kept GTK-free so it stays headlessly testable.
 */
const char* meowmenu_format_css_alpha(double alpha, char* out)
{
	g_ascii_formatd(out, MEOWMENU_CSS_ALPHA_BUFSZ, "%.3f", alpha);
	return out;
}
