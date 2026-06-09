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

#include "plugin.h"
#include "settings.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::resize(int delta_x, int delta_y, int delta_width, int delta_height)
{
	if (set_size(m_geometry.width + delta_width, m_geometry.height + delta_height))
	{
		check_scrollbar_needed();
	}

	if (centered_layout())
	{
		// Centered re-centres continuously: the position is recomputed from the
		// new size on each motion event instead of being translated by the
		// drag delta, so the geometric centre stays fixed (no drift, SC-003).
		// The delta_x/delta_y edge translation is intentionally ignored.
		// NOTE: if a given compositor shows jitter here, recentring only in
		// resize_end() (on drag release) is an acceptable fallback — the final
		// resting position is still centred.
		center_window();
		move_window();
	}
	else if (delta_x || delta_y)
	{
		m_geometry.x += delta_x;
		m_geometry.y += delta_y;
		move_window();
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::resize_start()
{
	m_resizing = true;
	set_child_has_focus();
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::resize_end()
{
	// Store new size (never persist fullscreen dimensions as the normal menu
	// size). Centered is windowed, so it reuses the exact Docked persistence
	// path — the resized width/height are saved to /menu-width and /menu-height.
	if (g_strcmp0(m_settings->layout_mode, "fullscreen") != 0)
	{
		m_settings->menu_width = m_geometry.width;
		m_settings->menu_height = m_geometry.height;
	}

	// Move window back to panel button or center of screen. Centered always
	// settles at the monitor centre regardless of the launch trigger.
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

	// Allow menu to hide
	m_resizing = false;
	m_child_has_focus = false;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::Window::set_size(int width, int height)
{
	bool resized = false;
	width = CLAMP(width, 10, m_monitor.width);
	height = CLAMP(height, 10, m_monitor.height);
	if ((m_geometry.width != width) || (m_geometry.height != height))
	{
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
