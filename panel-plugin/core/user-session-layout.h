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

#ifndef WHISKERMENU_USER_SESSION_LAYOUT_H
#define WHISKERMENU_USER_SESSION_LAYOUT_H

namespace WhiskerMenu
{

// Which window layout governs the Profile/Commands edge coupling. Derived from
// the /layout-mode key: "docked" → Docked, "fullscreen" → FullScreen.
enum class LayoutMode
{
	Docked,
	FullScreen
};

// Outcome of resolving a (profile_position, commands_position) pair against the
// active layout's coupling rule. Holds both the coherent values to render and
// (when changed) to persist, and the per-option enabled masks the Preferences
// combos use to grey disallowed — but still listed — options.
struct UserSessionResolution
{
	// Resolved, coupling-valid values. Pointers into static string literals
	// ("top" | "bottom" | "hidden", "top-right" | "bottom-right" | "hidden");
	// never NULL, never owned by the caller.
	const char* profile_position;
	const char* commands_position;

	// Per-option combo sensitivity (greyed, not removed — FR-010). The two
	// *_hidden_enabled masks are always true: visibility is only ever changed
	// by the user, never forced by coupling.
	bool profile_top_enabled;
	bool profile_bottom_enabled;
	bool profile_hidden_enabled;
	bool commands_top_right_enabled;
	bool commands_bottom_right_enabled;
	bool commands_hidden_enabled;

	// True when the resolved value differs from the corresponding input, i.e.
	// the caller snapped a disallowed edge and must persist the new value so the
	// stored configuration and the rendered row stay in sync (FR-014/FR-017).
	bool profile_changed;
	bool commands_changed;
};

/* normalize_user_session:
 * @mode:           Docked or FullScreen (from /layout-mode).
 * @search_bar_pos: "top" | "bottom"; governs both edges in FullScreen, ignored
 *                  in Docked. NULL is treated as "top".
 * @profile_pos:    stored "top" | "bottom" | "hidden". NULL is treated as "top".
 * @commands_pos:   stored "top-right" | "bottom-right" | "hidden". NULL is
 *                  treated as "top-right".
 *
 * Pure decision: depends only on the four inputs, reads no Xfconf, touches no
 * widget, holds no state. Resolves any coupling-invalid edge toward the
 * governing edge (the Profile edge in Docked, the search-bar edge in
 * FullScreen) without ever turning a visible component "hidden" and without
 * ever un-hiding a "hidden" one; reports the per-option enabled masks for the
 * dialog. Idempotent: feeding a resolved pair back in yields the same pair with
 * both *_changed flags false.
 *
 * Returns: a UserSessionResolution by value. No allocation, no ownership
 * transfer; the embedded const char* point at static literals.
 */
UserSessionResolution
normalize_user_session(LayoutMode mode, const char* search_bar_pos,
                       const char* profile_pos, const char* commands_pos);

}

#endif // WHISKERMENU_USER_SESSION_LAYOUT_H
