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

#ifndef MEOWMENU_UI_SWITCH_ICONS_H
#define MEOWMENU_UI_SWITCH_ICONS_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

// NULL-terminated icon-name fallback chains for the Apps/Places switch, most
// preferred first. Symbolic names degrade to a non-symbolic stock icon so a
// usable image always renders even on themes without symbolic variants.
extern const char* const MEOW_SWITCH_APPS_ICONS[];
extern const char* const MEOW_SWITCH_PLACES_ICONS[];

/* meow_resolve_icon_name:
 * @theme: icon theme to query; must not be NULL.
 * @chain: NULL-terminated array of candidate icon names, most-preferred first;
 *         must contain at least one entry.
 *
 * Walks @chain and returns the first name present in @theme. When none are
 * present the last entry is returned unconditionally so the caller always has a
 * usable name (GTK then draws its own missing-image fallback).
 *
 * Returns: a borrowed string owned by @chain; never NULL or empty for a
 * well-formed chain. Do not free.
 */
const char* meow_resolve_icon_name(GtkIconTheme* theme, const char* const* chain);

} // namespace WhiskerMenu

#endif // MEOWMENU_UI_SWITCH_ICONS_H
