/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "xfce-helpers.h"

#include <glib.h>

using namespace WhiskerMenu;

namespace
{

const HistoricalSearchAction HISTORICAL_SEARCH_ACTIONS[] = {
	{"#", "exo-open --launch TerminalEmulator man %s", false,
			"xfce-open --launch TerminalEmulator man %s"},
	{"?", "exo-open --launch WebBrowser https://duckduckgo.com/?q=%u", false,
			"xfce-open --launch WebBrowser https://duckduckgo.com/?q=%u"},
	{"!w", "exo-open --launch WebBrowser https://en.wikipedia.org/wiki/%u", false,
			"xfce-open --launch WebBrowser https://en.wikipedia.org/wiki/%u"},
	{"!", "exo-open --launch TerminalEmulator %s", false,
			"xfce-open --launch TerminalEmulator %s"},
	{"^(file|http|https):\\/\\/(.*)$", "exo-open \\0", true,
			"xfce-open \\0"}
};

/* build_quoted_command:
 * @prefix: fixed trusted command prefix.
 * @value: untrusted final argument; nullptr means an empty argument.
 *
 * Quotes the variable argument for the GLib shell parser used by launcher
 * execution, preserving it as exactly one argv element.
 *
 * Returns: the complete command line.
 */
std::string build_quoted_command(const std::string& prefix, const char* value)
{
	gchar* quoted = g_shell_quote(value ? value : "");
	std::string result = prefix;
	result += quoted;
	g_free(quoted);
	return result;
}

}

//-----------------------------------------------------------------------------

XfceDependencyRegime WhiskerMenu::current_xfce_dependency_regime()
{
#if defined(MEOWMENU_XFCE_SUCCESSOR) && MEOWMENU_XFCE_SUCCESSOR
	return XfceDependencyRegime::Successor;
#else
	return XfceDependencyRegime::Legacy;
#endif
}

//-----------------------------------------------------------------------------

const char* WhiskerMenu::xfce_opener(XfceDependencyRegime regime)
{
	return regime == XfceDependencyRegime::Successor ? "xfce-open" : "exo-open";
}

//-----------------------------------------------------------------------------

const char* WhiskerMenu::xfce_desktop_item_editor(XfceDependencyRegime regime)
{
	return regime == XfceDependencyRegime::Successor
			? "xfce-desktop-item-edit"
			: "exo-desktop-item-edit";
}

//-----------------------------------------------------------------------------

const char* WhiskerMenu::xfce_icon_chooser_family(XfceDependencyRegime regime)
{
	return regime == XfceDependencyRegime::Successor
			? "xfce-icon-chooser"
			: "exo-icon-chooser";
}

//-----------------------------------------------------------------------------

/* historical_search_actions:
 * @count: receives the number of rows; may be nullptr.
 *
 * Returns the immutable registry of built-in commands shipped before the Xfce
 * helper transition.
 *
 * Returns: borrowed process-lifetime storage.
 */
const HistoricalSearchAction* WhiskerMenu::historical_search_actions(
		std::size_t* count)
{
	if (count)
	{
		*count = G_N_ELEMENTS(HISTORICAL_SEARCH_ACTIONS);
	}
	return HISTORICAL_SEARCH_ACTIONS;
}

//-----------------------------------------------------------------------------

/* effective_search_action_command:
 * @regime: helper family selected by the build.
 * @pattern: stored action pattern.
 * @stored_command: stored command template.
 * @is_regex: stored matching mode.
 *
 * Maps only an exact historical tuple in successor builds. The returned value
 * is transient and the stored action is never modified.
 *
 * Returns: the command template to expand for this invocation.
 */
std::string WhiskerMenu::effective_search_action_command(
		XfceDependencyRegime regime,
		const std::string& pattern,
		const std::string& stored_command,
		bool is_regex)
{
	if (regime == XfceDependencyRegime::Successor)
	{
		for (const auto& action : HISTORICAL_SEARCH_ACTIONS)
		{
			if ((pattern == action.pattern)
					&& (stored_command == action.legacy_command)
					&& (is_regex == action.is_regex))
			{
				return action.successor_command;
			}
		}
	}
	return stored_command;
}

//-----------------------------------------------------------------------------

std::string WhiskerMenu::build_help_command(
		XfceDependencyRegime regime,
		const char* uri)
{
	return build_quoted_command(
			std::string(xfce_opener(regime)) + " --launch WebBrowser ",
			uri);
}

//-----------------------------------------------------------------------------

std::string WhiskerMenu::build_file_manager_command(
		XfceDependencyRegime regime,
		const char* path)
{
	return build_quoted_command(
			std::string(xfce_opener(regime)) + " --launch FileManager ",
			path);
}

//-----------------------------------------------------------------------------

std::string WhiskerMenu::build_terminal_command(
		XfceDependencyRegime regime,
		const char* path)
{
	return build_quoted_command(
			std::string(xfce_opener(regime))
					+ " --launch TerminalEmulator --working-directory ",
			path);
}
