/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "home-shortcuts.h"

#include <glib.h>

#include <cstdlib>
#include <set>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* qualify_home_shortcuts:
 * @home: dedicated Home path; may be NULL.
 * @standard_directories: provider-ordered standard-directory candidates.
 *
 * Produces the stable Places Home shortcut order while filtering invalid,
 * missing, Home-equivalent, and duplicate paths. The function performs only
 * synchronous bounded qualification over the supplied candidates; it neither
 * scans Home nor mutates GLib's process-global directory cache.
 *
 * Returns: owned path strings with Home first and every path unique.
 */
std::vector<std::string> WhiskerMenu::qualify_home_shortcuts(const char* home,
		const std::vector<const char*>& standard_directories)
{
	std::vector<std::string> result;
	std::set<std::string> included;

	auto append = [&result, &included](const char* path)
	{
		if (!path || !*path || !g_file_test(path, G_FILE_TEST_IS_DIR))
		{
			return;
		}

		char* canonical = realpath(path, nullptr);
		if (!canonical)
		{
			return;
		}
		const std::string identity(canonical);
		std::free(canonical);
		if (!included.insert(identity).second)
		{
			return;
		}
		result.emplace_back(path);
	};

	append(home);
	for (const char* path : standard_directories)
	{
		append(path);
	}
	return result;
}
