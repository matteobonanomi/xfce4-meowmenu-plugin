/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "interactive-resize.h"

#include <algorithm>
#include <limits>

using namespace WhiskerMenu::InteractiveResize;

namespace
{

/* clamp_axis:
 * @value: wide candidate size.
 * @bounds: accepted inclusive range; reversed values are normalized.
 *
 * Returns: @value clamped before conversion to a window integer.
 */
int clamp_axis(__int128 value, const AxisBounds& bounds)
{
	const int low = std::min(bounds.minimum, bounds.maximum);
	const int high = std::max(bounds.minimum, bounds.maximum);
	if (value < low)
		return low;
	if (value > high)
		return high;
	return static_cast<int>(value);
}

/* clamp_window_integer:
 * @value: wide candidate origin or limit.
 *
 * Returns: @value constrained to the platform window-integer range.
 */
int clamp_window_integer(__int128 value)
{
	if (value < std::numeric_limits<int>::min())
		return std::numeric_limits<int>::min();
	if (value > std::numeric_limits<int>::max())
		return std::numeric_limits<int>::max();
	return static_cast<int>(value);
}

} // namespace

/* direction_axes:
 * @direction: one of the eight visible handle directions.
 *
 * Returns: horizontal and vertical edge signs for reducer arithmetic.
 */
DirectionAxes WhiskerMenu::InteractiveResize::direction_axes(Direction direction)
{
	switch (direction)
	{
	case Direction::TopLeft:     return {-1, -1};
	case Direction::Top:         return { 0, -1};
	case Direction::TopRight:    return { 1, -1};
	case Direction::Left:        return {-1,  0};
	case Direction::Right:       return { 1,  0};
	case Direction::BottomLeft:  return {-1,  1};
	case Direction::Bottom:      return { 0,  1};
	case Direction::BottomRight: return { 1,  1};
	}
	return {0, 0};
}

/* opposite_edge_anchor:
 * @rectangle: exact starting docked geometry.
 *
 * Returns: all four starting edges in wide coordinates.
 */
Anchor WhiskerMenu::InteractiveResize::opposite_edge_anchor(
		const Rectangle& rectangle)
{
	return {
		AnchorKind::OppositeEdge,
		rectangle.x,
		static_cast<std::int64_t>(rectangle.x) + rectangle.width,
		rectangle.y,
		static_cast<std::int64_t>(rectangle.y) + rectangle.height,
		0,
		0
	};
}

/* monitor_center_anchor:
 * @monitor: frozen full-monitor geometry.
 *
 * Returns: the doubled monitor center, preserving odd-pixel precision.
 */
Anchor WhiskerMenu::InteractiveResize::monitor_center_anchor(
		const Rectangle& monitor)
{
	return {
		AnchorKind::MonitorCenter,
		0,
		0,
		0,
		0,
		(static_cast<std::int64_t>(monitor.x) * 2) + monitor.width,
		(static_cast<std::int64_t>(monitor.y) * 2) + monitor.height
	};
}

/* same_display:
 * @left / @right: display signatures to compare.
 *
 * Returns: true only when every coordinate and constraint property matches.
 */
bool WhiskerMenu::InteractiveResize::same_display(
		const DisplaySignature& left,
		const DisplaySignature& right)
{
	return left.monitor == right.monitor
			&& left.geometry.x == right.geometry.x
			&& left.geometry.y == right.geometry.y
			&& left.geometry.width == right.geometry.width
			&& left.geometry.height == right.geometry.height
			&& left.workarea.x == right.workarea.x
			&& left.workarea.y == right.workarea.y
			&& left.workarea.width == right.workarea.width
			&& left.workarea.height == right.workarea.height
			&& left.scale == right.scale
			&& left.screen == right.screen;
}

/* place_docked:
 * See interactive-resize.h for the stable-base placement contract.
 */
Rectangle WhiskerMenu::InteractiveResize::place_docked(
		const Rectangle& base,
		const Rectangle& monitor,
		PanelEdge edge,
		int gap)
{
	__int128 x = base.x;
	__int128 y = base.y;
	if (gap > 0)
	{
		switch (edge)
		{
		case PanelEdge::Top:    y += gap; break;
		case PanelEdge::Bottom: y -= gap; break;
		case PanelEdge::Left:   x += gap; break;
		case PanelEdge::Right:  x -= gap; break;
		case PanelEdge::None:   break;
		}
	}

	const std::int64_t maximum_x = static_cast<std::int64_t>(monitor.x)
			+ monitor.width - base.width;
	const std::int64_t maximum_y = static_cast<std::int64_t>(monitor.y)
			+ monitor.height - base.height;
	Rectangle result = base;
	result.x = clamp_axis(x,
			{monitor.x, clamp_window_integer(maximum_x)});
	result.y = clamp_axis(y,
			{monitor.y, clamp_window_integer(maximum_y)});
	return result;
}

/* reduce:
 * See interactive-resize.h for the independent-axis reducer contract.
 */
