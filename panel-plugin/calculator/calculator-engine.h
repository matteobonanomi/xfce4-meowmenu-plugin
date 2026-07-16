/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_CALCULATOR_ENGINE_H
#define WHISKERMENU_CALCULATOR_ENGINE_H

#include <string>
#include <vector>

namespace WhiskerMenu
{

enum class CalculatorEngine
{
	None,
	Bc,
	Qalculate,
	GnomeCalculator
};

enum class CalculatorInput
{
	Disabled,
	Argument,
	StandardInput
};

struct CalculatorEngineDescriptor
{
	CalculatorEngine engine;
	const char* id;
	const char* label;
	const char* executable;
	CalculatorInput input;
	const char* icon_name;
	const char* fallback_icon_name;
};

struct CalculatorQueryCandidate
{
	bool should_evaluate;
	bool forced;
	std::string expression;
};

const CalculatorEngineDescriptor& calculator_engine_descriptor(CalculatorEngine engine);
CalculatorEngine calculator_engine_from_id(const char* id);
bool calculator_engine_is_available(CalculatorEngine engine);
std::string calculator_engine_resolve_path(CalculatorEngine engine);
CalculatorQueryCandidate calculator_classify_query(CalculatorEngine engine,
		const std::string& query);
bool calculator_query_is_candidate(CalculatorEngine engine, const std::string& query,
			std::string& expression);
bool calculator_query_is_safe(const std::string& expression);
std::vector<std::string> calculator_engine_argv(CalculatorEngine engine,
			const std::string& program_path, const std::string& expression,
			int maximum_decimals);
std::string calculator_engine_stdin(CalculatorEngine engine,
		const std::string& expression, int maximum_decimals);

}

#endif // WHISKERMENU_CALCULATOR_ENGINE_H
