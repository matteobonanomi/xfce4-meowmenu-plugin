/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_GRID_PRESENTATION_H
#define WHISKERMENU_GRID_PRESENTATION_H

namespace WhiskerMenu
{

enum class LauncherViewKind
{
	IconGrid,
	List,
	Tree
};

bool full_redraw_safeguard_required(int menu_opacity,
		LauncherViewKind view_kind, bool transparent_grid);

}

#endif // WHISKERMENU_GRID_PRESENTATION_H
