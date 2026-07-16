/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "calculator-engine.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <algorithm>
#include <cstdio>

using namespace WhiskerMenu;

namespace
{

const CalculatorEngineDescriptor engines[] = {
	{ CalculatorEngine::None, "none", N_("None"), nullptr, CalculatorInput::Disabled,
		nullptr, "accessories-calculator" },
	{ CalculatorEngine::Bc, "bc", "bc", "bc", CalculatorInput::StandardInput,
		"accessories-calculator", "accessories-calculator" },
	{ CalculatorEngine::Qalculate, "qalc", N_("Qalculate"), "qalc", CalculatorInput::Argument,
		"qalculate", "accessories-calculator" },
	{ CalculatorEngine::GnomeCalculator, "gcalccmd", N_("GNOME Calculator"), "gcalccmd",
		CalculatorInput::Argument, "org.gnome.Calculator", "accessories-calculator" }
};

std::string trim(const std::string& value)
{
	auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return g_ascii_isspace(c); });
	auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return g_ascii_isspace(c); }).base();
	return first < last ? std::string(first, last) : std::string();
}

}

const CalculatorEngineDescriptor& WhiskerMenu::calculator_engine_descriptor(CalculatorEngine engine)
{
	for (const auto& descriptor : engines)
	{
		if (descriptor.engine == engine)
			return descriptor;
	}
	return engines[0];
}

CalculatorEngine WhiskerMenu::calculator_engine_from_id(const char* id)
{
	for (const auto& descriptor : engines)
	{
		if (g_strcmp0(id, descriptor.id) == 0)
			return descriptor.engine;
	}
	return CalculatorEngine::None;
}

bool WhiskerMenu::calculator_engine_is_available(CalculatorEngine engine)
{
	return !calculator_engine_resolve_path(engine).empty();
}

/* calculator_engine_resolve_path:
 * @engine: selected closed-registry engine.
 *
 * Resolves only the executable basename compiled into the descriptor. The
 * absolute result is captured before launch so a saved value can never supply
 * a path or alter the program that is executed.
 *
 * Returns: an absolute executable path, or an empty string when unavailable.
 */
std::string WhiskerMenu::calculator_engine_resolve_path(CalculatorEngine engine)
{
	const char* executable = calculator_engine_descriptor(engine).executable;
	if (!executable)
		return std::string();
	gchar* path = g_find_program_in_path(executable);
	std::string resolved = path ? path : "";
	g_free(path);
	return resolved;
}

/* calculator_query_is_safe:
 * @expression: exact expression sent to a known calculator executable.
 *
 * Rejects input that cannot be safely transported as one argument or bounded
 * standard input. The expression is never concatenated into shell text.
 */
bool WhiskerMenu::calculator_query_is_safe(const std::string& expression)
{
	if (expression.empty() || expression.size() > 4096 || !g_utf8_validate(expression.c_str(), expression.size(), nullptr))
		return false;
	return expression.find('\0') == std::string::npos
			&& expression.find('\r') == std::string::npos
			&& expression.find('\n') == std::string::npos;
}

/* calculator_query_is_candidate:
 * @engine: selected closed-registry engine.
 * @query: raw text from the search entry.
 * @expression: receives the exact candidate text on success.
 *
 * Recognizes explicit '=' requests and conservative arithmetic-like queries.
 * It intentionally does not rewrite syntax for another engine.
 *
 * Returns: true when the caller may evaluate @expression.
 */
CalculatorQueryCandidate WhiskerMenu::calculator_classify_query(
		CalculatorEngine engine, const std::string& query)
{
	CalculatorQueryCandidate candidate = { false, false, std::string() };
	if (engine == CalculatorEngine::None)
		return candidate;

	const std::string stripped = trim(query);
	if (stripped.empty())
		return candidate;
	if (stripped[0] == '=')
	{
		candidate.forced = true;
		candidate.expression = trim(stripped.substr(1));
		candidate.should_evaluate = calculator_query_is_safe(candidate.expression);
		return candidate;
	}

	const bool digit = std::any_of(stripped.begin(), stripped.end(), [](unsigned char c) { return g_ascii_isdigit(c); });
	bool token = stripped.find_first_of("+-*/%^()") != std::string::npos
			|| stripped.find("sqrt") != std::string::npos;
	if (engine != CalculatorEngine::Bc)
	{
		token = token || stripped.find("sin") != std::string::npos
				|| stripped.find("cos") != std::string::npos
				|| stripped.find("tan") != std::string::npos;
	}
	if (engine == CalculatorEngine::Qalculate)
	{
		token = token || stripped.find(" to ") != std::string::npos
				|| stripped.find(" in ") != std::string::npos;
	}
	if (!digit || !token || !calculator_query_is_safe(stripped))
		return candidate;
	candidate.expression = stripped;
	candidate.should_evaluate = true;
	return candidate;
}

bool WhiskerMenu::calculator_query_is_candidate(CalculatorEngine engine,
		const std::string& query, std::string& expression)
{
	const CalculatorQueryCandidate candidate = calculator_classify_query(engine, query);
	expression = candidate.expression;
	return candidate.should_evaluate;
}

/* calculator_engine_argv:
 * @engine: selected known engine.
 * @program_path: absolute path resolved from the engine descriptor.
 * @expression: exact, validated expression.
 * @maximum_decimals: configured precision ceiling in the range 0..10.
 *
 * Builds the fixed one-shot argv for the selected adapter. User text occupies
 * one argument only for argument-transport engines and is never shell-parsed.
 *
 * Returns: argv without the terminating NULL required by GLib.
 */
std::vector<std::string> WhiskerMenu::calculator_engine_argv(CalculatorEngine engine,
		const std::string& program_path, const std::string& expression,
		int maximum_decimals)
{
	if (program_path.empty())
		return { };
	if (engine == CalculatorEngine::Bc)
		return { program_path, "-q", "-l" };
	if (engine == CalculatorEngine::Qalculate)
	{
		char precision[16];
		g_snprintf(precision, sizeof(precision), "precision %d",
				std::max(20, maximum_decimals + 8));
		return { program_path, "--terse", "--time", "1900", "--defaults",
				"--set", precision, "--", expression };
	}
	if (engine == CalculatorEngine::GnomeCalculator)
		return { program_path, expression };
	return { };
}

/* calculator_engine_stdin:
 * @engine: selected known engine.
 * @expression: exact, validated expression.
 * @maximum_decimals: configured precision ceiling in the range 0..10.
 *
 * Produces the complete bounded stdin request for stdin-transport engines.
 * bc requires complete lines, so both the scale command and expression end in
 * newlines before EOF closes the request.
 *
 * Returns: complete stdin payload, or an empty string for argument engines.
 */
std::string WhiskerMenu::calculator_engine_stdin(CalculatorEngine engine,
		const std::string& expression, int maximum_decimals)
{
	if (engine != CalculatorEngine::Bc)
		return std::string();
	char scale[32];
	g_snprintf(scale, sizeof(scale), "scale=%d\n",
			std::max(20, maximum_decimals + 8));
	return std::string(scale) + expression + "\n";
}
