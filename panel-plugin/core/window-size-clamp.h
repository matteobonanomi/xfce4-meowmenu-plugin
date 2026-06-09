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

#ifndef MEOWMENU_CORE_WINDOW_SIZE_CLAMP_H
#define MEOWMENU_CORE_WINDOW_SIZE_CLAMP_H

#include <gdk/gdk.h>

namespace meow
{

/* clamp_default_size:
 * @desired_w / @desired_h: the window's intended default size in *logical*
 *   pixels (e.g. 820x600 for the preferences window).
 * @workarea_w / @workarea_h: the active monitor's usable area in logical
 *   pixels; pass 0 (or negative) on an axis to mean "no constraint on that
 *   axis" (e.g. the monitor could not be resolved).
 * @out_w / @out_h: receive the clamped size; never NULL.
 *
 * Shrinks @desired_* so the window never exceeds the work area, leaving a small
 * margin so the window is not flush to the screen edge. Never enlarges: at
 * realistic 1x work areas the output equals the input, guaranteeing no change
 * to the standard-DPI layout. Output is always >= 1 on each axis when the
 * corresponding desired input is >= 1.
 */
void clamp_default_size(int desired_w, int desired_h,
                        int workarea_w, int workarea_h,
                        int* out_w, int* out_h);

/* centered_origin:
 * @monitor: the target monitor in absolute root-window coordinates (full
 *   monitor geometry, struts included — NOT the work area). Its x/y origin
 *   may be non-zero on a multi-monitor layout.
 * @win_w / @win_h: the intended window size in logical pixels.
 * @out_x / @out_y: receive the top-left at which the window must be placed so
 *   its geometric centre coincides with the monitor centre; never NULL.
 *
 * Pure geometry. @win_w/@win_h are first clamped to the monitor so a window
 * larger than the screen can never overflow it, then the top-left is computed
 * as monitor.origin + (monitor.size - clamped_size)/2. The result is therefore
 * always fully on-monitor and centred, and depends only on the size and the
 * monitor — never on any previous position — so repeated calls across a resize
 * keep the centre fixed (no drift).
 */
void centered_origin(const GdkRectangle& monitor, int win_w, int win_h,
                     int* out_x, int* out_y);

} // namespace meow

#endif // MEOWMENU_CORE_WINDOW_SIZE_CLAMP_H
