/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "layout-mode.h"

#include <cstring>

using namespace WhiskerMenu;

LayoutMode WhiskerMenu::layout_mode_from_key(const char* value)
{
	if (value && std::strcmp(value, "centered") == 0)
		return LayoutMode::Centered;
	if (value && std::strcmp(value, "fullscreen") == 0)
		return LayoutMode::FullScreen;
	return LayoutMode::Docked;
}

bool WhiskerMenu::control_enabled(LayoutControl control, LayoutMode mode)
{
	switch (control)
	{
	case LayoutControl::MenuWidth:
	case LayoutControl::MenuHeight:
	case LayoutControl::CornerRadius:
		return mode != LayoutMode::FullScreen;
	case LayoutControl::PanelGap:
		return mode == LayoutMode::Docked;
	}
	return false;
}
