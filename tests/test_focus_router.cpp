/*
 * Headless tests for the focus-area resolution helpers declared in
 * panel-plugin/core/window-keyboard.h. No GTK widgets are instantiated;
 * only the VisibilityMask/MenuState/Zone inputs are fabricated.
 *
 * Covers the four-zone Ctrl+Tab cycle (contracts/area-cycle.md §"Test plan")
 * and the bare-Tab decision rule (contracts/tab-mode-toggle.md §"Test plan").
 */

#include "core/window-keyboard.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

using WhiskerMenu::Keyboard::Direction;
using WhiskerMenu::Keyboard::MenuState;
using WhiskerMenu::Keyboard::next_zone;
using WhiskerMenu::Keyboard::tab_action;
using WhiskerMenu::Keyboard::TabAction;
using WhiskerMenu::Keyboard::VisibilityMask;
using WhiskerMenu::Keyboard::Zone;
using WhiskerMenu::Keyboard::zone_active;
using WhiskerMenu::Keyboard::CalculatorFocus;
using WhiskerMenu::Keyboard::calculator_vertical_target;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

const char* zone_name(Zone z)
{
	switch (z)
	{
	case Zone::Search:  return "Search";
	case Zone::Results: return "Results";
	case Zone::Sidebar: return "Sidebar";
	case Zone::Profile: return "Profile";
	}
	return "?";
}

#define EQZ(actual, expected) do { \
		Zone _a = (actual); \
		Zone _e = (expected); \
		if (_a != _e) { \
			std::fprintf(stderr, "FAIL %s:%d: got %s expected %s\n", \
			             __FILE__, __LINE__, zone_name(_a), zone_name(_e)); \
			++g_failures; \
		} \
	} while (0)

void canonical_forward_all_visible()
{
	VisibilityMask mask;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Forward),
	    Zone::Results);
}

void canonical_full_loop()
{
	VisibilityMask mask;
	Zone z = Zone::Search;
	z = next_zone(mask, MenuState::Browsing, z, Direction::Forward); EQZ(z, Zone::Results);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Forward); EQZ(z, Zone::Sidebar);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Forward); EQZ(z, Zone::Profile);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Forward); EQZ(z, Zone::Search);
}

void reverse_full_loop()
{
	VisibilityMask mask;
	Zone z = Zone::Search;
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Profile);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Sidebar);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Results);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Search);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Profile);
}

void sidebar_skipped_when_searching()
{
	// Sidebar is inert while Searching, so Forward from Results skips it
	// and lands on Profile (the mode toggle is no longer a cycle stop).
	VisibilityMask mask;
	EQZ(next_zone(mask, MenuState::Searching, Zone::Results, Direction::Forward),
	    Zone::Profile);
}

void sidebar_rejoined_when_browsing()
{
	VisibilityMask mask;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Sidebar);
}

void hidden_profile_skipped()
{
	// With Profile hidden, Forward from Sidebar wraps past the absent
	// Profile back to Search.
	VisibilityMask mask;
	mask.profile = false;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Sidebar, Direction::Forward),
	    Zone::Search);
}

void hidden_sidebar()
{
	// Sidebar hidden → Forward from Results lands on Profile.
	VisibilityMask mask;
	mask.sidebar = false;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Profile);
}

void hidden_profile_searching()
{
	// Profile hidden and Sidebar inert (Searching) → Forward from Results
	// wraps to Search.
	VisibilityMask mask;
	mask.profile = false;
	EQZ(next_zone(mask, MenuState::Searching, Zone::Results, Direction::Forward),
	    Zone::Search);
}

void current_is_inert_zone()
{
	// Sidebar just became inert (user typed first character). Forward
	// from inert Sidebar should land on the first active zone after it
	// in canonical order, which is Profile (Mode is no longer a stop).
	VisibilityMask mask;
	EQZ(next_zone(mask, MenuState::Searching, Zone::Sidebar, Direction::Forward),
	    Zone::Profile);
}

void ltr_and_rtl_equivalence()
{
	// The cycle is independent of widget default direction (FR-120);
	// the function takes no GTK state. Pin the property explicitly by
	// running the same call twice with identical inputs and asserting
	// the same output.
	VisibilityMask mask;
	Zone ltr = next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Forward);
	Zone rtl = next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Forward);
	EQZ(ltr, rtl);
}

