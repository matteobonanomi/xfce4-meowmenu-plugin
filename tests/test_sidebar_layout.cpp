/*
 * Headless, table-driven tests for the pure layout-decision mapping declared in
 * panel-plugin/core/sidebar-layout.h. No GTK types are used.
 *
 * Asserts every row of the layout-decision contract (ui-contract.md §3) plus
 * the "forcing removed ⇒ effective reverts to stored intent" case (FR-029).
 */

#include "core/sidebar-layout.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace WhiskerMenu;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

SidebarLayoutState make_state(bool sidebar_enabled, SidebarPosition pos,
		bool names, bool icons, bool places)
{
	SidebarLayoutState s;
	s.sidebar_enabled = sidebar_enabled;
	s.position = pos;
	s.category_show_name = names;
	s.switch_show_icons = icons;
	s.search_bar_bottom = false;
	s.fullscreen = false;
	s.places_enabled = places;
	return s;
}

// Row 1: ON, left/right, names on, places on → in sidebar, horizontal,
// icons stored, names on, no heading.
void row1_vertical_names_on()
{
	auto p = meow_compute_sidebar_layout(
			make_state(true, SidebarPosition::Left, true, false, true));
	CHECK(p.sidebar_visible);
	CHECK(!p.categories_horizontal);
	CHECK(p.switch_location == SwitchLocation::InSidebar);
	CHECK(p.switch_orientation == SwitchOrientation::Horizontal);
	CHECK(p.effective_show_icons == false);          // stored intent
	CHECK(p.effective_show_category_names == true);
	CHECK(!p.show_default_category_heading);

	// Stored switch_show_icons=true must pass through unchanged on a vertical sidebar.
	auto p2 = meow_compute_sidebar_layout(
			make_state(true, SidebarPosition::Right, true, true, true));
	CHECK(p2.effective_show_icons == true);
}

// Row 2: ON, left/right, names off, places on → vertical switch, names off.
void row2_vertical_names_off()
{
	auto p = meow_compute_sidebar_layout(
			make_state(true, SidebarPosition::Left, false, false, true));
	CHECK(p.sidebar_visible);
	CHECK(!p.categories_horizontal);
	CHECK(p.switch_location == SwitchLocation::InSidebar);
	CHECK(p.switch_orientation == SwitchOrientation::Vertical);
	CHECK(p.effective_show_category_names == false);
}

// Row 3: ON, top/bottom, any, places on → horizontal strip, icons forced ON,
// names forced off, switch horizontal in sidebar.
void row3_strip()
{
	for (SidebarPosition pos : { SidebarPosition::Top, SidebarPosition::Bottom })
	{
		for (bool names : { true, false })
		{
			auto p = meow_compute_sidebar_layout(
					make_state(true, pos, names, false, true));
			CHECK(p.sidebar_visible);
			CHECK(p.categories_horizontal);
			CHECK(p.switch_location == SwitchLocation::InSidebar);
			CHECK(p.switch_orientation == SwitchOrientation::Horizontal);
			CHECK(p.effective_show_icons == true);            // forced
			CHECK(p.effective_show_category_names == false);  // forced
			CHECK(!p.show_default_category_heading);
		}
	}
}

// Row 4: ON, any, any, places off → no switch, no heading; names per rules.
void row4_places_off()
{
	auto p = meow_compute_sidebar_layout(
			make_state(true, SidebarPosition::Left, true, false, false));
	CHECK(p.sidebar_visible);
	CHECK(p.switch_location == SwitchLocation::None);
	CHECK(!p.show_default_category_heading);
	CHECK(p.effective_show_category_names == true);

	// top/bottom with places off still forces icon-only categories.
	auto p2 = meow_compute_sidebar_layout(
			make_state(true, SidebarPosition::Top, true, false, false));
	CHECK(p2.categories_horizontal);
	CHECK(p2.switch_location == SwitchLocation::None);
	CHECK(p2.effective_show_category_names == false);
}

// Row 5: OFF, places on → switch in search bar (horizontal, icons forced),
// heading shown.
void row5_disabled_places_on()
{
	auto p = meow_compute_sidebar_layout(
			make_state(false, SidebarPosition::Left, true, false, true));
	CHECK(!p.sidebar_visible);
	CHECK(p.switch_location == SwitchLocation::InSearchBar);
	CHECK(p.switch_orientation == SwitchOrientation::Horizontal);
	CHECK(p.effective_show_icons == true);   // forced
	CHECK(p.show_default_category_heading);
}

// Row 6: OFF, places off → no switch, heading shown.
void row6_disabled_places_off()
{
	auto p = meow_compute_sidebar_layout(
			make_state(false, SidebarPosition::Left, true, false, false));
	CHECK(!p.sidebar_visible);
	CHECK(p.switch_location == SwitchLocation::None);
	CHECK(p.show_default_category_heading);
}

// FR-029: a forced state never overwrites stored intent, so once the forcing
// layout is removed the effective value reverts to the stored value with no
// bookkeeping. Stored switch_show_icons=false + stored names=true:
//   top (forced icons ON, names off) → left (icons OFF, names ON).
void fr029_reversion()
{
	const bool stored_icons = false;
	const bool stored_names = true;

	auto forced = meow_compute_sidebar_layout(
			make_state(true, SidebarPosition::Top, stored_names, stored_icons, true));
	CHECK(forced.effective_show_icons == true);
	CHECK(forced.effective_show_category_names == false);

	auto reverted = meow_compute_sidebar_layout(
			make_state(true, SidebarPosition::Left, stored_names, stored_icons, true));
	CHECK(reverted.effective_show_icons == stored_icons);
	CHECK(reverted.effective_show_category_names == stored_names);
}

// "hidden" and unknown strings parse to Left; the four real values map through.
void parse_positions()
{
	CHECK(meow_parse_sidebar_position("left") == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position("right") == SidebarPosition::Right);
	CHECK(meow_parse_sidebar_position("top") == SidebarPosition::Top);
	CHECK(meow_parse_sidebar_position("bottom") == SidebarPosition::Bottom);
	CHECK(meow_parse_sidebar_position("hidden") == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position(nullptr) == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position("nonsense") == SidebarPosition::Left);
}

} // namespace

int main()
{
	row1_vertical_names_on();
	row2_vertical_names_off();
	row3_strip();
	row4_places_off();
	row5_disabled_places_on();
	row6_disabled_places_off();
	fr029_reversion();
	parse_positions();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_sidebar_layout: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_sidebar_layout: ok\n");
	return EXIT_SUCCESS;
}
