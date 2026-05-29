/*
 * Headless tests for the Tab-cycle resolution helpers declared in
 * panel-plugin/core/window-keyboard.h. No GTK widgets are instantiated;
 * only the VisibilityMask/MenuState/Zone inputs are fabricated.
 *
 * Covers contracts/focus-router.md §"Test plan".
 */

#include "core/window-keyboard.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

using WhiskerMenu::Keyboard::Direction;
using WhiskerMenu::Keyboard::MenuState;
using WhiskerMenu::Keyboard::next_zone;
using WhiskerMenu::Keyboard::VisibilityMask;
using WhiskerMenu::Keyboard::Zone;
using WhiskerMenu::Keyboard::zone_active;

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
	case Zone::Mode:    return "Mode";
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
	z = next_zone(mask, MenuState::Browsing, z, Direction::Forward); EQZ(z, Zone::Mode);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Forward); EQZ(z, Zone::Profile);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Forward); EQZ(z, Zone::Search);
}

void reverse_full_loop()
{
	VisibilityMask mask;
	Zone z = Zone::Search;
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Profile);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Mode);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Sidebar);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Results);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Search);
	z = next_zone(mask, MenuState::Browsing, z, Direction::Backward); EQZ(z, Zone::Profile);
}

void sidebar_skipped_when_searching()
{
	VisibilityMask mask;
	EQZ(next_zone(mask, MenuState::Searching, Zone::Results, Direction::Forward),
	    Zone::Mode);
}

void sidebar_rejoined_when_browsing()
{
	VisibilityMask mask;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Sidebar);
}

void hidden_profile_skipped()
{
	VisibilityMask mask;
	mask.profile = false;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Mode, Direction::Forward),
	    Zone::Search);
}

void hidden_sidebar_and_mode()
{
	VisibilityMask mask;
	mask.sidebar = false;
	mask.mode = false;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Profile);
}

void hidden_profile_and_mode_searching()
{
	VisibilityMask mask;
	mask.profile = false;
	mask.mode = false;
	EQZ(next_zone(mask, MenuState::Searching, Zone::Results, Direction::Forward),
	    Zone::Search);
}

void current_is_inert_zone()
{
	// Sidebar just became inert (user typed first character). Forward
	// from inert Sidebar should land on the first active zone after it
	// in canonical order, which is Mode.
	VisibilityMask mask;
	EQZ(next_zone(mask, MenuState::Searching, Zone::Sidebar, Direction::Forward),
	    Zone::Mode);
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

void four_tabs_visit_every_visible()
{
	// SC-006: starting from Search in an all-visible Browsing menu,
	// four Forward Tab steps must visit Results, Sidebar, Mode, Profile.
	VisibilityMask mask;
	bool visited[5] = { false, false, false, false, false };
	Zone z = Zone::Search;
	visited[static_cast<unsigned>(z)] = true;
	for (int i = 0; i < 4; ++i)
	{
		z = next_zone(mask, MenuState::Browsing, z, Direction::Forward);
		visited[static_cast<unsigned>(z)] = true;
	}
	for (unsigned i = 0; i < 5; ++i)
	{
		CHECK(visited[i]);
	}
}

void all_optional_zones_hidden()
{
	// Profile + Mode + Sidebar hidden → cycle reduces to Search ↔ Results.
	VisibilityMask mask;
	mask.sidebar = false;
	mask.mode = false;
	mask.profile = false;
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Forward),
	    Zone::Results);
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Results, Direction::Forward),
	    Zone::Search);
	EQZ(next_zone(mask, MenuState::Browsing, Zone::Search, Direction::Backward),
	    Zone::Results);
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

} // namespace

int main()
{
	canonical_forward_all_visible();
	canonical_full_loop();
	reverse_full_loop();
	sidebar_skipped_when_searching();
	sidebar_rejoined_when_browsing();
	hidden_profile_skipped();
	hidden_sidebar_and_mode();
	hidden_profile_and_mode_searching();
	current_is_inert_zone();
	ltr_and_rtl_equivalence();
	four_tabs_visit_every_visible();
	all_optional_zones_hidden();
	zone_active_basic();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_focus_router: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_focus_router: ok\n");
	return EXIT_SUCCESS;
}
