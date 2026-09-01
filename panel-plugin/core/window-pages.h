/*
 * Copyright (C) 2026 Matteo Bonanomi
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
 */

#ifndef WHISKERMENU_WINDOW_PAGES_H
#define WHISKERMENU_WINDOW_PAGES_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

enum class ApplicationOpeningTarget
{
	Favorites,
	Recent,
	All
};

/* resolve_application_opening_target:
 * @configured: stored Settings::DefaultCategory value.
 * @favorites_available: whether the Favourites target can be presented.
 * @recent_available: whether Recently Used can be presented.
 *
 * Resolves a publication-local fallback without changing the stored default.
 * All Applications is the stable fallback and is always available.
 *
 * Returns: the target to activate for this publication.
 */
inline ApplicationOpeningTarget resolve_application_opening_target(int configured,
		bool favorites_available, bool recent_available)
{
	if (configured == 0)
	{
		return favorites_available
				? ApplicationOpeningTarget::Favorites
				: ApplicationOpeningTarget::All;
	}
	if (configured == 1)
	{
		return recent_available
				? ApplicationOpeningTarget::Recent
				: ApplicationOpeningTarget::All;
	}
	return ApplicationOpeningTarget::All;
}

inline bool category_toggle_transition_is_active(GtkToggleButton* button)
{
	return GTK_IS_TOGGLE_BUTTON(button)
			&& gtk_toggle_button_get_active(button);
}

}

/* window-pages: private translation unit hosting Window's page-switching
 * logic. It owns the methods that change which child of m_panels_stack is
 * visible and which sidebar/category button is active (favorites/recent/
 * applications toggles, default-button reset, Apps↔Places mode switch, the
 * search-driven page swap, and active-page/active-category lookup).
 * Geometry, focus/grab, and positioning logic remain in their own units.
 */

#endif // WHISKERMENU_WINDOW_PAGES_H
