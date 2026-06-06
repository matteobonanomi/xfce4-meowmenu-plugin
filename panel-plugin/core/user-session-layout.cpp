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

#include "core/user-session-layout.h"

#include <cstring>

using namespace WhiskerMenu;

namespace
{

// Canonical position literals. The resolution returns pointers into these so
// the result owns no memory and every value is comparable by identity or value.
const char* const PROFILE_TOP     = "top";
const char* const PROFILE_BOTTOM  = "bottom";
const char* const PROFILE_HIDDEN  = "hidden";
const char* const COMMANDS_TOP    = "top-right";
const char* const COMMANDS_BOTTOM = "bottom-right";
const char* const COMMANDS_HIDDEN = "hidden";

bool str_eq(const char* a, const char* b)
{
	return a && b && std::strcmp(a, b) == 0;
}

}

/* normalize_user_session:
 * See user-session-layout.h for the full contract. The body is a direct
 * transcription of the coupling-matrix rule tables: §A for Docked (Commands
 * edge follows the Profile edge), §C for FullScreen (both edges follow the
 * search-bar edge), §D for the snap direction. A "hidden" value is always
 * carried through untouched and its *_hidden_enabled mask is always true.
 */
UserSessionResolution
WhiskerMenu::normalize_user_session(LayoutMode mode, const char* search_bar_pos,
                                    const char* profile_pos, const char* commands_pos)
{
	// Defensive canonicalisation: missing/unknown inputs fall back to the
	// schema defaults so the function is total over its declared domain.
	const bool profile_hidden  = str_eq(profile_pos, PROFILE_HIDDEN);
	const bool commands_hidden = str_eq(commands_pos, COMMANDS_HIDDEN);
	const bool profile_bottom_in  = str_eq(profile_pos, PROFILE_BOTTOM);
	const bool commands_bottom_in = str_eq(commands_pos, COMMANDS_BOTTOM);
	const bool search_bottom = str_eq(search_bar_pos, PROFILE_BOTTOM);

	UserSessionResolution r;
	// Hidden is never produced by coupling, so it is always a legal choice.
	r.profile_hidden_enabled  = true;
	r.commands_hidden_enabled = true;
	r.profile_changed  = false;
	r.commands_changed = false;

	if (mode == LayoutMode::Docked)
	{
		// §A: Profile is a free choice; Commands edge is coupled to it.
		r.profile_top_enabled    = true;
		r.profile_bottom_enabled = true;

		if (profile_hidden)
		{
			// Profile hidden frees both Commands edges (no governing edge).
			r.profile_position = PROFILE_HIDDEN;
			r.commands_top_right_enabled    = true;
			r.commands_bottom_right_enabled = true;
			r.commands_position = commands_hidden
					? COMMANDS_HIDDEN
					: (commands_bottom_in ? COMMANDS_BOTTOM : COMMANDS_TOP);
		}
		else
		{
			// Profile drives the governing edge; the opposite Commands edge is
			// greyed and any visible value on it snaps toward the Profile edge.
			r.profile_position = profile_bottom_in ? PROFILE_BOTTOM : PROFILE_TOP;
			r.commands_top_right_enabled    = !profile_bottom_in;
			r.commands_bottom_right_enabled =  profile_bottom_in;

			if (commands_hidden)
			{
				r.commands_position = COMMANDS_HIDDEN;
			}
			else
			{
				r.commands_position = profile_bottom_in ? COMMANDS_BOTTOM : COMMANDS_TOP;
				r.commands_changed =
						(profile_bottom_in != commands_bottom_in);
			}
		}
	}
	else
	{
		// §C: both Profile and Commands edges follow the search-bar edge; only
		// the visible-vs-hidden choice is free.
		r.profile_top_enabled    = !search_bottom;
		r.profile_bottom_enabled =  search_bottom;
		r.commands_top_right_enabled    = !search_bottom;
		r.commands_bottom_right_enabled =  search_bottom;

		if (profile_hidden)
		{
			r.profile_position = PROFILE_HIDDEN;
		}
		else
		{
			r.profile_position = search_bottom ? PROFILE_BOTTOM : PROFILE_TOP;
			r.profile_changed = (search_bottom != profile_bottom_in);
		}

		if (commands_hidden)
		{
			r.commands_position = COMMANDS_HIDDEN;
		}
		else
		{
			r.commands_position = search_bottom ? COMMANDS_BOTTOM : COMMANDS_TOP;
			r.commands_changed = (search_bottom != commands_bottom_in);
		}
	}

	return r;
}
