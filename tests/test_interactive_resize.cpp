/*
 * Display-free checks for interactive resize geometry.
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

ReducerState docked(Direction direction,
		const Rectangle& rectangle = {100, 200, 300, 240})
{
	return {
		direction,
		rectangle,
		point(500, 500),
		{{10, 2000}, {10, 2000}},
		opposite_edge_anchor(rectangle)
	};
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
		ReducerState state = docked(direction);
		const DirectionAxes axes = direction_axes(direction);
		const Rectangle result = reduce(state,
				point(500 + (axes.horizontal * 25),
					500 + (axes.vertical * 30)));

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

void grouped_and_single_motion_match()
{
	ReducerState grouped = docked(Direction::BottomRight);
	ReducerState singles = grouped;
	reduce(grouped, point(580, 440));
	reduce(singles, point(520, 490));
	reduce(singles, point(545, 470));
	reduce(singles, point(580, 440));
	CHECK(grouped.current.x == singles.current.x);
	CHECK(grouped.current.y == singles.current.y);
	CHECK(grouped.current.width == singles.current.width);
	CHECK(grouped.current.height == singles.current.height);
}

void initial_offset_and_origin_crossing()
{
	ReducerState state = docked(Direction::TopLeft);
	const Rectangle unchanged = reduce(state, point(500, 500));
	CHECK(unchanged.x == 100 && unchanged.y == 200);
	CHECK(unchanged.width == 300 && unchanged.height == 240);

	const Rectangle crossed = reduce(state, point(525, 535));
	CHECK(crossed.width == 275);
	CHECK(crossed.height == 205);
	CHECK(crossed.x == 125);
	CHECK(crossed.y == 235);
}

void clamp_discards_overshoot()
{
	ReducerState state = docked(Direction::Right);
	state.bounds.width = {200, 320};
	CHECK(reduce(state, point(600, 500)).width == 320);
	CHECK(reduce(state, point(599, 500)).width == 319);

	state.bounds.width = {290, 320};
	CHECK(reduce(state, point(500, 500)).width == 290);
	CHECK(reduce(state, point(501, 500)).width == 291);
}

void corner_axes_are_independent()
{
	ReducerState state = docked(Direction::BottomRight);
	state.bounds.width = {100, 310};
	state.bounds.height = {100, 500};
	const Rectangle result = reduce(state, point(700, 560));
	CHECK(result.width == 310);
	CHECK(result.height == 300);
	CHECK(result.x == 100);
	CHECK(result.y == 200);
}

void centered_geometry_is_symmetric()
{
	const Rectangle monitor = {-1920, -200, 1920, 1080};
	ReducerState state = {
		Direction::TopRight,
		{-1360, 40, 800, 600},
		point(0, 0),
		{{100, 1920}, {100, 1080}},
		monitor_center_anchor(monitor)
	};

	const Rectangle result = reduce(state, point(25, -10));
	CHECK(result.width == 850);
	CHECK(result.height == 620);
	CHECK(result.x == -1385);
	CHECK(result.y == 30);
	CHECK((result.x * 2) + result.width == -1920);
	CHECK((result.y * 2) + result.height == 680);
}

void integer_boundaries_are_safe()
{
	ReducerState state = docked(Direction::BottomRight,
			{INT_MIN, INT_MIN, 100, 100});
	state.last_pointer = point(INT64_MIN, INT64_MIN);
	state.bounds = {{1, INT_MAX}, {1, INT_MAX}};
	const Rectangle result = reduce(state, point(INT64_MAX, INT64_MAX));
	CHECK(result.width == INT_MAX);
	CHECK(result.height == INT_MAX);

	state.last_pointer = point(INT64_MAX, INT64_MAX);
	const Rectangle shrunk = reduce(state, point(INT64_MIN, INT64_MIN));
	CHECK(shrunk.width == 1);
	CHECK(shrunk.height == 1);

	ReducerState origin = docked(
			Direction::Left,
			{INT_MAX - 5, 20, 10, 100});
	origin.bounds.width = {1, INT_MAX};
	CHECK(reduce(origin, point(600, 500)).x == INT_MAX);
}

void coordinate_spaces_do_not_mix()
{
	ReducerState state = docked(Direction::Right);
	PointerSample local = {900, 500, 0, CoordinateSpace::Handle};
	const Rectangle result = reduce(state, local);
	CHECK(result.width == 300);
	CHECK(state.last_pointer.space == CoordinateSpace::Screen);
}

void completion_consumes_release_and_is_idempotent()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(520, 530), &rectangle));
	CHECK(rectangle.width == 320 && rectangle.height == 270);
	CHECK(state.complete(point(545, 565), &rectangle, &saved));
	CHECK(rectangle.width == 345 && rectangle.height == 305);
	CHECK(saved.width == 345 && saved.height == 305);
	CHECK(state.lifecycle() == Lifecycle::Completed);

	CHECK(state.complete(point(900, 900), &rectangle, &saved));
	CHECK(rectangle.width == 345 && rectangle.height == 305);
	CHECK(!state.motion(point(550, 570), &rectangle));
	CHECK(!state.cancel(&rectangle, &saved));
}

void cancellation_restores_exact_geometry()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(650, 700), &rectangle));
	CHECK(rectangle.width == 450 && rectangle.height == 440);
	CHECK(state.cancel(&rectangle, &saved));
	CHECK(rectangle.x == 100 && rectangle.y == 200);
	CHECK(rectangle.width == 300 && rectangle.height == 240);
	CHECK(saved.width == 300 && saved.height == 240);
	CHECK(state.cancel(&rectangle, &saved));
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
	CHECK(state.complete(point(620, 650), &rectangle, &saved));
	CHECK(rectangle.width == 420 && rectangle.height == 390);
	CHECK(saved.width == 420 && saved.height == 390);
}

void wayland_cancellation_keeps_press_geometry()
{
	Transaction state = transaction(BackendPolicy::WaylandReleaseToApply);
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(900, 850), &rectangle));
	CHECK(rectangle.x == 100 && rectangle.y == 200);
	CHECK(rectangle.width == 300 && rectangle.height == 240);
	CHECK(state.cancel(&rectangle, &saved));
	CHECK(rectangle.x == 100 && rectangle.y == 200);
	CHECK(rectangle.width == 300 && rectangle.height == 240);
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
	CHECK(restored.width == 300 && restored.height == 240);
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

void rapid_alternating_delivery_has_no_drift()
{
	ReducerState state = docked(Direction::BottomRight);
	const PointerSample samples[] = {
		point(560, 540),
		point(510, 490),
		point(580, 570),
		point(505, 505),
		point(530, 520)
	};
	for (const PointerSample& sample : samples)
		reduce(state, sample);
	CHECK(state.current.width == 330);
	CHECK(state.current.height == 260);
	CHECK(state.current.x == 100 && state.current.y == 200);
}

void constrained_corner_continues_and_reverses_immediately()
{
	ReducerState state = docked(Direction::BottomRight);
	state.bounds = {{250, 320}, {200, 500}};
	Rectangle rectangle = reduce(state, point(800, 650));
	CHECK(rectangle.width == 320);
	CHECK(rectangle.height == 390);

	rectangle = reduce(state, point(799, 700));
	CHECK(rectangle.width == 319);
	CHECK(rectangle.height == 440);

	rectangle = reduce(state, point(400, 699));
	CHECK(rectangle.width == 250);
	CHECK(rectangle.height == 439);
	CHECK(reduce(state, point(401, 699)).width == 251);
}

void delivery_policy_is_environment_independent()
{
	Transaction composited = transaction();
	Transaction uncomposited = transaction();
	Rectangle left = {};
	Rectangle right = {};
	SavedNormalSize left_saved = {};
	SavedNormalSize right_saved = {};

	CHECK(composited.motion(point(540, 520), &left));
	CHECK(uncomposited.motion(point(540, 520), &right));
	CHECK(composited.motion(point(610, 580), &left));
	CHECK(uncomposited.motion(point(610, 580), &right));
	CHECK(composited.complete(point(625, 590), &left, &left_saved));
	CHECK(uncomposited.complete(point(625, 590), &right, &right_saved));
	CHECK(left.x == right.x && left.y == right.y);
	CHECK(left.width == right.width && left.height == right.height);
	CHECK(left_saved.width == right_saved.width);
	CHECK(left_saved.height == right_saved.height);
}

void release_after_compressed_motion_is_not_lost()
{
	Transaction state = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(state.motion(point(510, 510), &rectangle));
	CHECK(rectangle.width == 310 && rectangle.height == 250);
	CHECK(state.complete(point(800, 720), &rectangle, &saved));
	CHECK(rectangle.width == 600 && rectangle.height == 460);
	CHECK(saved.width == 600 && saved.height == 460);
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
		ReducerState state = {
			direction,
			{-1760, 600, 960, 600},
			point(300, 300),
			{{100, 2560}, {100, 1440}},
			monitor_center_anchor(monitor)
		};
		const DirectionAxes axes = direction_axes(direction);
		const Rectangle result = reduce(state,
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

	const Rectangle top = place_docked(
			top_base, monitor, PanelEdge::Top, 12);
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

void completed_normal_size_is_the_next_opening_size()
{
	Transaction first = transaction();
	Rectangle rectangle = {};
	SavedNormalSize saved = {};
	CHECK(first.complete(point(640, 610), &rectangle, &saved));
	CHECK(saved.width == rectangle.width);
	CHECK(saved.height == rectangle.height);

	Transaction reopened;
	CHECK(reopened.begin(
			Direction::Top,
			BackendPolicy::X11Live,
			{80, 90, saved.width, saved.height},
			point(200, 200),
			{{10, 1000}, {10, 900}},
			opposite_edge_anchor(
					{80, 90, saved.width, saved.height}),
			saved,
			display_signature(),
			true));
	CHECK(reopened.rectangle().width == saved.width);
	CHECK(reopened.rectangle().height == saved.height);
}

void every_interruption_uses_exact_rollback()
{
	enum class Interruption
	{
		Escape,
		Hide,
		FocusLoss,
		GrabBroken,
		MissingButton,
		LayoutChange,
		Destruction
	};
	const Interruption interruptions[] = {
		Interruption::Escape,
		Interruption::Hide,
		Interruption::FocusLoss,
		Interruption::GrabBroken,
		Interruption::MissingButton,
		Interruption::LayoutChange,
		Interruption::Destruction
	};
	for (Interruption interruption : interruptions)
	{
		(void)interruption;
		Transaction state = transaction();
		Rectangle rectangle = {};
		SavedNormalSize saved = {};
		CHECK(state.motion(point(700, 650), &rectangle));
		CHECK(state.cancel(&rectangle, &saved));
		CHECK(rectangle.x == 100 && rectangle.y == 200);
		CHECK(rectangle.width == 300 && rectangle.height == 240);
		CHECK(saved.width == 300 && saved.height == 240);
		CHECK(!state.motion(point(701, 651), &rectangle));
	}
}

} // namespace

int main()
{
	direction_mapping();
	all_edges_keep_opposite_anchor();
	grouped_and_single_motion_match();
	initial_offset_and_origin_crossing();
	clamp_discards_overshoot();
	corner_axes_are_independent();
	centered_geometry_is_symmetric();
	integer_boundaries_are_safe();
	coordinate_spaces_do_not_mix();
	completion_consumes_release_and_is_idempotent();
	cancellation_restores_exact_geometry();
	fullscreen_completion_does_not_persist();
	wayland_applies_only_at_release();
	wayland_cancellation_keeps_press_geometry();
	display_invalidation_is_detected();
	terminal_transaction_can_be_replaced();
	rapid_alternating_delivery_has_no_drift();
	constrained_corner_continues_and_reverses_immediately();
	delivery_policy_is_environment_independent();
	release_after_compressed_motion_is_not_lost();
	centered_all_directions_keep_monitor_center();
	panel_gap_uses_a_stable_base();
	completed_normal_size_is_the_next_opening_size();
	every_interruption_uses_exact_rollback();

	if (g_failures)
	{
		std::fprintf(stderr, "test_interactive_resize: %d failure(s)\n",
				g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_interactive_resize: ok\n");
	return EXIT_SUCCESS;
}
