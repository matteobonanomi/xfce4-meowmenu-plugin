/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_CORE_INTERACTIVE_RESIZE_H
#define MEOWMENU_CORE_INTERACTIVE_RESIZE_H

#include <cstdint>

namespace WhiskerMenu
{
namespace InteractiveResize
{

enum class Direction
{
	TopLeft,
	Top,
	TopRight,
	Left,
	Right,
	BottomLeft,
	Bottom,
	BottomRight
};

enum class CoordinateSpace
{
	Screen,
	Handle
};

enum class AnchorKind
{
	OppositeEdge,
	MonitorCenter
};

enum class BackendPolicy
{
	X11Live,
	WaylandReleaseToApply
};

enum class Lifecycle
{
	Idle,
	Active,
	Completed,
	Cancelled
};

enum class PanelEdge
{
	None,
	Top,
	Bottom,
	Left,
	Right
};

struct DirectionAxes
{
	int horizontal;
	int vertical;
};

struct Rectangle
{
	int x;
	int y;
	int width;
	int height;
};

struct PointerSample
{
	std::int64_t x;
	std::int64_t y;
	std::uint32_t time;
	CoordinateSpace space;
};

struct AxisBounds
{
	int minimum;
	int maximum;
};

struct Bounds
{
	AxisBounds width;
	AxisBounds height;
};

struct Anchor
{
	AnchorKind kind;
	std::int64_t left;
	std::int64_t right;
	std::int64_t top;
	std::int64_t bottom;
	std::int64_t center_x_twice;
	std::int64_t center_y_twice;
};

struct ReducerState
{
	Direction direction;
	Rectangle current;
	PointerSample last_pointer;
	Bounds bounds;
	Anchor anchor;
};

struct SavedNormalSize
{
	int width;
	int height;
};

struct DisplaySignature
{
	std::uintptr_t monitor;
	Rectangle geometry;
	Rectangle workarea;
	int scale;
	std::uintptr_t screen;
};

DirectionAxes direction_axes(Direction direction);
Anchor opposite_edge_anchor(const Rectangle& rectangle);
Anchor monitor_center_anchor(const Rectangle& monitor);
bool same_display(const DisplaySignature& left, const DisplaySignature& right);

/* place_docked:
 * @base: unadjusted panel-relative window rectangle.
 * @monitor: full monitor geometry in the same coordinate space.
 * @edge: panel edge from which a positive gap moves the window away.
 * @gap: configured logical-pixel gap; non-positive values have no effect.
 *
 * Applies the panel gap once and clamps the resulting origin to the monitor.
 * Reusing the same base always returns the same placement.
 *
 * Returns: the absolute placed rectangle.
 */
Rectangle place_docked(const Rectangle& base,
		const Rectangle& monitor,
		PanelEdge edge,
		int gap);

/* reduce:
 * @state: active geometry and pointer baseline; updated in place.
 * @sample: next pointer position in the transaction's coordinate space.
 *
 * Applies one pointer sample independently on both selected axes. Pointer
 * baselines advance even when a size is clamped, which discards overshoot and
 * makes the first reverse movement effective. Wide intermediates are clamped
 * before they are converted back to window geometry.
 *
 * Returns: the accepted absolute rectangle stored in @state.
 */
Rectangle reduce(ReducerState& state, const PointerSample& sample);

class Transaction
{
public:
	Transaction();

	/* begin:
	 * Captures all geometry, persistence, coordinate, and display state needed
	 * by one resize. A transaction can begin only while idle.
	 *
	 * Returns: true when the transaction became active.
	 */
	bool begin(Direction direction,
			BackendPolicy policy,
			const Rectangle& rectangle,
			const PointerSample& pointer,
			const Bounds& bounds,
			const Anchor& anchor,
			const SavedNormalSize& saved_size,
			const DisplaySignature& display,
			bool normal_presentation);

	/* motion:
	 * @sample: next event-time pointer sample.
	 * @accepted: receives the current accepted rectangle; never NULL.
	 *
	 * X11 transactions apply the reducer immediately. Wayland transactions
	 * retain their press geometry so local coordinates remain stable until
	 * release.
	 *
	 * Returns: false for an inactive or mismatched-coordinate event.
	 */
	bool motion(const PointerSample& sample, Rectangle* accepted);

	/* complete:
	 * @release: final event-time pointer sample, consumed before completion.
	 * @accepted: receives the final accepted rectangle; never NULL.
	 * @saved_size: receives the post-completion normal size; never NULL.
	 *
	 * Completion is idempotent and updates the saved size only for a normal
	 * presentation.
	 *
	 * Returns: true for a completed transaction, including a repeated call.
	 */
	bool complete(const PointerSample& release,
			Rectangle* accepted,
			SavedNormalSize* saved_size);

	/* cancel:
	 * @restored: receives the exact pre-drag rectangle; never NULL.
	 * @saved_size: receives the unchanged pre-drag saved size; never NULL.
	 *
	 * Cancellation is idempotent and never exposes partially accepted
	 * geometry or a transient saved size.
	 *
	 * Returns: true for a cancelled transaction, including a repeated call.
	 */
	bool cancel(Rectangle* restored, SavedNormalSize* saved_size);

	bool display_matches(const DisplaySignature& display) const;

	Lifecycle lifecycle() const
	{
		return m_lifecycle;
	}

	bool active() const
	{
		return m_lifecycle == Lifecycle::Active;
	}

	BackendPolicy policy() const
	{
		return m_policy;
	}

	const Rectangle& rectangle() const
	{
		return m_reducer.current;
	}

	const Rectangle& pre_drag_rectangle() const
	{
		return m_pre_drag;
	}

private:
	Lifecycle m_lifecycle;
	BackendPolicy m_policy;
	ReducerState m_reducer;
	Rectangle m_pre_drag;
	SavedNormalSize m_saved_before;
	SavedNormalSize m_saved_after;
	DisplaySignature m_display;
	bool m_normal_presentation;
};

} // namespace InteractiveResize
} // namespace WhiskerMenu

#endif // MEOWMENU_CORE_INTERACTIVE_RESIZE_H
