/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "command-interpreter.h"

#include <utility>

using namespace WhiskerMenu;

CommandInterpretation::CommandInterpretation() :
	m_argv(nullptr),
	m_error(nullptr),
	m_argc(0),
	m_failure(CommandInterpretationFailure::Empty)
{
}

CommandInterpretation::~CommandInterpretation()
{
	clear();
}

CommandInterpretation::CommandInterpretation(
		CommandInterpretation&& other) noexcept :
	m_command_line(std::move(other.m_command_line)),
	m_argv(other.m_argv),
	m_error(other.m_error),
	m_argc(other.m_argc),
	m_failure(other.m_failure)
{
	other.m_argv = nullptr;
	other.m_error = nullptr;
	other.m_argc = 0;
	other.m_failure = CommandInterpretationFailure::Empty;
}

/* operator=:
 * @other: interpretation whose owned argv and error are transferred.
 *
 * Replaces any prior parse state while leaving the source safely destructible.
 *
 * Returns: this interpretation after the ownership transfer.
 */
CommandInterpretation& CommandInterpretation::operator=(
		CommandInterpretation&& other) noexcept
{
	if (this == &other)
		return *this;
	clear();
	m_command_line = std::move(other.m_command_line);
	m_argv = other.m_argv;
	m_error = other.m_error;
	m_argc = other.m_argc;
	m_failure = other.m_failure;
	other.m_argv = nullptr;
	other.m_error = nullptr;
	other.m_argc = 0;
	other.m_failure = CommandInterpretationFailure::Empty;
	return *this;
}

/* parse:
 * @command_line: GLib shell command line; nullptr denotes empty input.
 *
 * Retains the parsed argv and structured failure so lookup and launch consume
 * one interpretation without invoking a shell.
 *
 * Returns: an owned interpretation, valid or failed.
 */
CommandInterpretation CommandInterpretation::parse(const char* command_line)
{
	CommandInterpretation result;
	if (!command_line)
		return result;
	result.m_command_line = command_line;
	if (!g_shell_parse_argv(command_line, &result.m_argc,
			&result.m_argv, &result.m_error))
	{
		result.m_failure = CommandInterpretationFailure::Parse;
		return result;
	}
	result.m_failure = CommandInterpretationFailure::None;
	return result;
}

/* available:
 *
 * Checks only whether the interpreted executable currently resolves through
 * PATH. A later launch is still allowed to report disappearance as an error.
 *
 * Returns: true when argv is valid and its executable can currently be found.
 */
bool CommandInterpretation::available() const
{
	if (!valid() || !m_argv || !m_argv[0])
		return false;
	gchar* path = g_find_program_in_path(m_argv[0]);
	const bool found = path != nullptr;
	g_free(path);
	return found;
}

const char* CommandInterpretation::argument(int index) const
{
	return index >= 0 && index < m_argc ? m_argv[index] : nullptr;
}

GError* CommandInterpretation::copy_error() const
{
	return m_error ? g_error_copy(m_error) : nullptr;
}

/* clear:
 *
 * Releases all GLib-owned parse state before replacement or destruction.
 */
void CommandInterpretation::clear()
{
	g_strfreev(m_argv);
	g_clear_error(&m_error);
	m_argv = nullptr;
	m_argc = 0;
}

/* spawn_session_command_async:
 * @interpretation: valid parsed command owned by the caller.
 * @error: optional return location for a copied parse error or spawn error.
 *
 * Starts a detached session command with inherited environment and directory,
 * retaining the historical PATH-search and fire-and-forget policy.
 *
 * Returns: true when the child was started.
 */
bool WhiskerMenu::spawn_session_command_async(
		const CommandInterpretation& interpretation, GError** error)
{
	if (!interpretation.valid())
	{
		if (error)
			*error = interpretation.copy_error();
		return false;
	}
	return g_spawn_async(nullptr, interpretation.argv(), nullptr,
			G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, error);
}
