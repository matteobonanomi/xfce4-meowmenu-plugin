#include "core/menu-composition.h"

#include <algorithm>
#include <cstdio>
#include <vector>

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

template <typename T>
bool contains(const std::vector<T>& values, T value)
{
	return std::find(values.begin(), values.end(), value) != values.end();
}

template <typename T>
unsigned int count(const std::vector<T>& values, T value)
{
	return static_cast<unsigned int>(std::count(values.begin(), values.end(), value));
}

void check_cartesian_invariants()
{
	for (LayoutMode mode : { LayoutMode::Docked, LayoutMode::Centered,
			LayoutMode::FullScreen })
	for (PrimaryEdge edge : { PrimaryEdge::Top, PrimaryEdge::Bottom })
	for (CompositionSidebar sidebar : { CompositionSidebar::Hidden,
			CompositionSidebar::Left, CompositionSidebar::Right,
			CompositionSidebar::Horizontal })
	for (bool profile : { false, true })
	for (bool session : { false, true })
	for (unsigned int actions : { 0u, 1u, 9u })
	for (bool places : { false, true })
	for (MenuDirection direction : { MenuDirection::LeftToRight,
			MenuDirection::RightToLeft })
	{
		MenuCompositionInput input = { mode, edge, sidebar, profile, session,
			actions, places, direction };
		MenuComposition out = meow_resolve_menu_composition(input);

		CHECK(out.effective_profile == profile);
		CHECK(out.effective_session == (session && actions > 0));
		CHECK(out.primary_edge == (mode == LayoutMode::FullScreen
				? PrimaryEdge::Top : edge));
		CHECK(count(out.vertical_bands, MenuBand::Primary) == 1);
		CHECK(count(out.vertical_bands, MenuBand::Results) == 1);
		CHECK(count(out.vertical_bands, MenuBand::Secondary)
				== (out.secondary_visible ? 1u : 0u));
		CHECK(count(out.vertical_bands, MenuBand::HorizontalSidebar)
				== (sidebar == CompositionSidebar::Horizontal ? 1u : 0u));
		CHECK(count(out.primary_slots, MenuSlot::Search) == 1);
		CHECK(count(out.primary_slots, MenuSlot::Profile) == (profile ? 1u : 0u));
		CHECK(count(out.primary_slots, MenuSlot::Session)
				== (mode == LayoutMode::FullScreen && out.effective_session ? 1u : 0u));
		CHECK((out.apps_places_location == MenuControlLocation::Hidden) == !places);
		CHECK(count(out.primary_slots, MenuSlot::AppsPlaces)
				== (out.apps_places_location == MenuControlLocation::PrimaryRow ? 1u : 0u));
		CHECK(count(out.secondary_slots, MenuSlot::AppsPlaces)
				== (out.apps_places_location == MenuControlLocation::SecondaryRow ? 1u : 0u));
		CHECK(count(out.secondary_slots, MenuSlot::Session)
				== (mode != LayoutMode::FullScreen && out.effective_session ? 1u : 0u));
		CHECK(out.secondary_visible == !out.secondary_slots.empty());
		if (mode != LayoutMode::FullScreen && places && !out.effective_session
				&& (sidebar != CompositionSidebar::Hidden || profile))
		{
			CHECK(out.apps_places_location
					== MenuControlLocation::SecondaryRow);
			CHECK(out.secondary_visible);
		}

		if (mode == LayoutMode::FullScreen)
		{
			CHECK(!out.secondary_visible);
			CHECK(out.vertical_bands.front() == MenuBand::Primary);
			CHECK(out.horizontal_sidebar_edge == PrimaryEdge::Bottom);
			CHECK(out.search_column == MenuColumnRole::MiddleResults);
		}
		else if (sidebar == CompositionSidebar::Hidden && out.effective_session)
		{
			CHECK(out.vertical_bands.back() == MenuBand::Secondary);
		}
		else
		{
			CHECK((edge == PrimaryEdge::Top
					&& out.vertical_bands.front() == MenuBand::Primary)
					|| (edge == PrimaryEdge::Bottom
						&& out.vertical_bands.back() == MenuBand::Primary));
		}

		CHECK(!contains(out.primary_slots, MenuSlot::Profile)
				|| out.profile_location == MenuControlLocation::PrimaryRow);
		CHECK(!contains(out.primary_slots, MenuSlot::Session)
				|| out.session_location == MenuControlLocation::PrimaryRow);
	}
}

void check_physical_mirroring()
{
	MenuCompositionInput input = { LayoutMode::Docked, PrimaryEdge::Top,
		CompositionSidebar::Hidden, false, false, 0, true,
		MenuDirection::LeftToRight };
	MenuComposition ltr = meow_resolve_menu_composition(input);
	input.direction = MenuDirection::RightToLeft;
	MenuComposition rtl = meow_resolve_menu_composition(input);
	CHECK((ltr.primary_slots == std::vector<MenuSlot>{ MenuSlot::Search,
			MenuSlot::AppsPlaces }));
	CHECK((rtl.primary_slots == std::vector<MenuSlot>{ MenuSlot::AppsPlaces,
			MenuSlot::Search }));

	input.sidebar = CompositionSidebar::Right;
	input.show_profile = true;
	input.places_enabled = false;
	ltr = meow_resolve_menu_composition(input);
	input.direction = MenuDirection::LeftToRight;
	rtl = meow_resolve_menu_composition(input);
	CHECK((ltr.primary_slots == std::vector<MenuSlot>{ MenuSlot::Search,
			MenuSlot::Profile }));
	CHECK(ltr.primary_slots == rtl.primary_slots);
}

