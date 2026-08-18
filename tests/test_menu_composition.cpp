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
		const bool vertical_sidebar = sidebar == CompositionSidebar::Left
				|| sidebar == CompositionSidebar::Right;
		const MenuSurfaceRole main_role = mode == LayoutMode::FullScreen
				? MenuSurfaceRole::FullScreen : MenuSurfaceRole::Content;
		CHECK(out.baseline_surface == main_role);
		CHECK(out.primary_surface == main_role);
		CHECK(out.search_surface == main_role);
		CHECK(out.results_surface == main_role);
		CHECK(out.profile_surface == (!profile ? MenuSurfaceRole::None
				: (mode == LayoutMode::FullScreen
					? MenuSurfaceRole::FullScreen
					: (vertical_sidebar ? MenuSurfaceRole::Chrome
						: MenuSurfaceRole::Content))));
		CHECK(out.sidebar_surface == (!vertical_sidebar
				? MenuSurfaceRole::None
				: (mode == LayoutMode::FullScreen
					? MenuSurfaceRole::FullScreen : MenuSurfaceRole::Chrome)));
		CHECK(out.horizontal_sidebar_surface
				== (sidebar != CompositionSidebar::Horizontal
					? MenuSurfaceRole::None
					: (mode == LayoutMode::FullScreen
						? MenuSurfaceRole::FullScreen : MenuSurfaceRole::Chrome)));
		CHECK(out.secondary_surface == (out.secondary_visible
				? MenuSurfaceRole::Chrome : MenuSurfaceRole::None));
		if (mode != LayoutMode::FullScreen && places && !out.effective_session
				&& (vertical_sidebar || profile))
		{
			CHECK(out.apps_places_location
					== MenuControlLocation::SecondaryRow);
			CHECK(out.secondary_visible);
		}
		if (places && sidebar == CompositionSidebar::Horizontal)
		{
			CHECK(out.apps_places_location != MenuControlLocation::Sidebar);
		}

		if (mode == LayoutMode::FullScreen)
		{
			CHECK(!out.secondary_visible);
			CHECK(out.vertical_bands.front() == MenuBand::Primary);
			CHECK(out.horizontal_sidebar_edge == PrimaryEdge::Bottom);
			CHECK(out.search_column == MenuColumnRole::MiddleResults);
			CHECK(out.primary_surface == MenuSurfaceRole::FullScreen);
			CHECK(out.results_surface == MenuSurfaceRole::FullScreen);
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
			== MenuControlLocation::PrimaryRow);
}

