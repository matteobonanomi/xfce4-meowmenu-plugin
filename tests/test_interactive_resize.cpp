/*
 * Display-free checks for interactive resize geometry and delivery state.
 */

#include "core/interactive-resize.h"

#include <climits>
#include <cstdio>
#include <cstdlib>

using namespace WhiskerMenu::InteractiveResize;

namespace
{

int g_failures = 0;

#define CHECK(condition) do { \
		if (!(condition)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", \
					__FILE__, __LINE__, #condition); \
			++g_failures; \
		} \
	} while (0)

PointerSample point(std::int64_t x, std::int64_t y)
{
	return {x, y, 0, CoordinateSpace::Screen};
}

bool same_rectangle(const Rectangle& left, const Rectangle& right)
{
	return left.x == right.x
			&& left.y == right.y
			&& left.width == right.width
			&& left.height == right.height;
}

ReducerState docked(Direction direction,
		const Rectangle& rectangle = {100, 200, 300, 240})
{
	return {
		direction,
		rectangle,
		point(500, 500),
		{{10, 2000}, {10, 2000}},
		opposite_edge_anchor(rectangle),
		rectangle
	};
}

Rectangle candidate(const ReducerState& state,
		const PointerSample& sample,
		bool* valid = nullptr)
{
	Rectangle rectangle = {};
	const bool accepted = calculate_candidate(state, sample, &rectangle);
	if (valid)
		*valid = accepted;
	return rectangle;
}

DisplaySignature display_signature()
{
	return {
		17,
		{0, 0, 1920, 1080},
		{0, 24, 1920, 1056},
		1,
		23
	};
}

Transaction transaction(BackendPolicy policy = BackendPolicy::X11Live,
		bool normal_presentation = true)
{
	Transaction result;
	const Rectangle rectangle = {100, 200, 300, 240};
	CHECK(result.begin(
			Direction::BottomRight,
			policy,
			rectangle,
			point(500, 500),
			{{100, 1000}, {100, 900}},
			opposite_edge_anchor(rectangle),
			{300, 240},
			display_signature(),
			normal_presentation));
	return result;
}

void direction_mapping()
{
	const Direction directions[] = {
		Direction::TopLeft, Direction::Top, Direction::TopRight,
		Direction::Left, Direction::Right,
		Direction::BottomLeft, Direction::Bottom, Direction::BottomRight
	};
	const int horizontal[] = {-1, 0, 1, -1, 1, -1, 0, 1};
	const int vertical[] = {-1, -1, -1, 0, 0, 1, 1, 1};
	for (unsigned int i = 0; i < 8; ++i)
	{
		const DirectionAxes axes = direction_axes(directions[i]);
		CHECK(axes.horizontal == horizontal[i]);
		CHECK(axes.vertical == vertical[i]);
	}
}

void all_edges_keep_opposite_anchor()
{
	const Direction directions[] = {
		Direction::TopLeft, Direction::Top, Direction::TopRight,
		Direction::Left, Direction::Right,
		Direction::BottomLeft, Direction::Bottom, Direction::BottomRight
	};
	for (Direction direction : directions)
	{
		const ReducerState state = docked(direction);
		const DirectionAxes axes = direction_axes(direction);
		bool valid = false;
		const Rectangle result = candidate(state,
				point(500 + (axes.horizontal * 25),
					500 + (axes.vertical * 30)), &valid);

		CHECK(valid);
		CHECK(result.width == 300 + (axes.horizontal ? 25 : 0));
		CHECK(result.height == 240 + (axes.vertical ? 30 : 0));
		if (axes.horizontal < 0)
			CHECK(result.x + result.width == 400);
		else
			CHECK(result.x == 100);
		if (axes.vertical < 0)
			CHECK(result.y + result.height == 440);
		else
			CHECK(result.y == 200);
	}
}

void every_sample_is_press_relative()
{
	const ReducerState state = docked(Direction::BottomRight);
	const Rectangle direct = candidate(state, point(580, 440));
	(void)candidate(state, point(520, 490));
	(void)candidate(state, point(545, 470));
	const Rectangle after_intermediates = candidate(state, point(580, 440));
	CHECK(same_rectangle(direct, after_intermediates));
	CHECK(direct.width == 380);
	CHECK(direct.height == 180);
}

void invalid_edge_request_retains_last_displayed()
{
	ReducerState state = docked(Direction::Right);
	state.bounds.width = {200, 320};
	state.last_displayed = {100, 200, 315, 240};
	bool valid = true;
	const Rectangle invalid = candidate(state, point(600, 500), &valid);
	CHECK(!valid);
	CHECK(same_rectangle(invalid, state.last_displayed));

	const Rectangle reversed = candidate(state, point(519, 500), &valid);
	CHECK(valid);
	CHECK(reversed.width == 319);
	CHECK(reversed.x == 100);
}

void invalid_corner_request_rejects_both_axes()
{
	ReducerState state = docked(Direction::BottomRight);
	state.bounds.width = {100, 310};
	state.bounds.height = {100, 500};
	state.last_displayed = {100, 200, 305, 270};
	bool valid = true;
	const Rectangle invalid = candidate(state, point(700, 560), &valid);
	CHECK(!valid);
	CHECK(same_rectangle(invalid, state.last_displayed));

	const Rectangle accepted = candidate(state, point(508, 560), &valid);
	CHECK(valid);
	CHECK(accepted.width == 308);
	CHECK(accepted.height == 300);
}

void centered_geometry_is_symmetric()
{
	const Rectangle monitor = {-1920, -200, 1920, 1080};
	ReducerState state = {
		Direction::TopRight,
		{-1360, 40, 800, 600},
		point(0, 0),
		{{100, 1920}, {100, 1080}},
		monitor_center_anchor(monitor),
		{-1360, 40, 800, 600}
	};

	bool valid = false;
	const Rectangle result = candidate(state, point(25, -10), &valid);
	CHECK(valid);
	CHECK(result.width == 850);
	CHECK(result.height == 620);
	CHECK(result.x == -1385);
	CHECK(result.y == 30);
	CHECK((result.x * 2) + result.width == -1920);
	CHECK((result.y * 2) + result.height == 680);

	state.bounds.width.maximum = 840;
	state.last_displayed = result;
	const Rectangle invalid = candidate(state, point(30, -15), &valid);
	CHECK(!valid);
	CHECK(same_rectangle(invalid, result));
}

void wide_intermediates_are_rejected_safely()
{
	ReducerState state = docked(Direction::BottomRight,
			{INT_MIN, INT_MIN, 100, 100});
	state.press_pointer = point(INT64_MIN, INT64_MIN);
	state.bounds = {{1, INT_MAX}, {1, INT_MAX}};
	bool valid = true;
	const Rectangle result = candidate(
			state, point(INT64_MAX, INT64_MAX), &valid);
	CHECK(!valid);
	CHECK(same_rectangle(result, state.last_displayed));

	state.press_pointer = point(INT64_MAX, INT64_MAX);
	const Rectangle other_direction = candidate(
			state, point(INT64_MIN, INT64_MIN), &valid);
	CHECK(!valid);
	CHECK(same_rectangle(other_direction, state.last_displayed));
}

void coordinate_spaces_do_not_mix()
{
	const ReducerState state = docked(Direction::Right);
	const PointerSample local = {900, 500, 0, CoordinateSpace::Handle};
	bool valid = true;
	const Rectangle result = candidate(state, local, &valid);
	CHECK(!valid);
	CHECK(same_rectangle(result, state.last_displayed));
}

void motion_coalesces_to_one_latest_frame()
{
	Transaction state;
	const Rectangle initial = {100, 200, 300, 240};
	CHECK(state.begin(
			Direction::BottomRight,
			BackendPolicy::X11Live,
			initial,
			point(500, 500),
			{{100, 10000}, {100, 10000}},
			opposite_edge_anchor(initial),
			{300, 240},
			display_signature(),
			true));

	Rectangle rectangle = {};
	int requested_frames = 0;
	for (int i = 1; i <= 5000; ++i)
	{
		if (!state.frame_pending())
			++requested_frames;
		CHECK(state.motion(point(500 + i, 500 + i), &rectangle));
	}
	CHECK(requested_frames == 1);
	CHECK(state.frame_pending());
	CHECK(state.has_pending_geometry());
	CHECK(rectangle.width == 5300 && rectangle.height == 5240);
	CHECK(state.rectangle().width == 300 && state.rectangle().height == 240);

	CHECK(state.take_pending(&rectangle));
	CHECK(!state.frame_pending());
	CHECK(!state.has_pending_geometry());
	CHECK(rectangle.width == 5300 && rectangle.height == 5240);
	state.mark_displayed(rectangle);
	CHECK(state.rectangle().width == 5300);
	CHECK(!state.take_pending(&rectangle));
}

void invalid_latest_motion_discards_older_pending_geometry()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	CHECK(state.motion(point(600, 600), &rectangle));
	CHECK(state.frame_pending());
	CHECK(state.has_pending_geometry());
	CHECK(state.motion(point(1500, 600), &rectangle));
	CHECK(same_rectangle(rectangle, state.pre_drag_rectangle()));
	CHECK(state.frame_pending());
	CHECK(!state.has_pending_geometry());
	CHECK(!state.take_pending(&rectangle));
	CHECK(!state.frame_pending());
}

void displayed_geometry_advances_only_after_acknowledgement()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	CHECK(state.motion(point(560, 540), &rectangle));
	CHECK(rectangle.width == 360 && rectangle.height == 280);
	CHECK(state.rectangle().width == 300 && state.rectangle().height == 240);
	CHECK(state.take_pending(&rectangle));
	CHECK(state.rectangle().width == 300 && state.rectangle().height == 240);
	state.mark_displayed(rectangle);
	CHECK(state.rectangle().width == 360 && state.rectangle().height == 280);
}

