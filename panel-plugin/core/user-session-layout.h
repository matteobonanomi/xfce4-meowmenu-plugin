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

// The window layout selected by the /layout-mode key: "docked" → Docked,
// "centered" → Centered, "fullscreen" → FullScreen. Centered is a windowed
// layout (like Docked) that floats at the monitor centre; for the Profile/
// Commands edge coupling it behaves exactly like Docked.
enum class LayoutMode
{
	Docked,
	Centered,
	FullScreen
};

/* layout_mode_from_key:
 * @value: the raw /layout-mode string. "docked", "centered", "fullscreen" map
 *         to the matching enum; any other value — including empty or NULL —
 *         maps to Docked.
 *
 * Pure, total classifier over the /layout-mode value domain. This is a
 * read-time mapping only: an unknown or stale stored value is reported as
 * Docked but never written back, so a normal read never mutates the user's
 * configuration.
 *
 * Returns: the resolved LayoutMode; never throws, reads no Xfconf.
 */
LayoutMode layout_mode_from_key(const char* value);

// The Properties "Layout" controls whose enabled/greyed state depends on the
// selected layout mode (the documented behavior matrix). Width/height/corner-radius are windowed
// controls; the panel gap only means something flush against a panel edge.
enum class LayoutControl
{
	MenuWidth,
	MenuHeight,
	PanelGap,
	CornerRadius
};

// The shared top/bottom edge occupied by the user/session row after passive
// normalization. None means both clusters are hidden.
enum class UserSessionRowEdge
{
	None,
	Top,
	Bottom
};

/* control_enabled:
 * @control: which Layout-section control is being queried.
 * @mode:    the active layout mode.
 *
 * Pure decision table encoding the the documented behavior control-sensitivity matrix. The
 * single source of truth for which Layout controls are sensitive in each mode;
 * the Properties dialog calls this per registered widget so the matrix lives in
 * exactly one place.
 *
 * Returns: true if @control should be sensitive (enabled) under @mode.
 */
bool control_enabled(LayoutControl control, LayoutMode mode);

/* profile_position_is_hidden:
 * @value: a stored or incoming profile-position string.
 *
 * Treats canonical values and tolerated legacy aliases equivalently so callers
 * can classify visibility without duplicating string tables.
 *
 * Returns: true iff @value resolves to the hidden profile state.
 */
bool profile_position_is_hidden(const char* value);

/* profile_position_is_bottom:
 * @value: a stored or incoming profile-position string.
 *
 * Accepts canonical values (`bottom-left`) and legacy aliases (`bottom`,
 * `bottom-right`) so callers can classify the requested row before or after
 * passive normalization.
 *
 * Returns: true iff @value resolves to the bottom profile row.
 */
bool profile_position_is_bottom(const char* value);

/* commands_position_is_hidden:
 * @value: a stored or incoming commands-position string.
 *
 * Returns: true iff @value resolves to the hidden commands state.
 */
bool commands_position_is_hidden(const char* value);

/* commands_position_is_bottom:
 * @value: a stored or incoming commands-position string.
 *
 * Returns: true iff @value resolves to the bottom commands row.
 */
bool commands_position_is_bottom(const char* value);

/* profile_position_for_row:
 * @row_edge: the resolved row edge, or None when no visible row remains.
 *
 * Returns: the canonical profile-position string for @row_edge. None maps to
 *          `hidden`.
 */
const char* profile_position_for_row(UserSessionRowEdge row_edge);

/* commands_position_for_row:
 * @row_edge: the resolved row edge, or None when no visible row remains.
 *
 * Returns: the canonical commands-position string for @row_edge. None maps to
 *          `hidden`.
 */
const char* commands_position_for_row(UserSessionRowEdge row_edge);

// Outcome of resolving a (profile_position, commands_position) pair against the
// active layout's coupling rule. Holds both the coherent values to render and
// (when changed) to persist, and the per-option enabled masks the Preferences
// combos use to grey disallowed — but still listed — options.
struct UserSessionResolution
{
	// Resolved, coupling-valid values. Pointers into static string literals
	// ("top-left" | "bottom-left" | "hidden",
	//  "top-right" | "bottom-right" | "hidden"); never NULL, never owned by
	// the caller.
	const char* profile_position;
	const char* commands_position;

	// The shared visible row after passive normalization plus the visibility
	// state of each cluster. When both clusters are hidden, row_edge == None.
	UserSessionRowEdge row_edge;
	bool profile_visible;
	bool commands_visible;

	// Per-option combo sensitivity (greyed, not removed — the documented behavior). The two
	// *_hidden_enabled masks are always true: visibility is only ever changed
	// by the user, never forced by coupling.
	bool profile_top_left_enabled;
	bool profile_bottom_left_enabled;
	bool profile_hidden_enabled;
	bool commands_top_right_enabled;
	bool commands_bottom_right_enabled;
	bool commands_hidden_enabled;

	// True when the resolved value differs from the corresponding input, i.e.
	// the caller rewrote a legacy alias or snapped a disallowed edge and must
	// persist the new value so the stored configuration and the rendered row stay
	// in sync (the documented behavior).
	bool profile_changed;
	bool commands_changed;
};

/* normalize_user_session:
 * @mode:           Docked or FullScreen (from /layout-mode).
 * @search_bar_pos: "top" | "bottom"; governs both edges in FullScreen, ignored
 *                  in Docked. NULL is treated as "top".
 * @profile_pos:    stored "top-left" | "bottom-left" | "hidden". Legacy
 *                  "top" | "bottom" | "bottom-right" aliases are accepted
 *                  and rewritten. NULL is treated as "top-left".
 * @commands_pos:   stored "top-right" | "bottom-right" | "hidden". NULL is
 *                  treated as "top-right".
 *
 * Pure decision: depends only on the four inputs, reads no Xfconf, touches no
 * widget, holds no state. Resolves any coupling-invalid edge toward the
 * governing edge (the Profile edge in Docked, the search-bar edge in
 * FullScreen) without ever turning a visible component "hidden" and without
 * ever un-hiding a "hidden" one; rewrites tolerated legacy Profile aliases to
 * canonical storage; reports the resolved row and the per-option enabled masks
 * for the dialog. Idempotent: feeding a resolved pair back in yields the same
 * pair with both *_changed flags false.
 *
 * Returns: a UserSessionResolution by value. No allocation, no ownership
 * transfer; the embedded const char* point at static literals.
 */
UserSessionResolution
normalize_user_session(LayoutMode mode, const char* search_bar_pos,
                       const char* profile_pos, const char* commands_pos);

}

#endif // WHISKERMENU_USER_SESSION_LAYOUT_H