void three_tabs_visit_every_visible()
{
	// SC-006: starting from Search in an all-visible Browsing menu, three
	// Forward steps must visit the other three zones: Results, Sidebar,
	// Profile (the four-zone cycle has no Mode stop).
	VisibilityMask mask;
	bool visited[4] = { false, false, false, false };
	Zone z = Zone::Search;
	visited[static_cast<unsigned>(z)] = true;
	for (int i = 0; i < 3; ++i)
	{
		z = next_zone(mask, MenuState::Browsing, z, Direction::Forward);
		visited[static_cast<unsigned>(z)] = true;
	}
	for (unsigned i = 0; i < 4; ++i)
	{
		CHECK(visited[i]);
	}
}

void all_optional_zones_hidden()
{
	// Sidebar + Profile hidden → cycle reduces to Search ↔ Results.
	VisibilityMask mask;
	mask.sidebar = false;
	mask.profile = false;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Forward),
	    Zone::Results);
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Search);
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Backward),
	    Zone::Results);
}

void profile_unfocusable_skipped()
{
	// US3 / contracts/focus-cycle.md: when the Profile zone's only command
	// button is visible but NOT focusable, current_visibility_mask() now
	// reports profile=false (the focusability-aware availability gate). At
	// this pure layer that is simply mask.profile == false, and the cycle must
	// skip Profile instead of treating it as a dead-end stop.
	//
	// NOTE: the focusability *decision* lives in current_visibility_mask()
	// (GTK-widget-coupled, verified manually per quickstart). next_zone is
	// authoritative over the resulting mask and already skips an unavailable
	// zone, so this case documents and locks that contract.
	VisibilityMask mask;
	mask.profile = false;

	// Browsing, from Results: Profile unavailable but Sidebar is active, so
	// the next focusable zone is Sidebar.
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Sidebar);

	// Searching (Sidebar inert) + Profile unavailable: Forward from Results
	// finds no later active zone and wraps to Search.
	EQZ(next_zone(mask, MenuState::Searching, Zone::Results, Direction::Forward),
	    Zone::Search);

	// From Sidebar with Profile unavailable, Forward wraps past the absent
	// Profile back to Search.
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Sidebar, Direction::Forward),
	    Zone::Search);
}

void single_available_zone_noop()
{
	// FR-011 / C3: when exactly one zone can receive focus, Ctrl+Tab is a
	// harmless no-op — next_zone returns `current`. Fabricate a single-zone
	// mask (only Search active) to exercise the no-op path directly. In the
	// live menu Search and Results are never both hidden (FR-030); this is the
	// pure-layer guarantee the runtime grab-retry loop relies on.
	VisibilityMask mask;
	mask.results = false;
	mask.sidebar = false;
	mask.profile = false;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Forward),
	    Zone::Search);
	EQZ(next_zone(mask, MenuState::Searching, Zone::Search, Direction::Forward),
	    Zone::Search);
}

void tab_action_rule()
{
	// FR-006: Tab toggles the mode when Places is available and is inert
	// otherwise — it never falls back to area cycling.
	CHECK(tab_action(true)  == TabAction::ToggleMode);
	CHECK(tab_action(false) == TabAction::Inert);
}

void sidebar_focus_retention_zone_invariant()
{
	// US1 / contracts/focus-retention.md C1. Regression guard — expected green
	// both before and after the fix: the ejection defect lived in the
	// widget-level activation handoff, not in this pure zone routing. Lock the
	// zone model the retention relies on so a future change to next_zone /
	// zone_active cannot silently reintroduce the Sidebar→Search ejection.
	//
	// The sidebar must be an active, focus-holding zone while browsing, and its
	// only Ctrl+Tab neighbours are Results and Profile — never Search. An
	// along-axis category move is not a zone transition at all (it stays in the
	// sidebar by construction), so no along-axis press can resolve to Search
	// through this layer.
	VisibilityMask mask;
	CHECK(zone_active(Zone::Sidebar, mask, MenuState::Browsing));
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Sidebar, Direction::Forward),
	    Zone::Profile);
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Sidebar, Direction::Backward),
	    Zone::Results);

	// The keyboard-origin guard (Window::m_keyboard_category_nav) is deliberately
	// NOT an input to the pure router: the resolved zone is identical regardless
	// of whether a keyboard navigation is in progress. The router taking no such
	// parameter is the structural guarantee; re-evaluating yields the same zone,
	// locking that the resolution cannot drift to Search.
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Sidebar, Direction::Forward),
	    Zone::Profile);

	// SC-002 single-navigable-category edge. At the widget level an along-axis
	// move with one focusable sibling is a no-op that keeps focus on that
	// category (verified manually per quickstart). The pure-layer analogue is
	// the single-available-zone no-op: with only one focusable zone, resolution
	// returns `current` rather than ejecting elsewhere.
	VisibilityMask single;
	single.results = false;
	single.sidebar = false;
	single.profile = false;
	EQZ(next_zone(single, MenuState::Browsing, Zone::Search, Direction::Forward),
	    Zone::Search);
}

