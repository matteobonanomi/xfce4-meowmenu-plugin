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
 * Pure, GTK-free model for the menu's three opacity controls.
 *
 * Rendering contract (0 = fully transparent, 100 = fully solid):
 *   - Each control is an integer percentage in [0, 100].
 *   - alpha = clamp(value, 0, 100) / 100.0, so 0 -> 0.0 and 100 -> 1.0,
 *     strictly monotonic with no internal floor or ceiling (no dead zone).
 *   - A region's visible alpha is exactly the alpha of its single governing
 *     control. Regions never compound: the renderer must not place one opaque
 *     background behind another so the two combine into 1-(1-a)(1-b).
 *
 * This translation unit deliberately includes no GTK headers so the contract
 * can be unit-tested headlessly, mirroring the sidebar-layout / window-size-clamp
 * helpers.
 */

/* The single alpha assigned to each menu surface region for one render pass. */
struct OpacityRegionAlphas
{
	double window;     /* the .meowmenu window shell background */
	double categories; /* the sidebar/categories region (and its chrome strips) */
	double apps;       /* the results / applications area */
};

/* meowmenu_opacity_alpha:
 * @value: an opacity percentage; values outside [0, 100] are clamped.
 *
 * Maps a stored opacity percentage to a CSS alpha in [0.0, 1.0] as
 * clamp(value, 0, 100) / 100.0.
 *
 * Returns: the alpha; 0.0 at value <= 0, 1.0 at value >= 100.
 */
double meowmenu_opacity_alpha(int value);

/* meowmenu_region_alphas:
 * @fullscreen: true when the menu is in full-screen layout mode.
 * @categories_opacity: the sidebar/categories control value (docked only).
 * @apps_opacity: the results-area control value (docked only).
 * @full_screen_opacity: the single full-screen control value (full-screen only).
 *
 * Resolves which absolute alpha each region receives so that exactly one
 * control governs any given region (no inter-region compounding). In
 * full-screen the window shell owns the single full-screen alpha and the
 * regions are transparent; in docked mode the window shell is transparent and
 * each region owns its own absolute alpha.
 *
 * Returns: the per-region alphas for this render pass.
 */
OpacityRegionAlphas meowmenu_region_alphas(bool fullscreen,
                                           int categories_opacity,
                                           int apps_opacity,
                                           int full_screen_opacity);

#endif // MEOWMENU_CORE_OPACITY_MODEL_H
