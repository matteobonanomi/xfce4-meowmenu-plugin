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
const char* const PROFILE_TOP_LEFT    = "top-left";
const char* const PROFILE_BOTTOM_LEFT = "bottom-left";
const char* const PROFILE_HIDDEN      = "hidden";
const char* const COMMANDS_TOP        = "top-right";
const char* const COMMANDS_BOTTOM     = "bottom-right";
const char* const COMMANDS_HIDDEN = "hidden";

bool str_eq(const char* a, const char* b)
{
	return a && b && std::strcmp(a, b) == 0;
}

const char* normalize_profile_position(const char* value, bool* changed)
{
	const char* resolved = PROFILE_TOP_LEFT;
	bool rewrote = false;

	if (!value || !*value)
	{
		resolved = PROFILE_TOP_LEFT;
	}
	else if (str_eq(value, PROFILE_HIDDEN))
	{
		resolved = PROFILE_HIDDEN;
	}
	else if (str_eq(value, PROFILE_TOP_LEFT) || str_eq(value, "top"))
	{
		resolved = PROFILE_TOP_LEFT;
		rewrote = !str_eq(value, PROFILE_TOP_LEFT);
	}
	else if (str_eq(value, PROFILE_BOTTOM_LEFT)
			|| str_eq(value, "bottom")
			|| str_eq(value, "bottom-right"))
	{
		resolved = PROFILE_BOTTOM_LEFT;
		rewrote = !str_eq(value, PROFILE_BOTTOM_LEFT);
	}
	else
	{
		resolved = PROFILE_TOP_LEFT;
		rewrote = true;
	}

	if (changed)
		*changed = rewrote;

	return resolved;
}

const char* normalize_commands_position(const char* value, bool* changed)
{
	const char* resolved = COMMANDS_TOP;
	bool rewrote = false;

	if (!value || !*value)
	{
		resolved = COMMANDS_TOP;
	}
	else if (str_eq(value, COMMANDS_HIDDEN))
	{
		resolved = COMMANDS_HIDDEN;
	}
	else if (str_eq(value, COMMANDS_BOTTOM))
	{
		resolved = COMMANDS_BOTTOM;
	}
	else if (str_eq(value, COMMANDS_TOP))
	{
		resolved = COMMANDS_TOP;
	}
	else
	{
		resolved = COMMANDS_TOP;
		rewrote = true;
	}

	if (changed)
		*changed = rewrote;

	return resolved;
}

bool row_is_bottom(UserSessionRowEdge row_edge)
{
	return row_edge == UserSessionRowEdge::Bottom;
}

}

/* layout_mode_from_key:
 * See user-session-layout.h for the full contract. NULL and any unrecognised
 * value fall through to Docked — the safe windowed default — so callers can
 * read /layout-mode defensively without a separate validity check and without
 * ever normalising the stored value.
 */
LayoutMode
WhiskerMenu::layout_mode_from_key(const char* value)
{
	if (str_eq(value, "centered"))
		return LayoutMode::Centered;
	if (str_eq(value, "fullscreen"))
		return LayoutMode::FullScreen;
	// "docked", empty, NULL, or any unknown string → Docked.
	return LayoutMode::Docked;
}

/* control_enabled:
 * See user-session-layout.h. Direct transcription of the the documented behavior matrix:
 * size/shape controls are enabled in both windowed modes (Docked, Centered)
 * and greyed in Full-Screen; the panel gap is enabled only in Docked.
 */
bool
WhiskerMenu::control_enabled(LayoutControl control, LayoutMode mode)
{
	switch (control)
	{
	case LayoutControl::MenuWidth:
	case LayoutControl::MenuHeight:
	case LayoutControl::CornerRadius:
		return mode != LayoutMode::FullScreen;
	case LayoutControl::PanelGap:
		return mode == LayoutMode::Docked;
	}
	return false;
}

bool
WhiskerMenu::profile_position_is_hidden(const char* value)
{
	return str_eq(normalize_profile_position(value, nullptr), PROFILE_HIDDEN);
}

bool
WhiskerMenu::profile_position_is_bottom(const char* value)
{
	return str_eq(normalize_profile_position(value, nullptr), PROFILE_BOTTOM_LEFT);
}

bool
WhiskerMenu::commands_position_is_hidden(const char* value)
{
	return str_eq(normalize_commands_position(value, nullptr), COMMANDS_HIDDEN);
}

bool
WhiskerMenu::commands_position_is_bottom(const char* value)
{
	return str_eq(normalize_commands_position(value, nullptr), COMMANDS_BOTTOM);
}

const char*
WhiskerMenu::profile_position_for_row(UserSessionRowEdge row_edge)
{
	switch (row_edge)
	{
	case UserSessionRowEdge::Top:
		return PROFILE_TOP_LEFT;
	case UserSessionRowEdge::Bottom:
		return PROFILE_BOTTOM_LEFT;
	case UserSessionRowEdge::None:
	default:
		return PROFILE_HIDDEN;
	}
}

