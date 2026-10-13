/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "preset-sync.h"

using namespace WhiskerMenu;

const std::vector<PresetSyncDescriptor>& WhiskerMenu::preset_sync_descriptors()
{
	static const std::vector<PresetSyncDescriptor> descriptors = {
		{ "corner-radius", PresetSyncControl::CornerRadius },
		{ "panel-gap", PresetSyncControl::PanelGap },
		{ "menu-opacity", PresetSyncControl::MenuOpacity },
		{ "sidebar-position", PresetSyncControl::SidebarPosition },
		{ "sidebar-enabled", PresetSyncControl::SidebarEnabled },
		{ "category-show-name", PresetSyncControl::CategoryShowName },
		{ "search-bar-position", PresetSyncControl::SearchBarPosition },
		{ "show-profile", PresetSyncControl::ShowProfile },
		{ "show-session", PresetSyncControl::ShowSession },
		{ "layout-mode", PresetSyncControl::LayoutMode },
		{ "launcher-icon-size", PresetSyncControl::LauncherIconSize },
		{ "category-icon-size", PresetSyncControl::CategoryIconSize },
		{ "hover-switch-category", PresetSyncControl::HoverSwitchCategory },
		{ "view-mode-default", PresetSyncControl::ViewModeDefault },
		{ "default-category", PresetSyncControl::DefaultCategory },
		{ "stay-on-focus-out", PresetSyncControl::StayOnFocusOut },
		{ "places-enabled", PresetSyncControl::PlacesEnabled },
		{ "places-show-icons", PresetSyncControl::PlacesShowIcons },
		{ "calculator-engine", PresetSyncControl::CalculatorEngine },
		{ "calculator-result-font-size", PresetSyncControl::CalculatorResultFontSize },
		{ "calculator-max-decimal-places", PresetSyncControl::CalculatorMaxDecimalPlaces },
	};
	return descriptors;
}