Rectangle WhiskerMenu::InteractiveResize::reduce(
		ReducerState& state,
		const PointerSample& sample)
{
	if (sample.space != state.last_pointer.space)
		return state.current;

	const DirectionAxes axes = direction_axes(state.direction);
	const __int128 delta_x = static_cast<__int128>(sample.x)
			- state.last_pointer.x;
	const __int128 delta_y = static_cast<__int128>(sample.y)
			- state.last_pointer.y;
	const int scale = state.anchor.kind == AnchorKind::MonitorCenter ? 2 : 1;

	int width = state.current.width;
	int height = state.current.height;
	if (axes.horizontal)
	{
		width = clamp_axis(static_cast<__int128>(width)
				+ (delta_x * axes.horizontal * scale), state.bounds.width);
	}
	if (axes.vertical)
	{
		height = clamp_axis(static_cast<__int128>(height)
				+ (delta_y * axes.vertical * scale), state.bounds.height);
	}

	Rectangle accepted = state.current;
	accepted.width = width;
	accepted.height = height;
	if (state.anchor.kind == AnchorKind::MonitorCenter)
	{
		accepted.x = clamp_window_integer(
				(state.anchor.center_x_twice - width) / 2);
		accepted.y = clamp_window_integer(
				(state.anchor.center_y_twice - height) / 2);
	}
	else
	{
		if (axes.horizontal < 0)
			accepted.x = clamp_window_integer(state.anchor.right - width);
		else if (axes.horizontal > 0)
			accepted.x = clamp_window_integer(state.anchor.left);

		if (axes.vertical < 0)
			accepted.y = clamp_window_integer(state.anchor.bottom - height);
		else if (axes.vertical > 0)
			accepted.y = clamp_window_integer(state.anchor.top);
	}

	// A constrained sample still becomes the new baseline. This intentionally
	// drops excess movement instead of creating a reversal dead zone.
	state.last_pointer = sample;
	state.current = accepted;
	return accepted;
}

/* Transaction::Transaction:
 *
 * Creates an inert transaction with valid placeholder geometry. A later
 * begin() replaces every field before the state becomes observable.
 */
Transaction::Transaction() :
	m_lifecycle(Lifecycle::Idle),
	m_policy(BackendPolicy::X11Live),
	m_reducer{
		Direction::BottomRight,
		{0, 0, 1, 1},
		{0, 0, 0, CoordinateSpace::Screen},
		{{1, 1}, {1, 1}},
		{AnchorKind::OppositeEdge, 0, 1, 0, 1, 0, 0}},
	m_pre_drag{0, 0, 1, 1},
	m_saved_before{1, 1},
	m_saved_after{1, 1},
	m_display{0, {0, 0, 1, 1}, {0, 0, 1, 1}, 1, 0},
	m_normal_presentation(true)
{
}

/* Transaction::begin:
 * See interactive-resize.h for transaction capture and replacement rules.
 */
bool Transaction::begin(
		Direction direction,
		BackendPolicy policy,
		const Rectangle& rectangle,
		const PointerSample& pointer,
		const Bounds& bounds,
		const Anchor& anchor,
		const SavedNormalSize& saved_size,
		const DisplaySignature& display,
		bool normal_presentation)
{
	if (m_lifecycle == Lifecycle::Active)
		return false;

	m_policy = policy;
	m_reducer = {direction, rectangle, pointer, bounds, anchor};
	m_pre_drag = rectangle;
	m_saved_before = saved_size;
	m_saved_after = saved_size;
	m_display = display;
	m_normal_presentation = normal_presentation;
	m_lifecycle = Lifecycle::Active;
	return true;
}

/* Transaction::motion:
 * See interactive-resize.h for live and release-to-apply behavior.
 */
bool Transaction::motion(
		const PointerSample& sample,
		Rectangle* accepted)
{
	if (!active() || sample.space != m_reducer.last_pointer.space)
		return false;

	if (m_policy == BackendPolicy::X11Live)
		reduce(m_reducer, sample);
	*accepted = m_reducer.current;
	return true;
}

/* Transaction::complete:
 * See interactive-resize.h for final sampling and persistence semantics.
 */
bool Transaction::complete(
		const PointerSample& release,
		Rectangle* accepted,
		SavedNormalSize* saved_size)
{
	if (m_lifecycle == Lifecycle::Completed)
	{
		*accepted = m_reducer.current;
		*saved_size = m_saved_after;
		return true;
	}
	if (!active() || release.space != m_reducer.last_pointer.space)
		return false;

	// X11 consumes movement after the last motion event; Wayland consumes the
	// complete press-to-release displacement because motion kept the baseline.
	reduce(m_reducer, release);
	if (m_normal_presentation)
	{
		m_saved_after.width = m_reducer.current.width;
		m_saved_after.height = m_reducer.current.height;
	}
	m_lifecycle = Lifecycle::Completed;
	*accepted = m_reducer.current;
	*saved_size = m_saved_after;
	return true;
}

/* Transaction::cancel:
 * See interactive-resize.h for exact rollback and idempotency semantics.
 */
bool Transaction::cancel(
		Rectangle* restored,
		SavedNormalSize* saved_size)
{
	if (m_lifecycle == Lifecycle::Completed || m_lifecycle == Lifecycle::Idle)
		return false;

	m_lifecycle = Lifecycle::Cancelled;
	*restored = m_pre_drag;
	*saved_size = m_saved_before;
	return true;
}

/* Transaction::display_matches:
 * @display: live signature for the retained active monitor.
 *
 * Returns: true only while active and while every captured property matches.
 */
bool Transaction::display_matches(const DisplaySignature& display) const
{
	return active() && same_display(m_display, display);
}
