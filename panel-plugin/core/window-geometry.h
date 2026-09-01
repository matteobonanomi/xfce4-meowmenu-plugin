/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef WHISKERMENU_WINDOW_GEOMETRY_H
#define WHISKERMENU_WINDOW_GEOMETRY_H

#include "interactive-resize.h"

namespace WhiskerMenu
{

struct WindowGeometryChanges
{
	bool origin;
	bool width;
	bool height;

	bool empty() const { return !origin && !width && !height; }
};

struct WindowResizeFramePlan
{
	WindowGeometryChanges changes;
	int current_toplevel_width;
	int requested_toplevel_width;

	bool empty() const { return changes.empty(); }
	bool updates_result_width() const { return changes.width; }
	bool updates_vertical_overflow() const { return changes.height; }
	bool moves_window() const { return changes.origin; }
};

/* window_geometry_changes:
 * @displayed: geometry currently presented by the window.
 * @requested: accepted geometry to present next.
 *
 * Classifies changed axes so a resize frame can skip unrelated layout work.
 *
 * Returns: the origin and size components that differ.
 */
inline WindowGeometryChanges window_geometry_changes(
		const InteractiveResize::Rectangle& displayed,
		const InteractiveResize::Rectangle& requested)
{
	return {
		displayed.x != requested.x || displayed.y != requested.y,
		displayed.width != requested.width,
		displayed.height != requested.height,
	};
}

/* window_resize_frame_plan:
 * @displayed: geometry currently presented by the window.
 * @requested: newest accepted geometry owned by the pending Window frame.
 *
 * Captures the exact old and new toplevel widths used by the synchronous
 * result-grid update and classifies the remaining axis-specific work. The plan
 * is immutable for one delivery, so no child callback can substitute a stale
 * width after the Window begins applying the frame.
 *
 * Returns: the complete work plan for one visible resize transaction.
 */
inline WindowResizeFramePlan window_resize_frame_plan(
		const InteractiveResize::Rectangle& displayed,
		const InteractiveResize::Rectangle& requested)
{
	return {
		window_geometry_changes(displayed, requested),
		displayed.width,
		requested.width,
	};
}

/* window_resize_frame_should_schedule:
 * @callback_id: current GTK tick callback identifier, or zero when idle.
 * @frame_pending: whether the transaction owns an undelivered request.
 *
 * Returns: true only when one callback is needed and none is already owned.
 */
inline bool window_resize_frame_should_schedule(unsigned int callback_id,
		bool frame_pending)
{
	return callback_id == 0 && frame_pending;
}

}

/* window-geometry: private translation unit hosting Window's resize and
 * size-persistence helpers (resize/resize_start/resize_end/set_size and the
 * scrollbar fit check). Positioning logic (center_window, move_window) and
 * focus/grab handling intentionally stay in window.cpp.
 */

#endif // WHISKERMENU_WINDOW_GEOMETRY_H