const char*
WhiskerMenu::commands_position_for_row(UserSessionRowEdge row_edge)
{
	switch (row_edge)
	{
	case UserSessionRowEdge::Top:
		return COMMANDS_TOP;
	case UserSessionRowEdge::Bottom:
		return COMMANDS_BOTTOM;
	case UserSessionRowEdge::None:
	default:
		return COMMANDS_HIDDEN;
	}
}

/* normalize_user_session:
 * See user-session-layout.h for the full contract. The body is a direct
 * transcription of the coupling-matrix rule tables: Docked resolves conflicting
 * visible pairs to the Profile row on passive load, while FullScreen resolves
 * both visible rows to the search-bar edge. Legacy Profile aliases are folded
 * to canonical storage before any row logic runs. A "hidden" value is always
 * carried through untouched and its *_hidden_enabled mask is always true.
 */
UserSessionResolution
WhiskerMenu::normalize_user_session(LayoutMode mode, const char* search_bar_pos,
                                    const char* profile_pos, const char* commands_pos)
{
	bool profile_alias_changed = false;
	bool commands_alias_changed = false;
	const char* normalized_profile = normalize_profile_position(profile_pos,
			&profile_alias_changed);
	const char* normalized_commands = normalize_commands_position(commands_pos,
			&commands_alias_changed);

	// Defensive canonicalisation: missing/unknown inputs fall back to the
	// schema defaults so the function is total over its declared domain.
	const bool profile_hidden  = str_eq(normalized_profile, PROFILE_HIDDEN);
	const bool commands_hidden = str_eq(normalized_commands, COMMANDS_HIDDEN);
	const bool profile_bottom_in  = str_eq(normalized_profile, PROFILE_BOTTOM_LEFT);
	const bool commands_bottom_in = str_eq(normalized_commands, COMMANDS_BOTTOM);
	const bool search_bottom = str_eq(search_bar_pos, "bottom");

	UserSessionResolution r = { };
	// Hidden is never produced by coupling, so it is always a legal choice.
	r.profile_hidden_enabled  = true;
	r.commands_hidden_enabled = true;
	r.profile_changed  = profile_alias_changed;
	r.commands_changed = commands_alias_changed;
	r.profile_visible = !profile_hidden;
	r.commands_visible = !commands_hidden;
	r.row_edge = UserSessionRowEdge::None;

	if (mode == LayoutMode::Docked)
	{
		if (profile_hidden)
		{
			// Profile hidden frees both visible rows; lone visible Commands keep
			// their own row and a hidden Commands cluster keeps the row absent.
			r.profile_position = PROFILE_HIDDEN;
			r.commands_position = commands_hidden
					? COMMANDS_HIDDEN
					: (commands_bottom_in ? COMMANDS_BOTTOM : COMMANDS_TOP);
			if (!commands_hidden)
				r.row_edge = commands_bottom_in ? UserSessionRowEdge::Bottom
						: UserSessionRowEdge::Top;
		}
		else
		{
			// Profile drives passive load normalization: when both clusters are
			// visible they share the Profile row, otherwise the visible cluster
			// keeps its own edge and the hidden one stays hidden.
			r.row_edge = profile_bottom_in ? UserSessionRowEdge::Bottom
					: UserSessionRowEdge::Top;
			r.profile_position = profile_position_for_row(r.row_edge);

			if (commands_hidden)
			{
				r.commands_position = COMMANDS_HIDDEN;
			}
			else
			{
				r.commands_position = commands_position_for_row(r.row_edge);
				r.commands_changed = r.commands_changed
						|| (profile_bottom_in != commands_bottom_in);
			}
		}

		if (r.profile_visible && r.commands_visible)
		{
			r.profile_top_left_enabled     = !row_is_bottom(r.row_edge);
			r.profile_bottom_left_enabled  =  row_is_bottom(r.row_edge);
			r.commands_top_right_enabled   = !row_is_bottom(r.row_edge);
			r.commands_bottom_right_enabled = row_is_bottom(r.row_edge);
		}
		else
		{
			r.profile_top_left_enabled     = true;
			r.profile_bottom_left_enabled  = true;
			r.commands_top_right_enabled   = true;
			r.commands_bottom_right_enabled = true;
		}
	}
	else
	{
		// Full-screen parity: the search-bar edge remains authoritative for any
		// visible user/session cluster.
		r.profile_top_left_enabled     = !search_bottom;
		r.profile_bottom_left_enabled  =  search_bottom;
		r.commands_top_right_enabled   = !search_bottom;
		r.commands_bottom_right_enabled = search_bottom;
		r.row_edge = (profile_hidden && commands_hidden)
				? UserSessionRowEdge::None
				: (search_bottom ? UserSessionRowEdge::Bottom
						: UserSessionRowEdge::Top);

		if (profile_hidden)
		{
			r.profile_position = PROFILE_HIDDEN;
		}
		else
		{
			r.profile_position = profile_position_for_row(r.row_edge);
			r.profile_changed = r.profile_changed
					|| (search_bottom != profile_bottom_in);
		}

		if (commands_hidden)
		{
			r.commands_position = COMMANDS_HIDDEN;
		}
		else
		{
			r.commands_position = commands_position_for_row(r.row_edge);
			r.commands_changed = r.commands_changed
					|| (search_bottom != commands_bottom_in);
		}
	}

	return r;
}
