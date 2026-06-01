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

#include "window-keyboard.h"

#include <gdk/gdkkeysyms.h>

namespace WhiskerMenu
{
namespace Keyboard
{

bool zone_active(Zone z, VisibilityMask mask, MenuState state)
{
	bool visible = false;
	switch (z)
	{
	case Zone::Search:  visible = mask.search;  break;
	case Zone::Results: visible = mask.results; break;
	case Zone::Sidebar: visible = mask.sidebar; break;
	case Zone::Profile: visible = mask.profile; break;
	}
	if (!visible)
		return false;

	// NOTE: Sidebar is the only state-dependent inert zone (FR-046).
	if (z == Zone::Sidebar && state == MenuState::Searching)
		return false;

	return true;
}

Zone next_zone(VisibilityMask mask,
               MenuState     state,
               Zone          current,
               Direction     direction)
{
	const std::size_t N = CANONICAL_CYCLE.size();

	// Count active zones first; if there are none the contract returns
	// `current` unchanged (FR-030 guarantees Search and Results are
	// active, so this branch is unreachable in practice).
	std::size_t active_count = 0;
	for (Zone z : CANONICAL_CYCLE)
	{
		if (zone_active(z, mask, state))
			++active_count;
	}
	if (active_count == 0)
		return current;

	// Locate the current zone's canonical index. The algorithm walks
	// CANONICAL_CYCLE from that anchor and returns the first active
	// zone in the requested direction, wrapping at the ends. When
	// `current` is itself inert (e.g. the user just typed and Sidebar
	// went inert mid-cycle) the anchor remains its canonical position,
	// so the next active zone after Sidebar in Forward direction is
	// Profile (the mode toggle is no longer a cycle stop, FR-007).
	std::size_t anchor = 0;
	for (std::size_t k = 0; k < N; ++k)
	{
		if (CANONICAL_CYCLE[k] == current)
		{
			anchor = k;
			break;
		}
	}

	const int step = (direction == Direction::Forward) ? 1 : -1;
	for (std::size_t i = 1; i <= N; ++i)
	{
		const std::size_t idx = (anchor + i * step + N * N) % N;
		const Zone candidate = CANONICAL_CYCLE[idx];
		if (zone_active(candidate, mask, state))
			return candidate;
	}

	return current;
}

TabAction tab_action(bool places_available)
{
	return places_available ? TabAction::ToggleMode : TabAction::Inert;
}

EscState classify_esc_state(bool context_menu_open,
                            bool resize_in_progress,
                            bool query_non_empty)
{
	if (context_menu_open)
		return EscState::ContextMenuOpen;
	if (resize_in_progress)
		return EscState::ResizeInProgress;
	if (query_non_empty)
		return EscState::QueryNonEmpty;
	return EscState::MenuOpen;
}

EscAction esc_action(EscState state)
{
	switch (state)
	{
	case EscState::ContextMenuOpen:  return EscAction::CloseContextMenu;
	case EscState::ResizeInProgress: return EscAction::CancelResize;
	case EscState::QueryNonEmpty:    return EscAction::ClearQuery;
	case EscState::MenuOpen:         return EscAction::CloseMenu;
	}
	// Unreachable; total over the enum.
	return EscAction::CloseMenu;
}

namespace
{

/* is_bare_modifier_keyval:
 * @keyval: the GDK keysym from a key-press event.
 *
 * Lift-and-test for the bare-modifier set listed in
 * contracts/key-routing.md §"Bare modifiers".
 *
 * Returns: true iff @keyval names a modifier-only key.
 */
bool is_bare_modifier_keyval(guint keyval)
{
	switch (keyval)
	{
	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
	case GDK_KEY_Super_L:
	case GDK_KEY_Super_R:
	case GDK_KEY_Hyper_L:
	case GDK_KEY_Hyper_R:
	case GDK_KEY_Meta_L:
	case GDK_KEY_Meta_R:
	case GDK_KEY_ISO_Level3_Shift:
	case GDK_KEY_ISO_Level5_Shift:
	case GDK_KEY_Caps_Lock:
	case GDK_KEY_Num_Lock:
		return true;
	default:
		return false;
	}
}

/* is_function_utility_keyval:
 * @keyval: the GDK keysym from a key-press event.
 *
 * Tests against the FunctionUtility set from contracts/key-routing.md
 * §"Function and utility keys". Tab/Backspace/Enter/Escape are NOT
 * in this set — they are handled by dedicated branches upstream and
 * must never reach the post-default catch-all.
 *
 * Returns: true iff @keyval is in the function/utility set.
 */
bool is_function_utility_keyval(guint keyval)
{
	if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12)
		return true;
	switch (keyval)
	{
	case GDK_KEY_Insert:
	case GDK_KEY_KP_Insert:
	case GDK_KEY_Delete:
	case GDK_KEY_KP_Delete:
	case GDK_KEY_Print:
	case GDK_KEY_Scroll_Lock:
	case GDK_KEY_Pause:
	case GDK_KEY_Menu:
	case GDK_KEY_Home:
	case GDK_KEY_KP_Home:
	case GDK_KEY_End:
	case GDK_KEY_KP_End:
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left:
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right:
		return true;
	default:
		return false;
	}
}

} // namespace

KeyClass classify_key(const GdkEventKey* event)
{
	if (!event)
		return KeyClass::FunctionUtility;

	// IME first: GTK reserves bit 25 of GdkModifierType to mark
	// IM-consumed events. Anything carrying that bit MUST be left
	// alone so the in-progress composition can complete.
	if (event->state & GDK_MODIFIER_RESERVED_25_MASK)
		return KeyClass::ImeComposition;

	if (event->is_modifier || is_bare_modifier_keyval(event->keyval))
		return KeyClass::ModifierOnly;

	if (is_function_utility_keyval(event->keyval))
		return KeyClass::FunctionUtility;

	// Printable test: a key produces a character (gdk_keyval_to_unicode
	// returns non-zero) AND neither Ctrl nor Alt is held. Shift alone
	// is fine — it produces uppercase letters and shifted symbols.
	const guint32 unichar = gdk_keyval_to_unicode(event->keyval);
	if (unichar == 0)
		return KeyClass::FunctionUtility;

	// NOTE: control characters (Tab 0x09, Backspace 0x08, Return 0x0d,
	// Escape 0x1b, etc.) all have non-zero gdk_keyval_to_unicode but
	// must NOT be routed into the search entry as printable text:
	// they each have dedicated dispatch paths (zone cycling on Tab,
	// activation on Return, the Esc ladder, and an explicit Backspace
	// branch for FR-013). Excluding them here keeps the post-default
	// printable catch-all from accidentally inserting "\b", "\t",
	// "\r", or "\x1b" into the query when focus is off the entry.
	if (unichar < 0x20 || unichar == 0x7F)
		return KeyClass::FunctionUtility;

	if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
		return KeyClass::FunctionUtility;

	return KeyClass::Printable;
}

bool is_printable_for_search(const GdkEventKey* event)
{
	return classify_key(event) == KeyClass::Printable;
}

} // namespace Keyboard
} // namespace WhiskerMenu