void check_horizontal_matches_hidden_selector_home()
{
	for (LayoutMode mode : { LayoutMode::Docked, LayoutMode::Centered,
			LayoutMode::FullScreen })
	for (bool profile : { false, true })
	for (bool session : { false, true })
	for (unsigned int actions : { 0u, 2u })
	{
		MenuCompositionInput hidden = { mode, PrimaryEdge::Bottom,
				CompositionSidebar::Hidden, profile, session, actions, true,
				MenuDirection::LeftToRight };
		MenuCompositionInput horizontal = hidden;
		horizontal.sidebar = CompositionSidebar::Horizontal;
		MenuComposition hidden_out = meow_resolve_menu_composition(hidden);
		MenuComposition horizontal_out = meow_resolve_menu_composition(horizontal);

		CHECK(horizontal_out.apps_places_location
				== hidden_out.apps_places_location);
		CHECK(horizontal_out.secondary_slots == hidden_out.secondary_slots);
		CHECK(horizontal_out.primary_slots == hidden_out.primary_slots);
		if (mode == LayoutMode::FullScreen)
		{
			CHECK(horizontal_out.apps_places_location
					== MenuControlLocation::PrimaryRow);
			CHECK(contains(horizontal_out.primary_slots, MenuSlot::AppsPlaces));
		}
	}

	MenuCompositionInput transition = { LayoutMode::FullScreen,
			PrimaryEdge::Bottom, CompositionSidebar::Left, false, false, 0,
			true, MenuDirection::LeftToRight };
	for (CompositionSidebar sidebar : { CompositionSidebar::Left,
			CompositionSidebar::Right, CompositionSidebar::Horizontal,
			CompositionSidebar::Hidden, CompositionSidebar::Horizontal })
	{
		transition.sidebar = sidebar;
		const MenuComposition first = meow_resolve_menu_composition(transition);
		const MenuComposition repeated = meow_resolve_menu_composition(transition);
		CHECK(first.apps_places_location == repeated.apps_places_location);
		CHECK(first.primary_slots == repeated.primary_slots);
		CHECK(first.secondary_slots == repeated.secondary_slots);
		if (sidebar == CompositionSidebar::Horizontal)
			CHECK(first.apps_places_location == MenuControlLocation::PrimaryRow);
	}
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
	CHECK(out.apps_places_location == MenuControlLocation::PrimaryRow);
	CHECK(out.apps_places_column == MenuColumnRole::MiddleResults);
	CHECK((out.primary_slots == std::vector<MenuSlot>{ MenuSlot::AppsPlaces,
			MenuSlot::Search }));
	CHECK(out.horizontal_sidebar_edge == PrimaryEdge::Bottom);

	input.show_profile = true;
	input.show_session = true;
	input.direction = MenuDirection::RightToLeft;
	out = meow_resolve_menu_composition(input);
	CHECK((out.primary_slots == std::vector<MenuSlot>{ MenuSlot::Session,
			MenuSlot::Search, MenuSlot::AppsPlaces, MenuSlot::Profile }));
	CHECK(out.profile_column == MenuColumnRole::Outer);
}

void check_complete_snapshot_transitions()
{
	MenuLayoutSnapshotInput input = {
		{ LayoutMode::FullScreen, PrimaryEdge::Top,
			CompositionSidebar::Left, true, true, 3, true,
			MenuDirection::LeftToRight },
		true, true, 32, 22, 16
	};
	const MenuLayoutSnapshot baseline = meow_resolve_layout_snapshot(input);
	CHECK(meow_layout_snapshot_equal(baseline,
			meow_resolve_layout_snapshot(input)));

	auto differs = [&](const MenuLayoutSnapshotInput& changed)
	{
		CHECK(!meow_layout_snapshot_equal(baseline,
				meow_resolve_layout_snapshot(changed)));
	};
	MenuLayoutSnapshotInput changed = input;
	changed.composition.layout_mode = LayoutMode::Docked; differs(changed);
	changed = input; changed.composition.primary_edge = PrimaryEdge::Bottom; differs(changed);
	changed = input; changed.composition.sidebar = CompositionSidebar::Right; differs(changed);
	changed = input; changed.composition.show_profile = false; differs(changed);
	changed = input; changed.composition.show_session = false; differs(changed);
	changed = input; changed.composition.available_session_actions = 0; differs(changed);
	changed = input; changed.composition.places_enabled = false; differs(changed);
	changed = input; changed.composition.direction = MenuDirection::RightToLeft; differs(changed);
	changed = input; changed.category_names_visible = false; differs(changed);
	changed = input; changed.selector_icons_requested = false; differs(changed);
	changed = input; changed.category_icon_px = 48; differs(changed);
	changed = input; changed.search_icon_px = 24; differs(changed);
	changed = input; changed.session_icon_px = 20; differs(changed);
}

