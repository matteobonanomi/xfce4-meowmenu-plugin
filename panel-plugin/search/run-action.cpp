/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "run-action.h"

#include "query.h"
#include "settings.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

bool WhiskerMenu::run_action_command_is_available(const char* command_line)
{
	if (!command_line)
		return false;
	gchar** argv = nullptr;
	if (!g_shell_parse_argv(command_line, nullptr, &argv, nullptr))
		return false;
	gchar* path = g_find_program_in_path(argv[0]);
	const bool available = path != nullptr;
	g_free(path);
	g_strfreev(argv);
	return available;
}

//-----------------------------------------------------------------------------

RunAction::RunAction(Settings* settings) :
	m_settings(settings)
{
	set_icon("system-run");
}

//-----------------------------------------------------------------------------

void RunAction::run(GdkScreen* screen) const
{
	spawn(screen, m_command_line.c_str(), nullptr, false, nullptr);
}

//-----------------------------------------------------------------------------

unsigned int RunAction::search(const Query& query)
{
	// Check if in PATH
	if (!run_action_command_is_available(query.raw_query().c_str()))
	{
		return UINT_MAX;
	}

	m_command_line = query.raw_query();

	// Set item text
	const gchar* direction = (gtk_widget_get_default_direction() != GTK_TEXT_DIR_RTL) ? "\342\200\216" : "\342\200\217";
	gchar* display_name = g_strdup_printf(_("Run %s"), m_command_line.c_str());
	if (m_settings->launcher_show_description && (m_settings->view_mode != Settings::ViewAsIcons))
	{
		set_text(g_markup_printf_escaped("%s<b>%s</b>\n", direction, display_name));
	}
	else
	{
		set_text(g_markup_printf_escaped("%s%s", direction, display_name));
	}
	set_tooltip(display_name);
	g_free(display_name);

	// Sort after matches in names and before matches in executables
	return 0xFFF;
}

//-----------------------------------------------------------------------------
