/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_HOME_SHORTCUTS_H
#define WHISKERMENU_HOME_SHORTCUTS_H

#include <string>
#include <vector>

namespace WhiskerMenu
{

std::vector<std::string> qualify_home_shortcuts(const char* home,
		const std::vector<const char*>& standard_directories);

}

#endif // WHISKERMENU_HOME_SHORTCUTS_H
