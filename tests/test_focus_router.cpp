/* Headless coverage for physical directional scoring and eligibility. */

#include "core/window-keyboard.h"

#include <cstdio>
#include <cstdlib>

using namespace WhiskerMenu::Keyboard;

namespace
{

int failures = 0;

#define CHECK(condition) do { \
	if (!(condition)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		++failures; \
	} \
} while (0)

FocusTarget target(std::size_t id, NavigationRegion region, int x, int y,
		unsigned ordinal, bool usable = true,
		FocusTargetKind kind = FocusTargetKind::ResultItem)
{
	FocusTarget result;
	result.target_id = id;
	result.region = region;
	result.kind = kind;
	result.rectangle = {x, y, 10, 10};
	result.visual_ordinal = ordinal;
	result.usable = usable;
	return result;
}

void strict_half_plane()
{
	const NavigationRect origin{50, 50, 10, 10};
	std::vector<FocusTarget> candidates = {
		target(1, NavigationRegion::Results, 70, 50, 0),
		target(2, NavigationRegion::Results, 40, 50, 1),
		target(3, NavigationRegion::Results, 50, 30, 2),
	};
	CHECK(choose_spatial_target(origin, PhysicalDirection::Right,
			candidates, false) == 0);
	CHECK(choose_spatial_target(origin, PhysicalDirection::Left,
			candidates, false) == 1);
	CHECK(choose_spatial_target(origin, PhysicalDirection::Up,
			candidates, false) == 2);
	CHECK(choose_spatial_target(origin, PhysicalDirection::Down,
			candidates, false) == NO_TARGET);
}

void lexicographic_score()
{
	const NavigationRect origin{0, 0, 10, 10};
	std::vector<FocusTarget> candidates = {
		target(1, NavigationRegion::Results, 30, 30, 0),
		target(2, NavigationRegion::Results, 20, 80, 1),
		target(3, NavigationRegion::Results, 20, 45, 2),
	};
	CHECK(choose_spatial_target(origin, PhysicalDirection::Down,
			candidates, false) == 0);
}

void rtl_tie_order()
{
	const NavigationRect origin{0, 0, 10, 10};
	std::vector<FocusTarget> candidates = {
		target(1, NavigationRegion::Results, -20, 20, 0),
		target(2, NavigationRegion::Results, 20, 20, 1),
	};
	CHECK(choose_spatial_target(origin, PhysicalDirection::Down,
			candidates, false) == 0);
	CHECK(choose_spatial_target(origin, PhysicalDirection::Down,
			candidates, true) == 1);
}

void eligibility_and_no_wrap()
{
	const NavigationRect origin{0, 0, 10, 10};
	std::vector<FocusTarget> candidates = {
		target(1, NavigationRegion::Sidebar, 20, 0, 0, true),
		target(2, NavigationRegion::Results, 20, 0, 1, false),
		target(3, NavigationRegion::Results, -20, 0, 2, true),
		target(4, NavigationRegion::Results, 20, 0, 3, true,
			FocusTargetKind::Decorative),
	};
	CHECK(choose_spatial_target(origin, PhysicalDirection::Right,
			candidates, false, MenuState::Searching) == NO_TARGET);
	CHECK(choose_spatial_target(origin, PhysicalDirection::Left,
			candidates, false) == 2);
}

void searching_results_can_exit_horizontally_to_sidebar()
{
	CHECK(!allows_results_sidebar_exit(MenuState::Searching,
			NavigationRegion::Search, PhysicalDirection::Left));
	CHECK(!allows_results_sidebar_exit(MenuState::Searching,
			NavigationRegion::Results, PhysicalDirection::Up));
	CHECK(!allows_results_sidebar_exit(MenuState::Searching,
			NavigationRegion::Results, PhysicalDirection::Down));
	CHECK(allows_results_sidebar_exit(MenuState::Searching,
			NavigationRegion::Results, PhysicalDirection::Left));
	CHECK(allows_results_sidebar_exit(MenuState::Searching,
			NavigationRegion::Results, PhysicalDirection::Right));
	CHECK(allows_results_sidebar_exit(MenuState::Browsing,
			NavigationRegion::Results, PhysicalDirection::Up));
	const NavigationRect origin{50, 50, 10, 10};
	std::vector<FocusTarget> sidebar = {
		target(1, NavigationRegion::Sidebar, 20, 50, 0),
	};
	CHECK(choose_spatial_target(origin, PhysicalDirection::Left, sidebar,
			false, MenuState::Browsing) == 0);
}

void sidebar_internal_move_during_search()
{
	const NavigationRect origin{50, 50, 10, 10};
	std::vector<FocusTarget> categories = {
		target(1, NavigationRegion::Sidebar, 50, 20, 0, true,
			FocusTargetKind::CategoryButton),
		target(2, NavigationRegion::Sidebar, 50, 80, 1, true,
			FocusTargetKind::CategoryButton),
	};
	CHECK(choose_spatial_target(origin, PhysicalDirection::Up, categories,
			false, MenuState::Searching) == 0);
	CHECK(choose_spatial_target(origin, PhysicalDirection::Down, categories,
			false, MenuState::Searching) == 1);
}

void internal_first()
{
	const FocusTarget origin = target(10, NavigationRegion::Results,
			0, 0, 0);
	std::vector<FocusTarget> internal = {
		target(11, NavigationRegion::Results, 20, 0, 0),
	};
	std::vector<FocusTarget> external = {
		target(12, NavigationRegion::Sidebar, 12, 0, 0),
	};
	const NavigationDecision decision = decide_navigation(origin,
			PhysicalDirection::Right, internal, external, false);
	CHECK(decision.kind == NavigationDecisionKind::InternalMove);
	CHECK(decision.target_id == 11);
}

void no_target_is_noop()
{
	const FocusTarget origin = target(10, NavigationRegion::Search,
			0, 0, 0);
	const NavigationDecision decision = decide_navigation(origin,
			PhysicalDirection::Up, {}, {}, false);
	CHECK(decision.kind == NavigationDecisionKind::NoOp);
}

void key_normalization()
{
	PhysicalDirection direction = PhysicalDirection::Up;
	CHECK(normalize_direction(GDK_KEY_KP_Left, &direction));
	CHECK(direction == PhysicalDirection::Left);
	GdkEventKey event = {};
	event.keyval = GDK_KEY_Right;
	CHECK(is_directional_key(&event));
	event.state = GDK_SHIFT_MASK;
	CHECK(!is_directional_key(&event));
}

} // namespace

int main()
{
	strict_half_plane();
	lexicographic_score();
	rtl_tie_order();
	eligibility_and_no_wrap();
	searching_results_can_exit_horizontally_to_sidebar();
	sidebar_internal_move_during_search();
	internal_first();
	no_target_is_noop();
	key_normalization();
	if (failures != 0)
		return EXIT_FAILURE;
	std::printf("test_focus_router: ok\n");
	return EXIT_SUCCESS;
}
