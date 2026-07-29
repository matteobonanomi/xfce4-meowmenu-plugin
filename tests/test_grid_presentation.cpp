/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ui/grid-presentation.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace WhiskerMenu;

int main()
{
	int failures = 0;
	for (int opacity : { 0, 1, 99, 100 })
	{
		for (LauncherViewKind kind : {
				LauncherViewKind::IconGrid,
				LauncherViewKind::List,
				LauncherViewKind::Tree })
		{
			for (bool transparent : { false, true })
			{
				const bool expected = opacity < 100
						|| (kind == LauncherViewKind::IconGrid && transparent);
				if (full_redraw_safeguard_required(opacity, kind,
						transparent) != expected)
				{
					std::fprintf(stderr,
							"FAIL opacity=%d kind=%d transparent=%d\n",
							opacity, static_cast<int>(kind), transparent);
					++failures;
				}
			}
		}
	}

	if (failures != 0)
	{
		return EXIT_FAILURE;
	}
	std::printf("test_grid_presentation: ok\n");
	return EXIT_SUCCESS;
}