void valid_release_is_synchronous_and_idempotent()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(520, 530), &rectangle));
	CHECK(state.frame_pending());
	CHECK(state.complete(point(545, 565), &rectangle, &saved));
	CHECK(!state.frame_pending());
	CHECK(!state.has_pending_geometry());
	CHECK(state.completion_valid());
	CHECK(rectangle.width == 345 && rectangle.height == 305);
	CHECK(saved.width == 345 && saved.height == 305);
	CHECK(state.lifecycle() == Lifecycle::Completed);

	CHECK(state.complete(point(900, 900), &rectangle, &saved));
	CHECK(rectangle.width == 345 && rectangle.height == 305);
	CHECK(!state.motion(point(550, 570), &rectangle));
	CHECK(!state.cancel(&rectangle, &saved));
}

void invalid_release_retains_displayed_geometry()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(560, 540), &rectangle));
	CHECK(state.take_pending(&rectangle));
	state.mark_displayed(rectangle);
	const Rectangle displayed = rectangle;

	CHECK(state.complete(point(1500, 1500), &rectangle, &saved));
	CHECK(!state.completion_valid());
	CHECK(same_rectangle(rectangle, displayed));
	CHECK(saved.width == 300 && saved.height == 240);
	CHECK(!state.frame_pending());
}

