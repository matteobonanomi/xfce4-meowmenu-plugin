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

#ifndef MEOWMENU_CORE_WINDOW_FRAME_H
#define MEOWMENU_CORE_WINDOW_FRAME_H

struct _cairo;
typedef struct _cairo cairo_t;
struct _GtkWidget;
typedef struct _GtkWidget GtkWidget;

namespace meow
{

/* meowmenu_clamp_corner_radius:
 * @radius: a requested corner radius in logical pixels (may be out of range).
 *
 * Clamps the requested radius into the supported window-frame range [0, 24].
 * This is the single source of truth shared by the draw path (which builds the
 * rounded clip) and the live property-changed handler, so the visible rounding
 * can never diverge from the control's range. The mapping is monotonic: a
 * larger in-range request never yields a smaller result.
 *
 * Returns: the clamped radius, always in [0, 24].
 */
int meowmenu_clamp_corner_radius(int radius);

/* meowmenu_frame_draws_border:
 * @is_fullscreen: true when the menu is in full-screen layout.
 * @supports_alpha: true when an RGBA visual / compositor is available.
 *
 * Decides whether the single rounded, composited border stroke is emitted.
 * It is drawn only for the docked, composited case: a full-screen menu reads as
 * one seamless surface (no outline), and without a compositor the rounded path
 * cannot be drawn. The non-composited docked square-border fallback is NOT this
 * predicate — that case is handled separately under its own !is_fullscreen
 * guard in the draw fallback.
 *
 * Returns: true iff (!is_fullscreen && supports_alpha).
 */
bool meowmenu_frame_draws_border(bool is_fullscreen, bool supports_alpha);

/* meowmenu_frameless_launcher_css:
 *
 * Returns the scoped GTK CSS rule that removes persistent Results viewport,
 * scrollbar, and trough chrome without selecting the scrollbar slider. The
 * returned string is static and must not be freed.
 */
const char* meowmenu_frameless_launcher_css();

/* meowmenu_clip_cairo_to_bounds:
 * @cr: widget-local Cairo context from a draw signal.
 * @width: visible Results width in logical pixels.
 * @height: visible Results height in logical pixels.
 *
 * Intersects the current draw clip with the visible Results bounds without
 * changing GTK allocation or damage geometry. The caller leaves the clip in
 * place for the widget's default draw handler and its descendants.
 *
 * Returns: true when a positive clip was applied.
 */
bool meowmenu_clip_cairo_to_bounds(cairo_t* cr, int width, int height);

/* meowmenu_draw_widget_with_bounds:
 * @widget: Results scroller whose class draw handler should run once.
 * @cr: widget-local Cairo context from the scroller's draw signal.
 *
 * Runs the widget's normal class draw handler inside its current allocation
 * bounds. Call this from a regular draw-signal handler and stop emission when
 * it returns true, since the default handler has already been invoked.
 *
 * Returns: true when the widget was drawn under the bounded clip.
 */
bool meowmenu_draw_widget_with_bounds(GtkWidget* widget, cairo_t* cr);

} // namespace meow

#endif // MEOWMENU_CORE_WINDOW_FRAME_H