void check_windowed_chrome_reaches_outer_edges()
{
	MenuCompositionInput input = {
		LayoutMode::Docked, PrimaryEdge::Top, CompositionSidebar::Left,
		true, true, 3, true, MenuDirection::LeftToRight
	};
	const MenuComposition composition = meow_resolve_menu_composition(input);
	const MenuChromeGeometry geometry = meow_resolve_chrome_geometry(
			composition, 450, 500, 6,
			{ 12, 12, 132, 36, true },
			{ 12, 54, 132, 402, true },
			{ 0, 0, 0, 0, false },
			{ 12, 464, 426, 24, true });

	CHECK(geometry.vertical.visible);
	CHECK(geometry.vertical.x == 0);
	CHECK(geometry.vertical.y == 0);
	CHECK(geometry.vertical.width == 144);
	CHECK(geometry.vertical.height == 500);
	CHECK(geometry.band.visible);
	CHECK(geometry.band.x == 0);
	CHECK(geometry.band.y == 452);
	CHECK(geometry.band.width == 450);
	CHECK(geometry.band.height == 48);
	CHECK(geometry.separator.visible);
	CHECK(geometry.separator.x == 0);
	CHECK(geometry.separator.y == 452);
	CHECK(geometry.separator.width == 450);
	CHECK(geometry.separator.height == 1);
	CHECK(464 - geometry.separator.y == 500 - (464 + 24));

	input.sidebar = CompositionSidebar::Right;
	const MenuComposition right = meow_resolve_menu_composition(input);
	const MenuChromeGeometry mirrored = meow_resolve_chrome_geometry(
			right, 450, 500, 6,
			{ 306, 12, 132, 36, true },
			{ 306, 54, 132, 402, true },
			{ 0, 0, 0, 0, false },
			{ 12, 464, 426, 24, true });
	CHECK(mirrored.vertical.visible);
	CHECK(mirrored.vertical.x == 306);
	CHECK(mirrored.vertical.width == 144);
	CHECK(mirrored.vertical.height == 500);
}

void check_horizontal_chrome_reaches_its_outer_edge()
{
	MenuCompositionInput input = {
		LayoutMode::Centered, PrimaryEdge::Top,
		CompositionSidebar::Horizontal, true, true, 2, true,
		MenuDirection::LeftToRight
	};
	const MenuChromeGeometry bottom = meow_resolve_chrome_geometry(
			meow_resolve_menu_composition(input), 450, 500, 6,
			{ 0, 0, 0, 0, false },
			{ 0, 0, 0, 0, false },
			{ 12, 420, 426, 28, true },
			{ 12, 464, 426, 24, true });
	CHECK(!bottom.vertical.visible);
	CHECK(bottom.band.visible);
	CHECK(bottom.band.x == 0);
	CHECK(bottom.band.y == 408);
	CHECK(bottom.band.width == 450);
	CHECK(bottom.band.height == 92);
	CHECK(bottom.separator.visible);
	CHECK(bottom.separator.y == 408);
	CHECK(420 - bottom.separator.y == 500 - (464 + 24));

	input.primary_edge = PrimaryEdge::Bottom;
	const MenuChromeGeometry top = meow_resolve_chrome_geometry(
			meow_resolve_menu_composition(input), 450, 500, 6,
			{ 0, 0, 0, 0, false },
			{ 0, 0, 0, 0, false },
			{ 12, 52, 426, 28, true },
			{ 12, 12, 426, 24, true });
	CHECK(top.band.visible);
	CHECK(top.band.x == 0);
	CHECK(top.band.y == 0);
	CHECK(top.band.width == 450);
	CHECK(top.band.height == 92);
	CHECK(top.separator.visible);
	CHECK(top.separator.y == 91);
	CHECK(top.separator.y + 1 - (52 + 28) == 12);

	input.show_session = false;
	input.places_enabled = false;
	const MenuChromeGeometry strip_only = meow_resolve_chrome_geometry(
			meow_resolve_menu_composition(input), 450, 500, 6,
			{ 0, 0, 0, 0, false },
			{ 0, 0, 0, 0, false },
			{ 12, 52, 426, 28, true },
			{ 0, 0, 0, 0, false });
	CHECK(strip_only.band.visible);
	CHECK(!strip_only.separator.visible);
}

}

int main()
{
	check_cartesian_invariants();
	check_physical_mirroring();
	check_switch_relocation();
	check_horizontal_matches_hidden_selector_home();
	check_fullscreen_fixed_row();
	check_complete_snapshot_transitions();
	check_windowed_chrome_reaches_outer_edges();
	check_horizontal_chrome_reaches_its_outer_edge();
	return g_failures == 0 ? 0 : 1;
}
