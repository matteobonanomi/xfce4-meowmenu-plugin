/*
 * Unit tests for the user/session-row visibility decision.
 *
 * The decision is pure (four bools in, one bool out), so it can be tested in
 * isolation without instantiating Settings or a GTK display. To keep this test
 * dependency-free — matching the other tests/ — the decision body is mirrored
 * here verbatim from user_session_row_visible() in panel-plugin/core/window.cpp.
 * If the canonical implementation ever diverges, this test should be updated in
 * lockstep.
 *
 * Covered behaviour:
 *   - both clusters hidden in a docked layout collapses the row (FR-003);
 *   - the shared row is preserved in unified-bar / full-screen (FR-004);
 *   - a single visible cluster keeps the row.
 */

#include "core/user-session-layout.h"

#include <cassert>
#include <cstring>

using namespace WhiskerMenu;

/* Mirror of user_session_row_visible() in panel-plugin/core/window.cpp. The
 * canonical implementation collapses the docked row only when both clusters are
 * hidden, independent of category placement (FR-001/004); this mirror is kept
 * in lockstep with it. */
static bool user_session_row_visible(bool unified, bool profile_hidden,
                                     bool commands_hidden, bool /*categories_alternate*/)
{
	if (unified)
		return true;
	return !(profile_hidden && commands_hidden);
}

static bool seq(const char* a, const char* b)
{
	return std::strcmp(a, b) == 0;
}

/* check_vector: assert one normalize_user_session() input→output row from the
 * coupling-matrix contract, including the *_changed expectations. */
static void check_vector(LayoutMode mode, const char* search, const char* pin,
                         const char* cin, const char* pout, const char* cout,
                         bool pchanged, bool cchanged)
{
	UserSessionResolution r = normalize_user_session(mode, search, pin, cin);
	assert(seq(r.profile_position, pout));
	assert(seq(r.commands_position, cout));
	assert(r.profile_changed == pchanged);
	assert(r.commands_changed == cchanged);
}

int main()
{
	// FR-003: docked, both profile and commands hidden → the row collapses,
	// regardless of the category arrangement.
	assert(user_session_row_visible(false, true, true, false) == false);
	assert(user_session_row_visible(false, true, true, true)  == false);

	// FR-004: unified-bar / full-screen always keeps the row (it carries the
	// shared search cluster), even when both clusters are hidden.
	assert(user_session_row_visible(true, true,  true,  false) == true);
	assert(user_session_row_visible(true, true,  true,  true)  == true);
	assert(user_session_row_visible(true, false, false, false) == true);

	// A visible profile keeps the docked row no matter what commands do.
	assert(user_session_row_visible(false, false, false, false) == true);
	assert(user_session_row_visible(false, false, true,  false) == true);
	assert(user_session_row_visible(false, false, true,  true)  == true);

	// FR-001/004: profile hidden but commands visible (docked) keeps the row —
	// and it no longer depends on category placement (the old bug suppressed it
	// when the category list sat at the bottom).
	assert(user_session_row_visible(false, true, false, false) == true);
	assert(user_session_row_visible(false, true, false, true)  == true);

	// ---------------------------------------------------------------------
	// normalize_user_session() contract (coupling-matrix §A/§C/§D).
	// Each row is "mode, search, profile_in, commands_in → profile_out,
	// commands_out, profile_changed, commands_changed".
	// ---------------------------------------------------------------------

	// Docked — Commands edge follows the Profile edge (§A); Profile is free.
	check_vector(LayoutMode::Docked, nullptr, "top",    "top-right",    "top",    "top-right",    false, false);
	check_vector(LayoutMode::Docked, nullptr, "top",    "bottom-right", "top",    "top-right",    false, true);
	check_vector(LayoutMode::Docked, nullptr, "bottom", "top-right",    "bottom", "bottom-right", false, true);
	check_vector(LayoutMode::Docked, nullptr, "hidden", "bottom-right", "hidden", "bottom-right", false, false);
	check_vector(LayoutMode::Docked, nullptr, "hidden", "hidden",       "hidden", "hidden",       false, false);

	// FullScreen — both edges follow the search-bar edge (§C/§D).
	check_vector(LayoutMode::FullScreen, "top",    "top",    "bottom-right", "top",    "top-right",    false, true);
	check_vector(LayoutMode::FullScreen, "bottom", "top",    "top-right",    "bottom", "bottom-right", true,  true);
	check_vector(LayoutMode::FullScreen, "bottom", "hidden", "top-right",    "hidden", "bottom-right", false, true);
	check_vector(LayoutMode::FullScreen, "top",    "hidden", "hidden",       "hidden", "hidden",       false, false);

	// Mask assertions.
	{
		// Docked Profile=top ⇒ Commands Bottom Right greyed; hidden always on.
		UserSessionResolution d = normalize_user_session(LayoutMode::Docked, nullptr, "top", "top-right");
		assert(d.commands_bottom_right_enabled == false);
		assert(d.commands_top_right_enabled == true);
		assert(d.profile_top_enabled == true && d.profile_bottom_enabled == true);
		assert(d.profile_hidden_enabled && d.commands_hidden_enabled);

		// FullScreen search=top ⇒ both bottom edges greyed; hidden always on.
		UserSessionResolution f = normalize_user_session(LayoutMode::FullScreen, "top", "top", "top-right");
		assert(f.profile_bottom_enabled == false);
		assert(f.commands_bottom_right_enabled == false);
		assert(f.profile_hidden_enabled && f.commands_hidden_enabled);
	}

	// Idempotency: feeding a resolved pair back in yields no further change.
	{
		UserSessionResolution once = normalize_user_session(LayoutMode::FullScreen, "bottom", "top", "top-right");
		UserSessionResolution twice = normalize_user_session(LayoutMode::FullScreen, "bottom",
				once.profile_position, once.commands_position);
		assert(seq(twice.profile_position, once.profile_position));
		assert(seq(twice.commands_position, once.commands_position));
		assert(twice.profile_changed == false && twice.commands_changed == false);
	}

	return 0;
}