void cancellation_restores_exact_geometry()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(650, 700), &rectangle));
	CHECK(state.take_pending(&rectangle));
	state.mark_displayed(rectangle);
	CHECK(state.cancel(&rectangle, &saved));
	CHECK(same_rectangle(rectangle, state.pre_drag_rectangle()));
	CHECK(saved.width == 300 && saved.height == 240);
	CHECK(!state.frame_pending());
	CHECK(!state.has_pending_geometry());
	CHECK(state.cancel(&rectangle, &saved));
	CHECK(same_rectangle(rectangle, state.pre_drag_rectangle()));
	CHECK(!state.motion(point(800, 800), &rectangle));
	CHECK(!state.complete(point(800, 800), &rectangle, &saved));
}

void fullscreen_completion_does_not_persist()
{
	Transaction state = transaction(BackendPolicy::X11Live, false);
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.complete(point(700, 700), &rectangle, &saved));
	CHECK(rectangle.width == 500 && rectangle.height == 440);
	CHECK(saved.width == 300 && saved.height == 240);
}

void wayland_applies_only_at_release()
{
	Transaction state = transaction(BackendPolicy::WaylandReleaseToApply);
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(600, 600), &rectangle));
	CHECK(rectangle.width == 300 && rectangle.height == 240);
	CHECK(!state.frame_pending());
	CHECK(state.complete(point(620, 650), &rectangle, &saved));
	CHECK(state.completion_valid());
	CHECK(rectangle.width == 420 && rectangle.height == 390);
	CHECK(saved.width == 420 && saved.height == 390);
}

void wayland_invalid_release_retains_press_geometry()
{
	Transaction state = transaction(BackendPolicy::WaylandReleaseToApply);
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(900, 850), &rectangle));
	CHECK(state.complete(point(1600, 1600), &rectangle, &saved));
	CHECK(!state.completion_valid());
	CHECK(same_rectangle(rectangle, state.pre_drag_rectangle()));
	CHECK(saved.width == 300 && saved.height == 240);
}

