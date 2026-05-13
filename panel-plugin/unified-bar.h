/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_UNIFIED_BAR_H
#define WHISKERMENU_UNIFIED_BAR_H

namespace WhiskerMenu
{

class Settings;

/* unified_bar_preconditions_raw:
 * @layout_mode:        e.g. "fullscreen", "docked".
 * @search_bar_position: e.g. "top", "bottom".
 * @profile_position:    e.g. "top", "bottom", "hidden".
 * @commands_position:   e.g. "top-right", "bottom-right", "hidden".
 *
 * Pure variant of unified_bar_preconditions_met() that takes raw strings
 * so it can be unit-tested without instantiating Settings.
 *
 * Returns: true iff layout is fullscreen AND all non-neutral vertical ends
 *          agree, AND search_bar_position is a real vertical end.
 */
bool unified_bar_preconditions_raw(const char* layout_mode,
                                    const char* search_bar_position,
                                    const char* profile_position,
                                    const char* commands_position);

/* unified_bar_preconditions_met:
 * @s: the current settings value-bag.
 *
 * Returns true when the active layout permits the unified profile/search/
 * session bar: FullScreen layout mode AND the three vertical-end position
 * keys (/search-bar-position, /profile-position, /commands-position)
 * collapse to the same end (all top or all bottom). The "hidden" profile
 * case is treated as transparent.
 *
 * Returns: true if a unified bar would be visually coherent now.
 */
bool unified_bar_preconditions_met(const Settings& s);

/* unified_bar_effective:
 * @s: the current settings value-bag.
 *
 * Returns: true iff the user setting is on AND the preconditions are met.
 */
bool unified_bar_effective(const Settings& s);

}

#endif // WHISKERMENU_UNIFIED_BAR_H
