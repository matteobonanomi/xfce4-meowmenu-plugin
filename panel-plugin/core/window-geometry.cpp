/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
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

#include "window-geometry.h"
#include "window.h"

#include "launcher/page.h"
#include "places/places-page.h"
#include "plugin.h"
#include "settings.h"
#include "ui/grid-cell-metrics.h"

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* Window::interactive_resize_begin:
 * @direction: selected edge or corner.
 * @policy: live X11 or release-to-apply fallback behavior.
 * @pointer: event-time press coordinate in the policy's stable space.
 *
 * Captures the exact current rectangle, stable layout anchor, size limits,
 * saved normal size, and display geometry.
 *
 * Returns: true when a new transaction starts.
 */
bool WhiskerMenu::Window::interactive_resize_begin(
		InteractiveResize::Direction direction,
		InteractiveResize::BackendPolicy policy,
		const InteractiveResize::PointerSample& pointer)
{
	using namespace InteractiveResize;

	if (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0)
		return false;

	GdkDisplay* gdk_display = gtk_widget_get_display(GTK_WIDGET(m_window));
	GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(m_window));
	GdkMonitor* gdk_monitor = gdk_window
			? gdk_display_get_monitor_at_window(gdk_display, gdk_window)
			: nullptr;
	if (!gdk_monitor)
	{
		gdk_monitor = gdk_display_get_monitor_at_point(
				gdk_display,
				m_geometry.x + (m_geometry.width / 2),
				m_geometry.y + (m_geometry.height / 2));
	}
	if (!gdk_monitor)
		return false;

	const DisplaySignature display = resize_display_signature(gdk_monitor);
	const Rectangle rectangle = {
		m_geometry.x,
		m_geometry.y,
		m_geometry.width,
		m_geometry.height
	};
	const Rectangle monitor = {
		display.geometry.x,
		display.geometry.y,
		display.geometry.width,
		display.geometry.height
	};
	const DirectionAxes axes = direction_axes(direction);

	int maximum_width = monitor.width;
	int maximum_height = monitor.height;
	if (!centered_layout())
	{
		if (axes.horizontal < 0)
			maximum_width = rectangle.x + rectangle.width - monitor.x;
		else if (axes.horizontal > 0)
			maximum_width = monitor.x + monitor.width - rectangle.x;

		if (axes.vertical < 0)
			maximum_height = rectangle.y + rectangle.height - monitor.y;
		else if (axes.vertical > 0)
			maximum_height = monitor.y + monitor.height - rectangle.y;
	}
	int minimum_width = 10;
	int minimum_height = 10;
	int content_minimum = 0;
	gtk_widget_get_preferred_width(
			GTK_WIDGET(m_frame), &content_minimum, nullptr);
	if (m_places_active)
	{
		content_minimum = meow_grid_release_resize_minimum(
				content_minimum,
				m_places->get_viewport_width(),
				m_places->get_minimum_viewport_width());
	}
	else if (Page* page = get_active_page())
	{
		content_minimum = meow_grid_release_resize_minimum(
				content_minimum,
				page->get_viewport_width(),
				page->get_minimum_viewport_width());
	}
	minimum_width = CLAMP(content_minimum, 10, maximum_width);
	gtk_widget_get_preferred_height(
			GTK_WIDGET(m_frame), &content_minimum, nullptr);
	minimum_height = CLAMP(content_minimum, 10, maximum_height);
	const Bounds bounds = {
		{minimum_width, maximum_width},
		{minimum_height, maximum_height}
	};
	const Anchor anchor = centered_layout()
			? monitor_center_anchor(monitor)
			: opposite_edge_anchor(rectangle);
	const SavedNormalSize saved = {
		m_settings->menu_width,
		m_settings->menu_height
	};
	if (!m_resize_transaction.begin(
			direction,
			policy,
			rectangle,
			pointer,
			bounds,
			anchor,
			saved,
			display,
			g_strcmp0(m_settings->layout_mode, "fullscreen") != 0))
	{
		return false;
	}

	if (policy == InteractiveResize::BackendPolicy::X11Live)
		set_results_interactive_resize(true);
	start_resize_display_watch(gdk_monitor);
	m_resizing = true;
	set_child_has_focus();
	return true;
}

//-----------------------------------------------------------------------------

/* Window::apply_resize_rectangle:
 * @rectangle: absolute geometry accepted by the resize transaction.
 *
 * Applies transaction geometry directly. Panel gaps and monitor clamps are
 * deliberately absent here because both were already frozen into the
 * transaction's starting anchor and bounds.
 */
void WhiskerMenu::Window::apply_resize_rectangle(
		const InteractiveResize::Rectangle& rectangle)
{
	const bool size_changed = set_size(rectangle.width, rectangle.height);
	m_geometry.x = rectangle.x;
	m_geometry.y = rectangle.y;

#ifdef HAVE_GTK_LAYER_SHELL
	if (gtk_layer_is_supported())
	{
		gtk_layer_set_margin(m_window, GTK_LAYER_SHELL_EDGE_LEFT,
				m_geometry.x - m_monitor.x);
		gtk_layer_set_margin(m_window, GTK_LAYER_SHELL_EDGE_TOP,
				m_geometry.y - m_monitor.y);
	}
	else
#endif
	{
		gtk_window_move(m_window, m_geometry.x, m_geometry.y);
	}

	if (size_changed)
		check_scrollbar_needed();
}