void display_invalidation_is_detected()
{
	DisplaySignature changed = display_signature();
	++changed.monitor;
	CHECK(!transaction().display_matches(changed));
	changed = display_signature();
	++changed.geometry.x;
	CHECK(!transaction().display_matches(changed));
	changed = display_signature();
	++changed.workarea.height;
	CHECK(!transaction().display_matches(changed));
	changed = display_signature();
	++changed.scale;
	CHECK(!transaction().display_matches(changed));
	changed = display_signature();
	++changed.screen;
	CHECK(!transaction().display_matches(changed));

	Transaction state = transaction();
	CHECK(state.display_matches(display_signature()));
	Rectangle restored = {};
	SavedNormalSize saved = {};
	CHECK(state.cancel(&restored, &saved));
	CHECK(!state.display_matches(display_signature()));
}

void terminal_transaction_can_be_replaced()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.complete(point(510, 510), &rectangle, &saved));

	const Rectangle next = {20, 30, 400, 350};
	CHECK(state.begin(
			Direction::Left,
			BackendPolicy::X11Live,
			next,
			point(100, 100),
			{{10, 1000}, {10, 900}},
			opposite_edge_anchor(next),
			{400, 350},
			display_signature(),
			true));
	CHECK(state.active());
	CHECK(state.motion(point(90, 100), &rectangle));
	CHECK(rectangle.x == 10 && rectangle.width == 410);
}

void centered_all_directions_keep_monitor_center()
{
	const Direction directions[] = {
		Direction::TopLeft, Direction::Top, Direction::TopRight,
		Direction::Left, Direction::Right,
		Direction::BottomLeft, Direction::Bottom, Direction::BottomRight
	};
	const Rectangle monitor = {-2560, 180, 2560, 1440};
	for (Direction direction : directions)
	{
		const Rectangle press = {-1760, 600, 960, 600};
		const ReducerState state = {
			direction,
			press,
			point(300, 300),
			{{100, 2560}, {100, 1440}},
			monitor_center_anchor(monitor),
			press
		};
		const DirectionAxes axes = direction_axes(direction);
		const Rectangle result = candidate(state,
				point(300 + (axes.horizontal * 37),
					300 + (axes.vertical * 29)));
		CHECK((result.x * 2) + result.width == -2560);
		CHECK((result.y * 2) + result.height == 1800);
	}
}

void panel_gap_uses_a_stable_base()
{
	const Rectangle monitor = {100, 50, 1200, 800};
	const Rectangle top_base = {300, 50, 400, 300};
	const Rectangle left_base = {100, 220, 400, 300};
	const Rectangle bottom_base = {300, 550, 400, 300};
	const Rectangle right_base = {900, 220, 400, 300};

	const Rectangle top = place_docked(top_base, monitor, PanelEdge::Top, 12);
	const Rectangle top_again = place_docked(
			top_base, monitor, PanelEdge::Top, 12);
	CHECK(top.y == 62 && top_again.y == 62);
	CHECK(place_docked(bottom_base, monitor,
			PanelEdge::Bottom, 12).y == 538);
	CHECK(place_docked(left_base, monitor,
			PanelEdge::Left, 12).x == 112);
	CHECK(place_docked(right_base, monitor,
			PanelEdge::Right, 12).x == 888);
}

} // namespace

int main()
{
	direction_mapping();
	all_edges_keep_opposite_anchor();
	every_sample_is_press_relative();
	invalid_edge_request_retains_last_displayed();
	invalid_corner_request_rejects_both_axes();
	centered_geometry_is_symmetric();
	wide_intermediates_are_rejected_safely();
	coordinate_spaces_do_not_mix();
	motion_coalesces_to_one_latest_frame();
	invalid_latest_motion_discards_older_pending_geometry();
	displayed_geometry_advances_only_after_acknowledgement();
	valid_release_is_synchronous_and_idempotent();
	invalid_release_retains_displayed_geometry();
	cancellation_restores_exact_geometry();
	fullscreen_completion_does_not_persist();
	wayland_applies_only_at_release();
	wayland_invalid_release_retains_press_geometry();
	display_invalidation_is_detected();
	terminal_transaction_can_be_replaced();
	centered_all_directions_keep_monitor_center();
	panel_gap_uses_a_stable_base();

	if (g_failures)
	{
		std::fprintf(stderr, "test_interactive_resize: %d failure(s)\n",
				g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_interactive_resize: ok\n");
	return EXIT_SUCCESS;
}
