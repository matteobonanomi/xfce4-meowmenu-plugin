/*
 * Copyright (C) 2021-2025 Graeme Gott <graeme@gottcode.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "resizer.h"

#include "ui/slot.h"
#include "window.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

using namespace WhiskerMenu;

namespace
{

bool uses_x11_root_coordinates(GdkDisplay* display)
{
#ifdef GDK_WINDOWING_X11
	return GDK_IS_X11_DISPLAY(display);
#else
	return false;
#endif
}

/* event_pointer:
 * @event: press, motion, or release event from one resize handle.
 * @space: stable coordinate space selected when the press began.
 *
 * Uses coordinates carried by the event so queued events remain ordered.
 * X11 samples use the root window; fallback samples use the unchanged handle
 * frame and never poll the current device position.
 *
 * Returns: a logical pointer sample with the event timestamp.
 */
InteractiveResize::PointerSample event_pointer(
		GdkEvent* event,
		InteractiveResize::CoordinateSpace space)
{
	double x = 0;
	double y = 0;
	if (space == InteractiveResize::CoordinateSpace::Screen)
		gdk_event_get_root_coords(event, &x, &y);
	else
		gdk_event_get_coords(event, &x, &y);
	return {
		static_cast<std::int64_t>(x),
		static_cast<std::int64_t>(y),
		gdk_event_get_time(event),
		space
	};
}

/* resize_direction:
 * @edge: one of the eight visible handle positions.
 *
 * Returns: the equivalent reducer direction without relying on enum ordinals.
 */
InteractiveResize::Direction resize_direction(Resizer::Edge edge)
{
	switch (edge)
	{
	case Resizer::TopLeft:     return InteractiveResize::Direction::TopLeft;
	case Resizer::Top:         return InteractiveResize::Direction::Top;
	case Resizer::TopRight:    return InteractiveResize::Direction::TopRight;
	case Resizer::Left:        return InteractiveResize::Direction::Left;
	case Resizer::Right:       return InteractiveResize::Direction::Right;
	case Resizer::BottomLeft:  return InteractiveResize::Direction::BottomLeft;
	case Resizer::Bottom:      return InteractiveResize::Direction::Bottom;
	case Resizer::BottomRight: return InteractiveResize::Direction::BottomRight;
	}
	return InteractiveResize::Direction::BottomRight;
}

} // namespace

//-----------------------------------------------------------------------------

Resizer::Resizer(Edge edge, Window* window) :
	m_window(window),
	m_cursor(nullptr),
	m_direction(resize_direction(edge)),
	m_coordinate_space(InteractiveResize::CoordinateSpace::Handle),
	m_pressed(false)
{
	m_drawing = gtk_drawing_area_new();
	gtk_widget_set_size_request(m_drawing, HandleSize, HandleSize);
	gtk_widget_add_events(m_drawing, GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_POINTER_MOTION_MASK
			| GDK_ENTER_NOTIFY_MASK
			| GDK_LEAVE_NOTIFY_MASK);

	connect(m_drawing, "button-press-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
			if (event_button->button != 1)
			{
				return GDK_EVENT_PROPAGATE;
			}
			m_coordinate_space = uses_x11_root_coordinates(
					gtk_widget_get_display(m_drawing))
					? InteractiveResize::CoordinateSpace::Screen
					: InteractiveResize::CoordinateSpace::Handle;
			const InteractiveResize::PointerSample pointer =
					event_pointer(event, m_coordinate_space);
			// HACK: Wayland has no dependable desktop-global coordinate space.
			// Keep the handle frame unchanged until release, then apply the
			// cumulative local displacement once. This also covers layer shell.
			const InteractiveResize::BackendPolicy policy =
					m_coordinate_space
							== InteractiveResize::CoordinateSpace::Screen
					? InteractiveResize::BackendPolicy::X11Live
					: InteractiveResize::BackendPolicy::WaylandReleaseToApply;
			m_pressed = m_window->interactive_resize_begin(
					m_direction,
					policy,
					pointer);

			return m_pressed ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
		});

	connect(m_drawing, "button-release-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
			if (event_button->button != 1)
			{
				return GDK_EVENT_PROPAGATE;
			}
			const InteractiveResize::PointerSample pointer =
					event_pointer(event, m_coordinate_space);
			m_window->interactive_resize_complete(pointer);
			m_pressed = false;

			return GDK_EVENT_STOP;
		});

	connect(m_drawing, "motion-notify-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			if (!m_pressed)
			{
				return GDK_EVENT_PROPAGATE;
			}

			GdkEventMotion* event_motion = reinterpret_cast<GdkEventMotion*>(event);
			if (!(event_motion->state & GDK_BUTTON1_MASK))
			{
				m_window->interactive_resize_cancel();
				m_pressed = false;
				return GDK_EVENT_STOP;
			}
			m_window->interactive_resize_step(
					event_pointer(event, m_coordinate_space));

			return GDK_EVENT_STOP;
		});

	connect(m_drawing, "grab-broken-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			if (m_pressed)
			{
				// The implicit press grab ended without a valid release. Roll
				// back through the owner before accepting any later event.
				m_window->interactive_resize_cancel();
			}
			m_pressed = false;
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_drawing, "enter-notify-event",
		[this](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			gdk_window_set_cursor(gtk_widget_get_window(widget), m_cursor);
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_drawing, "leave-notify-event",
		[](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			gdk_window_set_cursor(gtk_widget_get_window(widget), nullptr);
			return GDK_EVENT_PROPAGATE;
		});

	const char* type = nullptr;
	switch (edge)
	{
	case BottomLeft:
		type = "nesw-resize";
		break;

	case Bottom:
		type = "ns-resize";
		break;

	case BottomRight:
		type = "nwse-resize";
		break;

	case Left:
		type = "ew-resize";
		break;

	case Right:
		type = "ew-resize";
		break;

	case TopLeft:
		type = "nwse-resize";
		break;

	case Top:
		type = "ns-resize";
		break;

	case TopRight:
	default:
		type = "nesw-resize";
		break;
	}
	m_cursor = gdk_cursor_new_from_name(gtk_widget_get_display(m_drawing), type);
}

//-----------------------------------------------------------------------------

Resizer::~Resizer()
{
	if (m_cursor)
	{
		g_object_unref(G_OBJECT(m_cursor));
	}
}

//-----------------------------------------------------------------------------