void check_switch_relocation()
{
	MenuCompositionInput input = { LayoutMode::Docked, PrimaryEdge::Top,
			CompositionSidebar::Left, true, true, 1, true,
			MenuDirection::LeftToRight };
	MenuComposition out = meow_resolve_menu_composition(input);
	CHECK(out.apps_places_location
			== MenuControlLocation::SecondaryRow);
	CHECK(out.session_alignment == MenuAlignment::LogicalTrailing);
	input.available_session_actions = 0;
	out = meow_resolve_menu_composition(input);
	CHECK(out.apps_places_location == MenuControlLocation::SecondaryRow);
	CHECK(out.secondary_visible);
	CHECK((out.secondary_slots == std::vector<MenuSlot>{ MenuSlot::AppsPlaces }));
	input.sidebar = CompositionSidebar::Hidden;
	input.available_session_actions = 0;
	out = meow_resolve_menu_composition(input);
	CHECK(out.apps_places_location == MenuControlLocation::SecondaryRow);
	CHECK(out.secondary_visible);
	CHECK((out.secondary_slots == std::vector<MenuSlot>{ MenuSlot::AppsPlaces }));
	input.show_profile = false;
	out = meow_resolve_menu_composition(input);
	CHECK(out.apps_places_location == MenuControlLocation::PrimaryRow);
	CHECK(!out.secondary_visible);
	CHECK((out.primary_slots == std::vector<MenuSlot>{ MenuSlot::Search,
			MenuSlot::AppsPlaces }));
	input.available_session_actions = 1;
	out = meow_resolve_menu_composition(input);
	CHECK(out.apps_places_location
			== MenuControlLocation::SecondaryRow);
	CHECK(out.session_alignment == MenuAlignment::LogicalTrailing);
	input.direction = MenuDirection::RightToLeft;
	out = meow_resolve_menu_composition(input);
	CHECK(out.session_alignment == MenuAlignment::LogicalTrailing);
	input.layout_mode = LayoutMode::FullScreen;
	CHECK(meow_resolve_menu_composition(input).apps_places_location
			== MenuControlLocation::PrimaryRow);
	input.sidebar = CompositionSidebar::Horizontal;
	CHECK(meow_resolve_menu_composition(input).apps_places_location
			== MenuControlLocation::Sidebar);
}

void check_fullscreen_fixed_row()
{
	MenuCompositionInput input = { LayoutMode::FullScreen, PrimaryEdge::Bottom,
		CompositionSidebar::Hidden, true, true, 3, true,
		MenuDirection::LeftToRight };
	MenuComposition out = meow_resolve_menu_composition(input);
	CHECK(out.primary_edge == PrimaryEdge::Top);
	CHECK((out.vertical_bands == std::vector<MenuBand>{ MenuBand::Primary,
			MenuBand::Results }));
	CHECK((out.primary_slots == std::vector<MenuSlot>{ MenuSlot::Profile,
			MenuSlot::AppsPlaces, MenuSlot::Search, MenuSlot::Session }));
	CHECK(!out.secondary_visible);
	CHECK(out.session_location == MenuControlLocation::PrimaryRow);
	CHECK(out.profile_column == MenuColumnRole::Outer);
	CHECK(out.apps_places_column == MenuColumnRole::MiddleResults);
	CHECK(out.search_column == MenuColumnRole::MiddleResults);
	CHECK(out.search_location == MenuControlLocation::PrimaryRow);
	CHECK(out.search_alignment == MenuAlignment::Fill);
	CHECK(out.session_column == MenuColumnRole::Outer);
	CHECK(out.profile_alignment == MenuAlignment::LogicalLeading);
	CHECK(out.session_alignment == MenuAlignment::LogicalTrailing);

	input.show_profile = false;
	input.show_session = false;
	input.sidebar = CompositionSidebar::Right;
	out = meow_resolve_menu_composition(input);
	CHECK((out.primary_slots == std::vector<MenuSlot>{ MenuSlot::Search }));
	CHECK(out.profile_location == MenuControlLocation::Hidden);
	CHECK(out.session_location == MenuControlLocation::Hidden);
	CHECK(out.apps_places_location == MenuControlLocation::Sidebar);
	CHECK(out.apps_places_column == MenuColumnRole::Sidebar);
	CHECK(out.search_column == MenuColumnRole::MiddleResults);
	CHECK(out.search_location == MenuControlLocation::PrimaryRow);
	CHECK(out.search_alignment == MenuAlignment::Fill);
	CHECK(out.profile_column == MenuColumnRole::None);
	input.sidebar = CompositionSidebar::Horizontal;
	out = meow_resolve_menu_composition(input);
	CHECK(out.apps_places_location == MenuControlLocation::Sidebar);
	CHECK(out.apps_places_column == MenuColumnRole::Sidebar);
	CHECK(out.horizontal_sidebar_edge == PrimaryEdge::Bottom);

	input.show_profile = true;
	input.show_session = true;
	input.direction = MenuDirection::RightToLeft;
	out = meow_resolve_menu_composition(input);
	CHECK((out.primary_slots == std::vector<MenuSlot>{ MenuSlot::Session,
			MenuSlot::Search, MenuSlot::Profile }));
	CHECK(out.profile_column == MenuColumnRole::Outer);
}

}

int main()
{
	check_cartesian_invariants();
	check_physical_mirroring();
	check_switch_relocation();
	check_fullscreen_fixed_row();
	return g_failures == 0 ? 0 : 1;
}
