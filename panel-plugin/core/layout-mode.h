/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_CORE_LAYOUT_MODE_H
#define MEOWMENU_CORE_LAYOUT_MODE_H

namespace WhiskerMenu
{

enum class LayoutMode
{
	Docked,
	Centered,
	FullScreen
};

/* layout_mode_from_key:
 * @value: raw /layout-mode value; may be NULL.
 *
 * Classifies the stored layout without mutating it. Unknown values use the
 * safe Docked behavior.
 *
 * Returns: the resolved layout mode.
 */
LayoutMode layout_mode_from_key(const char* value);

enum class LayoutControl
{
	MenuWidth,
	MenuHeight,
	PanelGap,
	CornerRadius
};

/* control_enabled:
 * @control: layout-sensitive Properties control.
 * @mode: current layout mode.
 *
 * Returns: true when @control applies to @mode.
 */
bool control_enabled(LayoutControl control, LayoutMode mode);

}

#endif // MEOWMENU_CORE_LAYOUT_MODE_H
