/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_PRESET_SYNC_H
#define WHISKERMENU_PRESET_SYNC_H

#include <vector>

namespace WhiskerMenu
{

enum class PresetSyncControl
{
	CornerRadius,
	PanelGap,
	MenuOpacity,
	SidebarPosition,
	SidebarEnabled,
	CategoryShowName,
	SearchBarPosition,
	ShowProfile,
	ShowSession,
	LayoutMode,
	LauncherIconSize,
	CategoryIconSize,
	HoverSwitchCategory,
	ViewModeDefault,
	DefaultCategory,
	StayOnFocusOut,
	PlacesEnabled,
	PlacesShowIcons,
	CalculatorEngine,
	CalculatorResultFontSize,
	CalculatorMaxDecimalPlaces,
};

struct PresetSyncDescriptor
{
	const char* key;
	PresetSyncControl control;
};

/* preset_sync_descriptors:
 *
 * Returns the dialog-owned mapping from preset keys to the concrete controls
 * refreshed after applying a preset. The list has process lifetime and is
 * intentionally independent of the preset definition table.
 *
 * Returns: a reference to the process-lifetime descriptor list.
 */
const std::vector<PresetSyncDescriptor>& preset_sync_descriptors();

} // namespace WhiskerMenu

#endif // WHISKERMENU_PRESET_SYNC_H
