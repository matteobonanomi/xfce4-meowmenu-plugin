/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_XFCE_HELPERS_H
#define MEOWMENU_XFCE_HELPERS_H

#include <cstddef>
#include <string>

namespace WhiskerMenu
{

enum class XfceDependencyRegime
{
	Legacy,
	Successor
};

struct HistoricalSearchAction
{
	const char* pattern;
	const char* legacy_command;
	bool is_regex;
	const char* successor_command;
};

XfceDependencyRegime current_xfce_dependency_regime();
const char* xfce_opener(XfceDependencyRegime regime);
const char* xfce_desktop_item_editor(XfceDependencyRegime regime);
const char* xfce_icon_chooser_family(XfceDependencyRegime regime);

const HistoricalSearchAction* historical_search_actions(std::size_t* count);
std::string effective_search_action_command(
		XfceDependencyRegime regime,
		const std::string& pattern,
		const std::string& stored_command,
		bool is_regex);

std::string build_help_command(XfceDependencyRegime regime, const char* uri);
std::string build_file_manager_command(
		XfceDependencyRegime regime,
		const char* path);
std::string build_terminal_command(
		XfceDependencyRegime regime,
		const char* path);

}

#endif // MEOWMENU_XFCE_HELPERS_H
