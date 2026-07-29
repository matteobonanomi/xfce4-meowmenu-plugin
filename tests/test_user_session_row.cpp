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
 *   - both clusters hidden in a docked layout collapses the row (the documented behavior);
 *   - the shared row is preserved in unified-bar / full-screen (the documented behavior);
 *   - a single visible cluster keeps the row.
 */

#include "core/user-session-layout.h"

#include <cassert>
#include <cstring>

using namespace WhiskerMenu;

/* Mirror of user_session_row_visible() in panel-plugin/core/window.cpp. The
 * canonical implementation collapses the docked row only when both clusters are
 * hidden, independent of category placement (the documented behavior); this mirror is kept
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

static UserSessionResolution simulate_live_edit(LayoutMode mode, const char* search,
                                                const char* pin, const char* cin,
                                                bool profile_combo,
                                                const char* requested)
{
	UserSessionResolution current = normalize_user_session(mode, search, pin, cin);
	const bool requested_visible = profile_combo
			? !profile_position_is_hidden(requested)
			: !commands_position_is_hidden(requested);
	const UserSessionRowEdge requested_row = (profile_combo
			? profile_position_is_bottom(requested)
			: commands_position_is_bottom(requested))
			? UserSessionRowEdge::Bottom : UserSessionRowEdge::Top;

	const char* next_profile = pin;
	const char* next_commands = cin;

	if (profile_combo)
	{
		next_profile = requested;
		if ((mode == LayoutMode::Docked) && requested_visible && current.commands_visible)
			next_commands = commands_position_for_row(requested_row);
	}
	else
	{
		next_commands = requested;
		if ((mode == LayoutMode::Docked) && requested_visible && current.profile_visible)
			next_profile = profile_position_for_row(requested_row);
	}

	UserSessionResolution resolved = normalize_user_session(mode, search,
			next_profile, next_commands);
	if (resolved.profile_changed)
		next_profile = resolved.profile_position;
	if (resolved.commands_changed)
		next_commands = resolved.commands_position;
	return normalize_user_session(mode, search, next_profile, next_commands);
}

int main()
{
	// the documented behavior: docked, both profile and commands hidden → the row collapses,
	// regardless of the category arrangement.
	assert(user_session_row_visible(false, true, true, false) == false);
	assert(user_session_row_visible(false, true, true, true)  == false);

	// the documented behavior: unified-bar / full-screen always keeps the row (it carries the
	// shared search cluster), even when both clusters are hidden.
	assert(user_session_row_visible(true, true,  true,  false) == true);
	assert(user_session_row_visible(true, true,  true,  true)  == true);
	assert(user_session_row_visible(true, false, false, false) == true);

	// A visible profile keeps the docked row no matter what commands do.
	assert(user_session_row_visible(false, false, false, false) == true);
	assert(user_session_row_visible(false, false, true,  false) == true);
	assert(user_session_row_visible(false, false, true,  true)  == true);

	// the documented behavior: profile hidden but commands visible (docked) keeps the row —
	// and it no longer depends on category placement (the old bug suppressed it
	// when the category list sat at the bottom).
	assert(user_session_row_visible(false, true, false, false) == true);
	assert(user_session_row_visible(false, true, false, true)  == true);

	// ---------------------------------------------------------------------
	// normalize_user_session() contract (coupling-matrix §A/§C/§D).
	// Each row is "mode, search, profile_in, commands_in → profile_out,
	// commands_out, profile_changed, commands_changed".
	// ---------------------------------------------------------------------

	// Docked — passive load normalization rewrites the Profile aliases to
	// canonical left-anchored storage and snaps conflicting visible pairs to the
	// Profile row.
	check_vector(LayoutMode::Docked, nullptr, "top-left", "top-right",    "top-left",    "top-right",    false, false);
	check_vector(LayoutMode::Docked, nullptr, "top",      "bottom-right", "top-left",    "top-right",    true,  true);
	check_vector(LayoutMode::Docked, nullptr, "bottom",   "top-right",    "bottom-left", "bottom-right", true,  true);
	check_vector(LayoutMode::Docked, nullptr, "bottom-right", "top-right", "bottom-left", "bottom-right", true, true);
	check_vector(LayoutMode::Docked, nullptr, "hidden",   "bottom-right", "hidden",      "bottom-right", false, false);
	check_vector(LayoutMode::Docked, nullptr, "hidden",   "hidden",       "hidden",      "hidden",       false, false);

	// FullScreen — both edges follow the search-bar edge (§C/§D).
	check_vector(LayoutMode::FullScreen, "top",    "top-left",    "bottom-right", "top-left",    "top-right",    false, true);
	check_vector(LayoutMode::FullScreen, "bottom", "top-left",    "top-right",    "bottom-left", "bottom-right", true,  true);
	check_vector(LayoutMode::FullScreen, "bottom", "hidden",      "top-right",    "hidden",      "bottom-right", false, true);
	check_vector(LayoutMode::FullScreen, "top",    "hidden", "hidden",       "hidden", "hidden",       false, false);

	// Mask assertions.
	{
		// Docked, both visible on the top row ⇒ the opposite visible row stays
		// listed but dimmed in both combos; hidden always stays selectable.
		UserSessionResolution d = normalize_user_session(LayoutMode::Docked, nullptr,
				"top-left", "top-right");
		assert(d.commands_bottom_right_enabled == false);
		assert(d.commands_top_right_enabled == true);
		assert(d.profile_top_left_enabled == true);
		assert(d.profile_bottom_left_enabled == false);
		assert(d.profile_hidden_enabled && d.commands_hidden_enabled);
		assert(d.row_edge == UserSessionRowEdge::Top);
		assert(d.profile_visible && d.commands_visible);

		// Solo visibility frees the visible-row choices.
		UserSessionResolution solo = normalize_user_session(LayoutMode::Docked, nullptr,
				"hidden", "top-right");
		assert(solo.profile_top_left_enabled == true);
		assert(solo.profile_bottom_left_enabled == true);
		assert(solo.commands_top_right_enabled == true);
		assert(solo.commands_bottom_right_enabled == true);
		assert(solo.row_edge == UserSessionRowEdge::Top);
		assert(!solo.profile_visible && solo.commands_visible);

		UserSessionResolution profile_only = normalize_user_session(LayoutMode::Docked,
				nullptr, "bottom-left", "hidden");
		assert(profile_only.row_edge == UserSessionRowEdge::Bottom);
		assert(profile_only.profile_visible && !profile_only.commands_visible);

		UserSessionResolution both_hidden = normalize_user_session(LayoutMode::Docked,
				nullptr, "hidden", "hidden");
		assert(both_hidden.row_edge == UserSessionRowEdge::None);
		assert(!both_hidden.profile_visible && !both_hidden.commands_visible);

		// FullScreen search=top ⇒ both bottom edges greyed; hidden always on.
		UserSessionResolution f = normalize_user_session(LayoutMode::FullScreen, "top",
				"top-left", "top-right");
		assert(f.profile_bottom_left_enabled == false);
		assert(f.commands_bottom_right_enabled == false);
		assert(f.profile_hidden_enabled && f.commands_hidden_enabled);
	}

	// Idempotency: feeding a resolved pair back in yields no further change.
	{
		UserSessionResolution once = normalize_user_session(LayoutMode::FullScreen, "bottom",
				"top-left", "top-right");
		UserSessionResolution twice = normalize_user_session(LayoutMode::FullScreen, "bottom",
				once.profile_position, once.commands_position);
		assert(seq(twice.profile_position, once.profile_position));
		assert(seq(twice.commands_position, once.commands_position));
		assert(twice.profile_changed == false && twice.commands_changed == false);
	}

	// Live-edit semantics: visible partner follows the requested row in Docked,
	// hidden partner stays hidden, and FullScreen still defers to the search bar.
	{
		UserSessionResolution profile_hidden = simulate_live_edit(LayoutMode::Docked,
				nullptr, "top-left", "top-right", true, "hidden");
		assert(seq(profile_hidden.profile_position, "hidden"));
		assert(seq(profile_hidden.commands_position, "top-right"));

		UserSessionResolution commands_hidden = simulate_live_edit(LayoutMode::Docked,
				nullptr, "top-left", "top-right", false, "hidden");
		assert(seq(commands_hidden.profile_position, "top-left"));
		assert(seq(commands_hidden.commands_position, "hidden"));

		UserSessionResolution profile_then_commands = simulate_live_edit(LayoutMode::Docked,
				nullptr, profile_hidden.profile_position, profile_hidden.commands_position,
				false, "hidden");
		assert(seq(profile_then_commands.profile_position, "hidden"));
		assert(seq(profile_then_commands.commands_position, "hidden"));

		UserSessionResolution commands_then_profile = simulate_live_edit(LayoutMode::Docked,
				nullptr, commands_hidden.profile_position, commands_hidden.commands_position,
				true, "hidden");
		assert(seq(commands_then_profile.profile_position, "hidden"));
		assert(seq(commands_then_profile.commands_position, "hidden"));

		UserSessionResolution moved = simulate_live_edit(LayoutMode::Docked, nullptr,
				"top-left", "top-right", false, "bottom-right");
		assert(seq(moved.profile_position, "bottom-left"));
		assert(seq(moved.commands_position, "bottom-right"));

		UserSessionResolution profile_driven = simulate_live_edit(LayoutMode::Docked,
				nullptr, "top-left", "top-right", true, "bottom-left");
		assert(seq(profile_driven.profile_position, "bottom-left"));
		assert(seq(profile_driven.commands_position, "bottom-right"));

		UserSessionResolution session_driven_top = simulate_live_edit(LayoutMode::Docked,
				nullptr, "bottom-left", "bottom-right", false, "top-right");
		assert(seq(session_driven_top.profile_position, "top-left"));
		assert(seq(session_driven_top.commands_position, "top-right"));

		assert(moved.profile_top_left_enabled == false);
		assert(moved.profile_bottom_left_enabled == true);
		assert(moved.commands_top_right_enabled == false);
		assert(moved.commands_bottom_right_enabled == true);

		UserSessionResolution hidden_partner = simulate_live_edit(LayoutMode::Docked, nullptr,
				"top-left", "hidden", true, "bottom-left");
		assert(seq(hidden_partner.profile_position, "bottom-left"));
		assert(seq(hidden_partner.commands_position, "hidden"));

		UserSessionResolution hidden_profile_partner = simulate_live_edit(LayoutMode::Docked,
				nullptr, "hidden", "top-right", false, "bottom-right");
		assert(seq(hidden_profile_partner.profile_position, "hidden"));
		assert(seq(hidden_profile_partner.commands_position, "bottom-right"));
		assert(hidden_profile_partner.row_edge == UserSessionRowEdge::Bottom);

		UserSessionResolution restored = simulate_live_edit(LayoutMode::Docked, nullptr,
				"hidden", "hidden", true, "top-left");
		assert(seq(restored.profile_position, "top-left"));
		assert(seq(restored.commands_position, "hidden"));

		UserSessionResolution restored_commands = simulate_live_edit(LayoutMode::Docked,
				nullptr, "hidden", "hidden", false, "top-right");
		assert(seq(restored_commands.profile_position, "hidden"));
		assert(seq(restored_commands.commands_position, "top-right"));

		UserSessionResolution fullscreen = simulate_live_edit(LayoutMode::FullScreen, "bottom",
				"top-left", "top-right", false, "top-right");
		assert(seq(fullscreen.profile_position, "bottom-left"));
		assert(seq(fullscreen.commands_position, "bottom-right"));
	}

	return 0;
}