//-----------------------------------------------------------------------------

/* Window::interactive_resize_step:
 * @pointer: next event-time pointer coordinate.
 *
 * Advances the transaction. X11 applies accepted geometry immediately, while
 * release-to-apply policies leave GTK untouched until completion. Icon grids
 * keep stable preview cells so immediate delivery stays lightweight.
 *
 * Returns: true when the active transaction accepted the sample.
 */
bool WhiskerMenu::Window::interactive_resize_step(
		const InteractiveResize::PointerSample& pointer)
{
	InteractiveResize::Rectangle accepted = {};
	if (!m_resize_transaction.motion(pointer, &accepted))
		return false;
	if (m_resize_transaction.policy()
			== InteractiveResize::BackendPolicy::X11Live)
	{
		apply_resize_rectangle(accepted);
	}
	return true;
}

//-----------------------------------------------------------------------------

/* Window::settle_resize_position:
 *
 * Resolves the completed rectangle through the established centered or
 * panel-relative placement path. This applies a configured panel gap once,
 * from a freshly resolved panel position.
 */
void WhiskerMenu::Window::settle_resize_position()
{
	if (centered_layout())
	{
		center_window();
	}
	else if (m_position == PositionAtButton)
	{
		m_plugin->get_menu_position(&m_geometry.x, &m_geometry.y);
	}
	else if (m_position == PositionAtCenter)
	{
		center_window();
	}
	move_window();
}

//-----------------------------------------------------------------------------

/* Window::interactive_resize_complete:
 * @pointer: final primary-button release coordinate.
 *
 * Consumes the release, disconnects display watches, applies and settles the
 * final rectangle, then persists the normal size exactly once.
 *
 * Returns: true for a completed transaction, including repeated calls.
 */
bool WhiskerMenu::Window::interactive_resize_complete(
		const InteractiveResize::PointerSample& pointer)
{
	if (m_resize_transaction.lifecycle()
			== InteractiveResize::Lifecycle::Completed)
	{
		return true;
	}
	if (!m_resize_transaction.active())
		return false;

	InteractiveResize::Rectangle accepted = {};
	InteractiveResize::SavedNormalSize saved = {};
	if (!m_resize_transaction.complete(pointer, &accepted, &saved))
		return false;

	stop_resize_display_watch();
	apply_resize_rectangle(accepted);
	set_results_interactive_resize(false);
	m_settings->menu_width = saved.width;
	m_settings->menu_height = saved.height;
	settle_resize_position();
	m_resizing = false;
	m_child_has_focus = false;
	return true;
}

//-----------------------------------------------------------------------------

/* Window::interactive_resize_cancel:
 *
 * Clears every handle, disconnects display watches, and restores the exact
 * pre-drag rectangle without assigning normal-size settings.
 *
 * Returns: true for a cancelled transaction, including repeated calls.
 */
bool WhiskerMenu::Window::interactive_resize_cancel()
{
	clear_resize_handles();
	if (m_resize_transaction.lifecycle()
			== InteractiveResize::Lifecycle::Cancelled)
	{
		return true;
	}
	if (!m_resize_transaction.active())
		return false;

	InteractiveResize::Rectangle restored = {};
	InteractiveResize::SavedNormalSize saved = {};
	if (!m_resize_transaction.cancel(&restored, &saved))
		return false;

	stop_resize_display_watch();
	apply_resize_rectangle(restored);
	set_results_interactive_resize(false);
	// Motion never changes Xfconf. The snapshot is returned for contract
	// verification, but rollback deliberately performs no settings assignment.
	(void)saved;
	m_resizing = false;
	m_child_has_focus = false;
	return true;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::Window::set_size(int width, int height)
{
	bool resized = false;
	width = CLAMP(width, 10, m_monitor.width);
	height = CLAMP(height, 10, m_monitor.height);
	if ((m_geometry.width != width) || (m_geometry.height != height))
	{
		const int current_width = m_geometry.width;
		if (current_width > 0 && current_width != width)
		{
			prepare_results_width_resize(current_width, width);
		}
		m_geometry.width = width;
		m_geometry.height = height;
		gtk_widget_set_size_request(GTK_WIDGET(m_window), m_geometry.width, m_geometry.height);
		gtk_window_resize(m_window, 1, 1);
		resized = true;
	}
	return resized;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::check_scrollbar_needed()
{
	// Find height of sidebar and buttons
	int buttons_height = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_category_buttons), nullptr, &buttons_height);

	int sidebar_height = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_sidebar), nullptr, &sidebar_height);

	// Always show scrollbar if sidebar is shorter than buttons
	if (sidebar_height >= buttons_height)
	{
		gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
	else
	{
		gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	}
}

//-----------------------------------------------------------------------------
