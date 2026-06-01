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

#ifndef MEOWMENU_CORE_WINDOW_KEYBOARD_H
#define MEOWMENU_CORE_WINDOW_KEYBOARD_H

#include <array>
#include <cstdint>

#include <gdk/gdk.h>

namespace WhiskerMenu
{
namespace Keyboard
{

/* Logical focusable region of the menu. The cycle order used by Ctrl+Tab /
 * Ctrl+Shift+Tab is the declared numeric order below; see CANONICAL_CYCLE.
 * The Apps/Places mode toggle is deliberately absent: it is operated by Tab
 * and is never a keyboard-focusable cycle stop (FR-007). */
enum class Zone : unsigned
{
	Search  = 0,
	Results = 1,
	Sidebar = 2,
	Profile = 3,
};

/* Reflects whether the user is currently typing a query. The sidebar is
 * inert (skipped from the Ctrl+Tab cycle) while Searching. */
enum class MenuState : unsigned
{
	Browsing,
	Searching,
};

enum class Direction : unsigned
{
	Forward,
	Backward,
};

/* What a bare Tab / Shift+Tab press does. Encodes FR-006: Tab always means
 * "switch Applications/Places" and never silently becomes area cycling. */
enum class TabAction : unsigned
{
	ToggleMode,  // Places is available — flip Apps⇄Places
	Inert,       // Places unavailable — do nothing, consume the event
};

/* tab_action:
 * @places_available: whether the Places mode is enabled and thus a valid
 *                    target for the toggle (Window::m_settings->places_enabled).
 *
 * Pure, total, side-effect-free decision for a bare Tab/Shift+Tab press.
 * Keeping it a free function lets the FR-006 invariant be unit-tested
 * without instantiating any GTK widgets.
 *
 * Returns: TabAction::ToggleMode when Places is available, otherwise
 * TabAction::Inert.
 */
TabAction tab_action(bool places_available);

/* Per-zone visibility mask folded from the existing m_layout_* flags
 * and the preset's per-zone "hidden" positions. Search and Results
 * are forced visible per FR-030. */
struct VisibilityMask
{
	bool search  = true;
	bool results = true;
	bool sidebar = true;
	bool profile = true;
};

/* Canonical, locale-independent focus-area cycle order, walked by Ctrl+Tab
 * (Forward) and Ctrl+Shift+Tab (Backward). RTL does not reverse it; arrow
 * keys handle visual direction separately (FR-120). The Apps/Places mode
 * toggle is intentionally NOT a member: it is operated by Tab and is never
 * a cycle stop (FR-007). */
constexpr std::array<Zone, 4> CANONICAL_CYCLE = {
	Zone::Search, Zone::Results, Zone::Sidebar, Zone::Profile,
};

/* zone_active:
 * @z: the zone in question.
 * @mask: the visibility mask folded from layout flags.
 * @state: Browsing or Searching.
 *
 * True iff @z is both visible (per @mask) and not inert in the given
 * @state. Sidebar is the only zone whose activity depends on @state
 * (inert while Searching, per FR-046).
 */
bool zone_active(Zone z, VisibilityMask mask, MenuState state);

/* next_zone:
 * @mask: visibility mask folded from layout flags.
 * @state: Browsing or Searching.
 * @current: currently focused zone (may itself be inert/hidden — treated
 *           as a virtual position before filtered[0] going Forward, or
 *           after filtered[N-1] going Backward).
 * @direction: Forward for Ctrl+Tab, Backward for Ctrl+Shift+Tab.
 *
 * Returns the next zone to receive focus, walking CANONICAL_CYCLE with
 * the inactive zones filtered out and wrapping around at the ends.
 * Falls back to @current if no zone is active (unreachable in practice
 * because Search and Results are never hidden).
 */
Zone next_zone(VisibilityMask mask,
               MenuState     state,
               Zone          current,
               Direction     direction);

/* States the next Esc press operates on, highest priority first. */
enum class EscState : unsigned
{
	ContextMenuOpen  = 0,
	ResizeInProgress = 1,
	QueryNonEmpty    = 2,
	MenuOpen         = 3,
};

/* Single-step action selected by the Esc ladder. */
enum class EscAction : unsigned
{
	CloseContextMenu,
	CancelResize,
	ClearQuery,
	CloseMenu,
};

/* classify_esc_state:
 * @context_menu_open: a launcher/places context menu currently has the
 *                     grab.
 * @resize_in_progress: Window is mid-resize (m_resizing).
 * @query_non_empty: the search entry currently contains at least one
 *                   character.
 *
 * Strict priority: ContextMenuOpen > ResizeInProgress > QueryNonEmpty
 * > MenuOpen. The first true predicate, top-down, wins.
 *
 * Returns: the EscState that the ladder should dispatch on.
 */
EscState classify_esc_state(bool context_menu_open,
                            bool resize_in_progress,
                            bool query_non_empty);

/* esc_action:
 * @state: the classified Esc state.
 *
 * Pure mapping from EscState to EscAction. One Esc press, one action;
 * subsequent presses operate on the resulting state.
 *
 * Returns: the action the caller must perform; the caller MUST return
 * GDK_EVENT_STOP after invoking it.
 */
EscAction esc_action(EscState state);

/* Classification used by the type-to-search post-default handler. */
enum class KeyClass : unsigned
{
	ImeComposition,  // part of an active IME sequence; do not route
	ModifierOnly,    // bare modifier press (Shift, Ctrl, ...)
	FunctionUtility, // F1..F12, Insert, Delete, navigation keys, ...
	Printable,       // candidate for routing into the search entry
};

/* classify_key:
 * @event: the GdkEventKey* delivered to the post-default handler.
 *
 * Applies the rules from contracts/key-routing.md in order:
 *   1. IM-composition bit (GDK_MODIFIER_RESERVED_25_MASK)
 *   2. Bare-modifier set
 *   3. Function / utility keyval set
 *   4. Printable test (gdk_keyval_to_unicode != 0 and no Ctrl/Alt held)
 *
 * Total function over GdkEventKey*; every input maps to exactly one
 * KeyClass. Space classifies as Printable (FR-010).
 *
 * Returns: the KeyClass of the event.
 */
KeyClass classify_key(const GdkEventKey* event);

/* is_printable_for_search:
 * @event: the GdkEventKey* delivered to the post-default handler.
 *
 * Convenience predicate: classify_key(event) == KeyClass::Printable.
 *
 * Returns: true iff the event should be routed to the search entry.
 */
bool is_printable_for_search(const GdkEventKey* event);

/* Monotonic-clock guard absorbing key-repeat bursts on Enter against a
 * single launchable (FR-022, SC-005). Shared between launcher and
 * profile-bar activation sites. */
struct ActivationDebounce
{
	gint64 last_at      = 0;
	gint64 threshold_us = 250 * 1000;

	/* accept:
	 * @now: g_get_monotonic_time() at call site.
	 *
	 * Returns true and updates @last_at iff @now - @last_at >=
	 * @threshold_us. Otherwise returns false without mutating state.
	 */
	bool accept(gint64 now)
	{
		if (now - last_at >= threshold_us)
		{
			last_at = now;
			return true;
		}
		return false;
	}
};

} // namespace Keyboard
} // namespace WhiskerMenu

#endif // MEOWMENU_CORE_WINDOW_KEYBOARD_H
