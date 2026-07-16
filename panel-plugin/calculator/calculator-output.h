/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_CALCULATOR_OUTPUT_H
#define WHISKERMENU_CALCULATOR_OUTPUT_H

#include <string>

namespace WhiskerMenu
{

struct CalculatorNormalizedValue
{
	std::string full_value;
	unsigned int fraction_digits;
	bool rounded;
};

bool calculator_normalize_output(const std::string& output, int maximum_decimals,
			std::string& normalized);

}

#endif // WHISKERMENU_CALCULATOR_OUTPUT_H
