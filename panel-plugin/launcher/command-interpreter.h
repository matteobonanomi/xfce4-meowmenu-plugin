/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_LAUNCHER_COMMAND_INTERPRETER_H
#define MEOWMENU_LAUNCHER_COMMAND_INTERPRETER_H

#include <string>

#include <glib.h>

namespace WhiskerMenu
{

enum class CommandInterpretationFailure
{
	None,
	Empty,
	Parse
};

/* CommandInterpretation:
 *
 * Owns the GLib shell parsing result shared by command availability and launch.
 * It deliberately performs no expansion or shell execution; executable lookup
 * remains a separate observation so disappearance before launch still reaches
 * the existing spawn-error path.
 */
class CommandInterpretation
{
public:
	CommandInterpretation();
	~CommandInterpretation();
	CommandInterpretation(CommandInterpretation&& other) noexcept;
	CommandInterpretation& operator=(CommandInterpretation&& other) noexcept;

	CommandInterpretation(const CommandInterpretation&) = delete;
	CommandInterpretation& operator=(const CommandInterpretation&) = delete;

	/* parse:
	 * @command_line: GLib shell command line; nullptr is an empty input.
	 *
	 * Parses once and retains the exact argv boundary and any structured GLib
	 * parse failure for later availability and launch decisions.
	 *
	 * Returns: an owned interpretation, valid or failed.
	 */
	static CommandInterpretation parse(const char* command_line);

	bool valid() const { return m_failure == CommandInterpretationFailure::None; }
	bool available() const;
	CommandInterpretationFailure failure() const { return m_failure; }
	const std::string& command_line() const { return m_command_line; }
	int argument_count() const { return m_argc; }
	const char* argument(int index) const;
	gchar** argv() const { return m_argv; }
	GError* copy_error() const;

private:
	void clear();

private:
	std::string m_command_line;
	gchar** m_argv;
	GError* m_error;
	int m_argc;
	CommandInterpretationFailure m_failure;
};

/* spawn_session_command_async:
 * @interpretation: valid parsed command owned by the caller.
 * @error: optional return location for a GLib parse or spawn error.
 *
 * Preserves g_spawn_command_line_async semantics after parsing has already
 * happened: inherited directory/environment, PATH search, no other flags, and
 * fire-and-forget child handling.
 *
 * Returns: true when the child was started.
 */
bool spawn_session_command_async(const CommandInterpretation& interpretation,
		GError** error);

}

#endif // MEOWMENU_LAUNCHER_COMMAND_INTERPRETER_H