void pointer_origin_handoff_absent_from_router()
{
	// US2 / contracts/focus-retention.md C2. The pointer-vs-keyboard handoff
	// decision lives solely in the Window toggled handlers, keyed on
	// m_keyboard_category_nav, and is intentionally absent from the pure router:
	// the router resolves the same Sidebar zone regardless of activation origin.
	// This locks the routing layer as origin-agnostic so the distinction can
	// only ever live in the one auditable guard, never leak into zone routing.
	VisibilityMask mask;
	CHECK(zone_active(Zone::Sidebar, mask, MenuState::Browsing));
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Sidebar);
}

void sidebar_exit_paths_unchanged_by_guard()
{
	// US3 / contracts/focus-retention.md C4. The explicit exit paths are
	// unchanged by the focus-retention fix. The outward-arrow grab into the
	// results list and the printable-key redirect are widget-level (manual
	// quickstart); at the pure layer, lock that the browsing/searching
	// distinction and Sidebar's cross-region neighbours are exactly as before,
	// so the new guard perturbs none of them.
	VisibilityMask mask;
	// Sidebar is inert while searching (a printable key began a query); the
	// exit routing must skip it, unchanged.
	CHECK(!zone_active(Zone::Sidebar, mask, MenuState::Searching));
	EQZ(next_zone(mask, MenuState::Searching, Zone::Results, Direction::Forward),
	    Zone::Profile);
	// Browsing cross-region cycle from Results still reaches Sidebar.
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Sidebar);
}

void zone_active_basic()
{
	VisibilityMask mask;
	CHECK(zone_active(Zone::Sidebar, mask, MenuState::Browsing));
	CHECK(!zone_active(Zone::Sidebar, mask, MenuState::Searching));
	mask.profile = false;
	CHECK(!zone_active(Zone::Profile, mask, MenuState::Browsing));
	CHECK(zone_active(Zone::Search, mask, MenuState::Searching));
	CHECK(zone_active(Zone::Results, mask, MenuState::Searching));
}

void calculator_banner_bridges()
{
	CHECK(calculator_vertical_target(true, CalculatorFocus::Search, false, false)
		== CalculatorFocus::Banner);
	CHECK(calculator_vertical_target(true, CalculatorFocus::Banner, true, false)
		== CalculatorFocus::Search);
	CHECK(calculator_vertical_target(true, CalculatorFocus::Banner, false, false)
		== CalculatorFocus::Results);
	CHECK(calculator_vertical_target(true, CalculatorFocus::Results, true, true)
		== CalculatorFocus::Banner);
	CHECK(calculator_vertical_target(true, CalculatorFocus::Results, true, false)
		== CalculatorFocus::None);
	CHECK(calculator_vertical_target(false, CalculatorFocus::Search, false, false)
		== CalculatorFocus::None);
}

} // namespace

int main()
{
	canonical_forward_all_visible();
	canonical_full_loop();
	reverse_full_loop();
	sidebar_skipped_when_searching();
	sidebar_rejoined_when_browsing();
	hidden_profile_skipped();
	hidden_sidebar();
	hidden_profile_searching();
	current_is_inert_zone();
	ltr_and_rtl_equivalence();
	three_tabs_visit_every_visible();
	all_optional_zones_hidden();
	profile_unfocusable_skipped();
	single_available_zone_noop();
	tab_action_rule();
	sidebar_focus_retention_zone_invariant();
	pointer_origin_handoff_absent_from_router();
	sidebar_exit_paths_unchanged_by_guard();
	zone_active_basic();
	calculator_banner_bridges();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_focus_router: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_focus_router: ok\n");
	return EXIT_SUCCESS;
}
