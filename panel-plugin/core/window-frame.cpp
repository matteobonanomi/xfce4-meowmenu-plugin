/*
 * Copyright (C) 2026 MeowMenu contributors
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

#include "window-frame.h"

#include <gtk/gtk.h>

namespace meow
{

// Supported corner-radius range. Mirrors the /corner-radius control range so
// the visible rounding always tracks what the GUI can request.
namespace
{
const int kMinCornerRadius = 0;
const int kMaxCornerRadius = 24;
} // namespace

int meowmenu_clamp_corner_radius(int radius)
{
	if (radius < kMinCornerRadius)
		return kMinCornerRadius;
	if (radius > kMaxCornerRadius)
		return kMaxCornerRadius;
	return radius;
}

bool meowmenu_frame_draws_border(bool is_fullscreen, bool supports_alpha)
{
	// Full-screen reads as one seamless surface (no outline); the composited
	// rounded stroke also needs an RGBA visual to be drawn at all.
	return !is_fullscreen && supports_alpha;
}

/* meowmenu_frameless_launcher_css:
 *
 * Keeps the complete Results scrollbar chrome transparent while deliberately
 * leaving its slider outside the rule.
 *
 * Returns: static CSS owned by this module.
 */
const char* meowmenu_frameless_launcher_css()
{
	// Themes may paint the hairline on the scrollbar rather than its trough.
	return
			".meowmenu scrolledwindow.launchers-pane,"
			".meowmenu scrolledwindow.launchers-pane > viewport,"
			".meowmenu scrolledwindow.launchers-pane scrollbar,"
			".meowmenu scrolledwindow.launchers-pane scrollbar trough"
			"{ background-color: transparent; background-image: none;"
			"  border: none; outline: none; box-shadow: none; }";
}

const char* meowmenu_list_selection_css()
{
	return
			".meowmenu treeview.launchers.view:selected,"
			".meowmenu treeview.launchers.view:selected:focus"
			"{ background-color: @theme_selected_bg_color;"
			"  background-image: none;"
			"  color: @theme_selected_fg_color; }";
}

bool meowmenu_queue_complete_window_frame(GtkWidget* widget)
{
	if (!GTK_IS_WIDGET(widget))
		return false;
	gtk_widget_queue_draw(widget);
	return true;
}

} // namespace meow
