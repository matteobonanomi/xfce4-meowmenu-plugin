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

#include "switch-icons.h"

namespace WhiskerMenu
{

const char* const MEOW_SWITCH_APPS_ICONS[] = {
	"view-app-grid-symbolic",
	"view-grid-symbolic",
	"applications-other",
	nullptr
};

const char* const MEOW_SWITCH_PLACES_ICONS[] = {
	"folder-symbolic",
	"folder",
	nullptr
};

const char* meow_resolve_icon_name(GtkIconTheme* theme, const char* const* chain)
{
	const char* last = chain[0];
	for (const char* const* p = chain; *p; ++p)
	{
		last = *p;
		if (gtk_icon_theme_has_icon(theme, *p))
			return *p;
	}
	// No candidate is installed: return the last name unconditionally so the
	// caller always builds an image; GTK renders its own missing-icon glyph.
	return last;
}

} // namespace WhiskerMenu
