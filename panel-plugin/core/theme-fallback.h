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

namespace WhiskerMenu
{

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

} // namespace WhiskerMenu

#endif // MEOWMENU_CORE_THEME_FALLBACK_H
