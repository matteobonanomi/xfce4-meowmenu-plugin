/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "calculator-output.h"

#include <glib.h>

#include <algorithm>

using namespace WhiskerMenu;

namespace
{

std::string trim(const std::string& value)
{
	auto first = std::find_if_not(value.begin(), value.end(),
			[](unsigned char c) { return g_ascii_isspace(c); });
	auto last = std::find_if_not(value.rbegin(), value.rend(),
			[](unsigned char c) { return g_ascii_isspace(c); }).base();
	return first < last ? std::string(first, last) : std::string();
}

bool suffix_is_safe(const std::string& suffix)
{
	if (!g_utf8_validate(suffix.c_str(), suffix.size(), nullptr))
		return false;
	for (const gchar* p = suffix.c_str(); *p; p = g_utf8_next_char(p))
	{
		const gunichar c = g_utf8_get_char(p);
		if (g_unichar_iscntrl(c))
			return false;
		if (g_unichar_isalnum(c) || g_unichar_isspace(c))
			continue;
		if (c == '/' || c == '^' || c == '-' || c == '_' || c == '%'
				|| c == 0x00b0 || c == 0x00b7)
			continue;
		return false;
	}
	return true;
}

bool all_zeroes(const std::string& value)
{
	return std::all_of(value.begin(), value.end(),
			[](char digit) { return digit == '0'; });
}

/* round_fraction:
 * @integer: mutable decimal integer digits, without a sign.
 * @fraction: mutable decimal fraction digits.
 * @maximum_decimals: number of fractional digits to retain.
 *
 * Applies half-away-from-zero rounding to unsigned magnitude text. Sign is
 * handled by the caller, so a discarded 5 always increments the magnitude.
 *
 * Returns: true when digits were discarded.
 */
bool round_fraction(std::string& integer, std::string& fraction,
		int maximum_decimals)
{
	if (fraction.size() <= static_cast<size_t>(maximum_decimals))
		return false;
	bool carry = fraction[maximum_decimals] >= '5';
	fraction.resize(maximum_decimals);
	for (size_t i = fraction.size(); carry && i > 0; --i)
	{
		char& digit = fraction[i - 1];
		if (digit == '9')
			digit = '0';
		else
		{
			++digit;
			carry = false;
		}
	}
	for (size_t i = integer.size(); carry && i > 0; --i)
	{
		char& digit = integer[i - 1];
		if (digit == '9')
			digit = '0';
		else
		{
			++digit;
			carry = false;
		}
	}
	if (carry)
		integer.insert(integer.begin(), '1');
	return true;
}

}

/* calculator_normalize_output:
 * @output: bounded, complete stdout received from a calculator process.
 * @maximum_decimals: requested fractional-digit ceiling in the inclusive 0..10 range.
 * @normalized: receives the canonical value used by detail and clipboard.
 *
 * Accepts one numeric-leading scalar, conventional or GNOME-style exponent,
 * and an optional safe unit/base suffix. Rounding operates on decimal text so
 * large values retain their precision and half values round away from zero.
 *
 * Returns: true when @output is safe and represents one result.
 */
bool WhiskerMenu::calculator_normalize_output(const std::string& output,
		int maximum_decimals, std::string& normalized)
{
	normalized.clear();
	if (maximum_decimals < 0 || maximum_decimals > 10 || output.empty()
			|| output.size() > 16384
			|| !g_utf8_validate(output.c_str(), output.size(), nullptr)
			|| output.find('\0') != std::string::npos)
		return false;

	std::string line = output;
	if (!line.empty() && line.back() == '\n')
	{
		line.pop_back();
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
	}
	if (line.find('\n') != std::string::npos || line.find('\r') != std::string::npos)
		return false;
	std::string value = trim(line);
	if (value.empty())
		return false;

	std::string approximation;
	if (value.compare(0, 3, "\xE2\x89\x88") == 0)
	{
		approximation = "\xE2\x89\x88 ";
		value = trim(value.substr(3));
	}
	else if (!value.empty() && value[0] == '~')
	{
		approximation = "~ ";
		value = trim(value.substr(1));
	}
	if (value.empty())
		return false;

	size_t pos = 0;
	bool negative = false;
	if (value[pos] == '+' || value[pos] == '-')
	{
		negative = value[pos] == '-';
		++pos;
	}
	else if (value.compare(pos, 3, "\xE2\x88\x92") == 0)
	{
		negative = true;
		pos += 3;
	}

	const size_t integer_start = pos;
	while (pos < value.size() && g_ascii_isdigit(value[pos]))
		++pos;
	std::string integer = value.substr(integer_start, pos - integer_start);
	std::string fraction;
	char decimal_separator = '.';
	if (pos < value.size() && (value[pos] == '.' || value[pos] == ','))
	{
		decimal_separator = value[pos++];
		const size_t fraction_start = pos;
		while (pos < value.size() && g_ascii_isdigit(value[pos]))
			++pos;
		fraction = value.substr(fraction_start, pos - fraction_start);
		if (fraction.empty())
			return false;
	}
	if (integer.empty() && fraction.empty())
		return false;
	if (integer.empty())
		integer = "0";

	std::string exponent;
	if (pos < value.size() && (value[pos] == 'e' || value[pos] == 'E'))
	{
		const size_t start = pos++;
		if (pos < value.size() && (value[pos] == '+' || value[pos] == '-'))
			++pos;
		const size_t digits = pos;
		while (pos < value.size() && g_ascii_isdigit(value[pos]))
			++pos;
		if (pos == digits)
			return false;
		exponent = value.substr(start, pos - start);
	}
	else if (value.compare(pos, 5, "\xC3\x97" "10^") == 0)
	{
		const size_t start = pos;
		pos += 5;
		if (pos < value.size() && (value[pos] == '+' || value[pos] == '-'))
			++pos;
		const size_t digits = pos;
		while (pos < value.size() && g_ascii_isdigit(value[pos]))
			++pos;
		if (pos == digits)
			return false;
		exponent = value.substr(start, pos - start);
	}

	if (pos < value.size() && !g_ascii_isspace(value[pos]))
		return false;
	const std::string suffix = trim(value.substr(pos));
	if (!suffix_is_safe(suffix))
		return false;

	const bool rounded = round_fraction(integer, fraction, maximum_decimals);
	while (!fraction.empty() && fraction.back() == '0')
		fraction.pop_back();
	if (negative && all_zeroes(integer) && fraction.empty())
		negative = false;

	normalized = approximation;
	if (negative)
		normalized += '-';
	normalized += integer;
	if (!fraction.empty())
	{
		normalized += decimal_separator;
		normalized += fraction;
	}
	normalized += exponent;
	if (!suffix.empty())
		normalized += " " + suffix;
	(void)rounded;
	return true;
}
