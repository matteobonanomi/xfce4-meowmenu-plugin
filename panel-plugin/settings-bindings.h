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

#ifndef WHISKERMENU_SETTINGS_BINDINGS_H
#define WHISKERMENU_SETTINGS_BINDINGS_H

#include <glib.h>

/* settings-bindings: private translation unit hosting the Xfconf binding
 * helper classes (Boolean, Integer, String, StringList, SearchActionList).
 * Their declarations remain in settings.h next to the Settings class they
 * are friend-coupled to; only the implementations live here.
 */

namespace WhiskerMenu
{

enum class SearchActionPropertyField
{
	Invalid,
	Name,
	Pattern,
	Command,
	Regex
};

/* parse_search_action_property:
 * @property: Xfconf property path to classify.
 * @size: number of configured actions; valid indexes are [0, size).
 * @index: receives the parsed action index when valid.
 * @field: receives the recognized action field when valid.
 *
 * Recognizes /search-actions/action-N/FIELD paths and rejects negative,
 * oversized, malformed, or unknown values before callers index the action
 * vector. Unknown action paths are still considered handled by the
 * search-action binding; they simply do not mutate anything.
 *
 * Returns: true when @property belongs to the search-action namespace.
 */
bool parse_search_action_property(const gchar* property, int size,
		int* index, SearchActionPropertyField* field);

}

#endif // WHISKERMENU_SETTINGS_BINDINGS_H
