/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
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

#include "settings.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

namespace
{

static bool property_matches_any(const gchar* property, const char* const* values)
{
	for (const char* const* value = values; *value; ++value)
	{
		if (g_strcmp0(property, *value) == 0)
		{
			return true;
		}
	}
	return false;
}

}

//-----------------------------------------------------------------------------

/* classify_reload_intent:
 * @property: base-relative Xfconf property path such as "/profile-position".
 *
 * Classifies a setting notification by the narrowest live refresh that can make
 * the current UI coherent. Layout refreshes reuse existing widgets and keep the
 * current Garcon category epoch; content refreshes rebuild application/category
 * data; button refreshes touch only the panel button.
 *
 * Returns: the refresh intent for @property, or ReloadIntent::None when the
 * setting has no immediate menu/window presentation side effect.
 */
ReloadIntent WhiskerMenu::classify_reload_intent(const gchar* property)
{
	if (!property)
	{
		return ReloadIntent::None;
	}

	static const char* const button_properties[] = {
		"/button-title",
		"/button-icon",
		"/show-button-title",
		"/show-button-icon",
		"/button-single-row",
		nullptr
	};
	static const char* const content_properties[] = {
		"/favorites",
		"/recent",
		"/custom-menu-file",
		"/launcher-show-name",
		"/launcher-show-description",
		"/sort-categories",
		"/view-mode",
		"/favorites-in-recent",
		nullptr
	};
	static const char* const layout_properties[] = {
		"/launcher-show-tooltip",
		"/transparent-grid",
		"/launcher-icon-size",
		"/category-show-name",
		"/category-icon-size",
		"/default-category",
		"/recent-items-max",
		"/position-profile-alternate",
		"/position-search-alternate",
		"/position-commands-alternate",
		"/position-categories-alternate",
		"/position-categories-horizontal",
		"/stay-on-focus-out",
		"/profile-shape",
		"/confirm-session-command",
		"/menu-width",
		"/menu-height",
		"/menu-opacity",
		"/corner-radius",
		"/panel-gap",
		"/sidebar-position",
		"/sidebar-enabled",
		"/search-bar-position",
		"/profile-position",
		"/commands-position",
		"/grid-density",
		"/layout-mode",
		"/places/enabled",
		"/places/history-enabled",
		"/places/favourites-enabled",
		"/places/favourite-sync",
		"/places/max-items",
		"/places/remember-last-mode",
		"/places/last-mode",
		"/places/favourites",
		"/places/switch-show-icons",
		"/places/switch-button-shape",
		nullptr
	};

	if (property_matches_any(property, button_properties))
	{
		return ReloadIntent::Button;
	}
	if (property_matches_any(property, content_properties))
	{
		return ReloadIntent::Content;
	}
	if (property_matches_any(property, layout_properties)
			|| g_str_has_prefix(property, "/command-")
			|| g_str_has_prefix(property, "/show-command-"))
	{
		return ReloadIntent::Layout;
	}
	return ReloadIntent::None;
}
