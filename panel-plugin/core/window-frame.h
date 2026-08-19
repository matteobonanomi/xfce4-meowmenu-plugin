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

/* meowmenu_queue_complete_window_frame:
 * @widget: launcher toplevel after a complete result-model publication.
 *
 * Invalidates the complete composed window once. Child-only damage is not
 * sufficient for the launcher's manual root-child propagation when a hidden
 * loading stack is replaced before the first map.
 *
 * Returns: true when a valid widget was queued.
 */
bool meowmenu_queue_complete_window_frame(GtkWidget* widget);

} // namespace meow

#endif // MEOWMENU_CORE_WINDOW_FRAME_H
