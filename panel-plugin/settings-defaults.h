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
