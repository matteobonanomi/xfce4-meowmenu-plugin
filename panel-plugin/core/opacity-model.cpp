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

/* meowmenu_background_translucent:
 * Keyed on the single resolved alpha so the launcher-view safeguard shares one
 * GTK-free definition of "is the background see-through" with the renderer and
 * the unit test. alpha < 1.0 is exactly menu_opacity < 100 after clamping, so a
 * fully-opaque (100) or out-of-range-high value is reported as solid.
 */
bool meowmenu_background_translucent(int menu_opacity)
{
	return meowmenu_opacity_alpha(menu_opacity) < 1.0;
}

double meowmenu_effective_background_alpha(int menu_opacity, bool composited)
{
	return composited ? meowmenu_opacity_alpha(menu_opacity) : 1.0;
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
