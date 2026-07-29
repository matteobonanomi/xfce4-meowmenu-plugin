/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "grid-presentation.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* full_redraw_safeguard_required:
 * @menu_opacity: current menu opacity percentage.
 * @view_kind: concrete result-view family.
 * @transparent_grid: whether transparent resting grid cells are enabled.
 *
 * Keeps the existing translucent-menu safeguard and extends it to transparent
 * icon grids at full opacity. List and tree views remain unchanged at 100%.
 *
 * Returns: true when navigation and scrolling must queue a full-view redraw.
 */
bool WhiskerMenu::full_redraw_safeguard_required(int menu_opacity,
		LauncherViewKind view_kind, bool transparent_grid)
{
	return menu_opacity < 100
			|| (view_kind == LauncherViewKind::IconGrid && transparent_grid);
}
