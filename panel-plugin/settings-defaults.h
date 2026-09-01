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

#ifndef WHISKERMENU_SETTINGS_DEFAULTS_H
#define WHISKERMENU_SETTINGS_DEFAULTS_H

#include <cstring>

typedef struct _XfconfChannel XfconfChannel;

namespace WhiskerMenu
{

/* should_apply_fresh_preset:
 * @marker: true when the profile has previously completed initialization.
 * @empty_channel: true when no plugin settings existed at load time.
 *
 * Keeps first-install preset application separate from upgrade migration. An
 * existing profile is never reset merely because its marker needs backfilling.
 *
 * Returns: true only for a genuinely new profile.
 */
inline bool should_apply_fresh_preset(bool marker, bool empty_channel)
{
	return !marker && empty_channel;
}

/* settings_relative_property:
 * @property: absolute Xfconf property returned by a property-base channel.
 * @base: absolute plugin property base without a trailing slash.
 *
 * Separates real plugin settings from the panel registration node stored at
 * the base itself. Xfconf includes that node in property enumeration even
 * though it identifies the panel slot rather than persisted launcher state.
 *
 * Returns: the slash-prefixed relative setting, or nullptr for the base node,
 * an empty child, or a property outside the requested base.
 */
inline const char* settings_relative_property(const char* property,
		const char* base)
{
	if (!property || !base)
		return nullptr;
	const std::size_t base_length = std::strlen(base);
	if (std::strncmp(property, base, base_length) != 0
			|| property[base_length] != '/'
			|| property[base_length + 1] == '\0')
	{
		return nullptr;
	}
	return property + base_length;
}

/* migrate_layout_schema_v13:
 * @channel: property-base-anchored panel Xfconf channel.
 *
 * Applies the bounded layout cleanup introduced by schema version 13. The
 * caller remains responsible for advancing /schema-version after updating its
 * in-memory setting. Unrelated properties, including obsolete reset markers,
 * are never inspected or changed.
 *
 * Returns: the canonical sidebar value when it changed, otherwise nullptr.
 */
const char* migrate_layout_schema_v13(XfconfChannel* channel);

}

/* settings-defaults: private translation unit for the schema-version
 * defaults table and the forward schema migration that seeds it. The
 * declarations live alongside Settings (settings.h) because they are
 * member functions; this header exists only so the implementation file
 * remains a recognised compilation unit with a matching header.
 *
 * The Xfconf legacy-import path used on first launch from Whisker lives
 * in migration.cpp and is intentionally NOT consolidated here.
 */

#endif // WHISKERMENU_SETTINGS_DEFAULTS_H
