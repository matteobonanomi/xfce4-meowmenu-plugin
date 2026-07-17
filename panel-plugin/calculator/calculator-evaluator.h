/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_CALCULATOR_EVALUATOR_H
#define WHISKERMENU_CALCULATOR_EVALUATOR_H

#include "calculator-engine.h"

#include <functional>
#include <string>

struct _GCancellable;
struct _GSubprocess;

namespace WhiskerMenu
{

struct CalculatorEvaluationJob;

enum class CalculatorEvaluationState
{
	Success,
	Unavailable,
	Failed,
	TimedOut,
	Cancelled
};

struct CalculatorEvaluation
{
	CalculatorEvaluationState state;
	CalculatorEngine engine;
	std::string expression;
	std::string value;
	int maximum_decimals;
	unsigned int generation;
};

struct CalculatorEvaluationRequest
{
	CalculatorEngine engine;
	std::string expression;
	std::string program_path;
	int maximum_decimals;
	unsigned int generation;
};

class CalculatorEvaluator
{
public:
	typedef std::function<void(const CalculatorEvaluation&)> Callback;

	CalculatorEvaluator();
	~CalculatorEvaluator();

	void evaluate(CalculatorEngine engine, const std::string& expression,
			int maximum_decimals, unsigned int generation, Callback callback);
	void cancel();

	// Internal completion hook used by the GLib callback after child reaping.
	void finish(CalculatorEvaluationJob* job);

private:
	CalculatorEvaluationJob* m_job;
};

}

#endif // WHISKERMENU_CALCULATOR_EVALUATOR_H
