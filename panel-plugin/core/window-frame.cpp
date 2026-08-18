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

/* meowmenu_clip_cairo_to_bounds:
 * @cr: widget-local Cairo context from a draw signal.
 * @width: visible Results width in logical pixels.
 * @height: visible Results height in logical pixels.
 *
 * Bounds the current drawing transaction without mutating GtkWidget clip
 * metadata. GTK uses that metadata as parent-coordinate damage geometry, so
 * narrowing it manually can both constrain grid allocation and invalidate the
 * wrong region after scrolling.
 *
 * Returns: true when a positive clip was applied.
 */
bool meowmenu_clip_cairo_to_bounds(cairo_t* cr, int width, int height)
{
	if (!cr || width <= 0 || height <= 0)
		return false;
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_clip(cr);
	return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

/* meowmenu_draw_widget_with_bounds:
 * @widget: Results scroller whose class draw handler should run once.
 * @cr: widget-local Cairo context from the scroller's draw signal.
 *
 * Invokes the class handler while the local Results rectangle is active. GTK
 * restores signal-handler Cairo state before its default draw handler runs, so
 * drawing must happen inside this transaction rather than after returning.
 *
 * Returns: true when the class handler was invoked under the bounded clip.
 */
bool meowmenu_draw_widget_with_bounds(GtkWidget* widget, cairo_t* cr)
{
	if (!GTK_IS_WIDGET(widget) || !cr)
		return false;
	GtkWidgetClass* widget_class = GTK_WIDGET_GET_CLASS(widget);
	if (!widget_class || !widget_class->draw)
		return false;
	cairo_save(cr);
	const bool clipped = meowmenu_clip_cairo_to_bounds(cr,
			gtk_widget_get_allocated_width(widget),
			gtk_widget_get_allocated_height(widget));
	if (clipped)
		widget_class->draw(widget, cr);
	cairo_restore(cr);
	return clipped;
}

} // namespace meow
