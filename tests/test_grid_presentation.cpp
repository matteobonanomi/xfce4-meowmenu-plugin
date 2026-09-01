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
	struct PresetSurface
	{
		const char* name;
		int opacity;
		LauncherViewKind kind;
		bool transparent;
		bool sidebar;
		bool hover;
		bool places;
		int default_category;
		bool expected_safeguard;
	};
	const PresetSurface presets[] = {
		{ "Classic", 100, LauncherViewKind::List, false,
				true, false, false, 0, false },
		{ "Modern", 100, LauncherViewKind::IconGrid, true,
				true, true, true, 1, true },
		{ "Full Screen", 80, LauncherViewKind::IconGrid, true,
				true, true, true, 2, true },
		{ "Minimal", 60, LauncherViewKind::List, false,
				false, true, true, 1, true },
	};
	for (const PresetSurface& preset : presets)
	{
		(void)preset.sidebar;
		(void)preset.hover;
		(void)preset.places;
		(void)preset.default_category;
		if (full_redraw_safeguard_required(preset.opacity, preset.kind,
				preset.transparent) != preset.expected_safeguard)
		{
			std::fprintf(stderr, "FAIL preset=%s\n", preset.name);
			++failures;
		}
	}

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
