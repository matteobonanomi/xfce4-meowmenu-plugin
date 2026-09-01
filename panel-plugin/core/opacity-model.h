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

#ifndef MEOWMENU_CORE_OPACITY_MODEL_H
#define MEOWMENU_CORE_OPACITY_MODEL_H

/*
 * Pure, GTK-free model for the single menu-opacity control.
 *
 * Rendering contract (0 = fully transparent, 100 = fully solid):
 *   - The control is one integer percentage in [0, 100] (the /menu-opacity key).
 *   - alpha = clamp(value, 0, 100) / 100.0, so 0 -> 0.0 and 100 -> 1.0,
 *     strictly monotonic with no internal floor or ceiling (no dead zone).
 *   - That one alpha paints the baseline and source-replaced semantic surfaces.
 *     Foreground widgets stay opaque and adjacent surfaces never compound.
 *
 * This translation unit deliberately includes no GTK headers so the contract
 * can be unit-tested headlessly, mirroring the sidebar-layout / window-size-clamp
 * helpers.
 */

/* meowmenu_opacity_alpha:
 * @value: an opacity percentage; values outside [0, 100] are clamped.
 *
 * Maps a stored opacity percentage to a CSS alpha in [0.0, 1.0] as
 * clamp(value, 0, 100) / 100.0.
 *
 * Returns: the alpha; 0.0 at value <= 0, 1.0 at value >= 100.
 */
double meowmenu_opacity_alpha(int value);

/* meowmenu_background_translucent:
 * @menu_opacity: the single menu-opacity percentage; values outside [0, 100]
 *                are clamped, mirroring meowmenu_opacity_alpha.
 *
 * Pure predicate deciding whether the menu background is translucent for a
 * given opacity, i.e. whether its resolved alpha is below 1.0 (equivalently
 * menu_opacity < 100). The launcher views gate their full-redraw safeguard on
 * this: the fully-opaque path pays nothing, while a translucent background
 * recomposites the whole result surface so no stale highlight pixels survive.
 *
 * Returns: true iff the resolved alpha is strictly less than 1.0.
 */
bool meowmenu_background_translucent(int menu_opacity);

/* meowmenu_effective_background_alpha:
 * @menu_opacity: the configured percentage.
 * @composited: whether an RGBA compositor path is available.
 *
 * Applies the solid fallback used without compositing while preserving the
 * configured alpha on the composited paint path.
 *
 * Returns: 1.0 without compositing, otherwise the clamped configured alpha.
 */
double meowmenu_effective_background_alpha(int menu_opacity, bool composited);

/* Buffer size that is always sufficient to hold the textual form produced by
 * meowmenu_format_css_alpha (a fixed-3-decimal rendering such as "0.600",
 * plus its NUL terminator). */
#define MEOWMENU_CSS_ALPHA_BUFSZ 16

/* meowmenu_format_css_alpha:
 * @alpha: a CSS alpha in [0.0, 1.0] (callers pass the value from
 *         meowmenu_opacity_alpha; out-of-range input is formatted as given).
 * @out:   caller-owned buffer receiving a NUL-terminated, locale-independent
 *         fixed-3-decimal rendering (e.g. "0.600"); MEOWMENU_CSS_ALPHA_BUFSZ
 *         bytes is always sufficient.
 *
 * Formats @alpha exactly as printf "%.3f" would under the "C" locale — the
 * decimal separator is always '.', never a locale-dependent ','. Reads and
 * mutates no global locale state, so it is safe to call from a process whose
 * LC_NUMERIC uses a comma and that hosts other plugins.
 *
 * Returns: @out, for call-site convenience.
 */
const char* meowmenu_format_css_alpha(double alpha, char* out /* [MEOWMENU_CSS_ALPHA_BUFSZ] */);

#endif // MEOWMENU_CORE_OPACITY_MODEL_H
