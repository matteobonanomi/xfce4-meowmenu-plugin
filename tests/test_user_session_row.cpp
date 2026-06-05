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

#include <cassert>

/* Mirror of user_session_row_visible() in panel-plugin/core/window.cpp. */
static bool user_session_row_visible(bool unified, bool profile_hidden,
                                     bool commands_hidden, bool categories_alternate)
{
	if (unified)
		return true;
	if (!profile_hidden)
		return true;
	if (commands_hidden)
		return false;
	return !categories_alternate;
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

	// Profile hidden but commands visible (docked): the row stays, following the
	// existing bottom-categories suppression.
	assert(user_session_row_visible(false, true, false, false) == true);
	assert(user_session_row_visible(false, true, false, true)  == false);

	return 0;
}
