/*
 * Copyright (C) 2013-2020 Graeme Gott <graeme@gottcode.org>
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

#include "query.h"

#include <algorithm>
#include <numeric>
#include <sstream>
#include <vector>

#include <glib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* normalized_codepoints:
 * @text: valid UTF-8 search text to normalize and case-fold.
 *
 * Returns: Unicode code points used by fuzzy distance; empty for invalid input.
 */
static std::vector<gunichar> normalized_codepoints(const std::string& text)
{
	std::vector<gunichar> result;
	if (!g_utf8_validate(text.c_str(), -1, nullptr))
	{
		return result;
	}

	gchar* normalized = g_utf8_normalize(text.c_str(), -1, G_NORMALIZE_DEFAULT);
	if (!normalized)
	{
		return result;
	}
	gchar* folded = g_utf8_casefold(normalized, -1);
	g_free(normalized);
	if (!folded)
	{
		return result;
	}

	for (const gchar* pos = folded; *pos; pos = g_utf8_next_char(pos))
	{
		result.push_back(g_utf8_get_char(pos));
	}
	g_free(folded);

	return result;
}

// Rolling 2-row Levenshtein distance: O(m*n) time, O(n) space.
static int levenshtein(const std::vector<gunichar>& a,
		const std::vector<gunichar>& b)
{
	const size_t m = a.size();
	const size_t n = b.size();
	std::vector<int> prev(n + 1), curr(n + 1);
	std::iota(prev.begin(), prev.end(), 0);
	for (size_t i = 1; i <= m; ++i)
	{
		curr[0] = static_cast<int>(i);
		for (size_t j = 1; j <= n; ++j)
		{
			curr[j] = (a[i - 1] == b[j - 1])
			           ? prev[j - 1]
			           : 1 + std::min({prev[j], curr[j - 1], prev[j - 1]});
		}
		std::swap(prev, curr);
	}
	return prev[n];
}

//-----------------------------------------------------------------------------

static inline bool is_start_word(const std::string& string, std::string::size_type pos)
{
	return (pos == 0) || g_unichar_isspace(g_utf8_get_char(g_utf8_prev_char(&string.at(pos))));
}

//-----------------------------------------------------------------------------

Query::Query(const std::string& query)
{
	set(query);
}

//-----------------------------------------------------------------------------

unsigned int Query::match(const std::string& haystack) const
{
	// Make sure haystack is longer than query
	if (m_query.empty() || (m_query.length() > haystack.length()))
	{
		return UINT_MAX;
	}

	// Check if haystack begins with or is query
	std::string::size_type pos = haystack.find(m_query);
	if (pos == 0)
	{
		return (haystack.length() == m_query.length()) ? 0x4 : 0x8;
	}
	// Check if haystack contains query starting at a word boundary
	else if ((pos != std::string::npos) && is_start_word(haystack, pos))
	{
		return 0x10;
	}

	if (m_query_words.size() > 1)
	{
		// Check if haystack contains query as words
		std::string::size_type search_pos = 0;
		for (const auto& word : m_query_words)
		{
			search_pos = haystack.find(word, search_pos);
			if ((search_pos == std::string::npos) || !is_start_word(haystack, search_pos))
			{
				search_pos = std::string::npos;
				break;
			}
		}
		if (search_pos != std::string::npos)
		{
			return 0x20;
		}

		// Check if haystack contains query as words in any order
		decltype(m_query_words.size()) found_words = 0;
		for (const auto& word : m_query_words)
		{
			search_pos = haystack.find(word);
			if ((search_pos != std::string::npos) && is_start_word(haystack, search_pos))
			{
				++found_words;
			}
			else
			{
				break;
			}
		}
		if (found_words == m_query_words.size())
		{
			return 0x40;
		}
	}

	// Check if haystack contains query
	if (pos != std::string::npos)
	{
		return 0x80;
	}

	// Compact-form fallback for multi-token queries: rerun the
	// prefix / word-boundary / substring tests with the query's
	// internal whitespace stripped. Treats an accidentally-typed
	// space inside a word (e.g. "fi le") as a typo so it can still
	// reach "file manager" instead of dropping to zero results.
	// Legitimate multi-word queries already matched above with a
	// better (lower) score, so they are unaffected.
	if (m_query_words.size() > 1)
	{
		std::string compact;
		for (const auto& word : m_query_words)
		{
			compact += word;
		}
		if (compact.length() <= haystack.length())
		{
			const std::string::size_type cpos = haystack.find(compact);
			if (cpos == 0)
			{
				return (haystack.length() == compact.length()) ? 0x4 : 0x8;
			}
			else if (cpos != std::string::npos)
			{
				return is_start_word(haystack, cpos) ? 0x10 : 0x80;
			}
		}
	}

	return UINT_MAX;
}

//-----------------------------------------------------------------------------

unsigned int Query::match_as_characters(const std::string& haystack) const
{
	// Make sure haystack is longer than query
	if (m_query.empty() || (m_query.length() > haystack.length()))
	{
		return UINT_MAX;
	}

	bool start_word = true;
	const gchar* query_startwords_string = m_query.c_str();
	const gchar* query_string = m_query.c_str();
	for (const gchar* pos = haystack.c_str(); *pos; pos = g_utf8_next_char(pos))
	{
		gunichar c = g_utf8_get_char(pos);

		if (start_word)
		{
			// Check if individual letters of query start words in haystack
			if (c == g_utf8_get_char(query_startwords_string))
			{
				query_startwords_string = g_utf8_next_char(query_startwords_string);
			}
			start_word = false;
		}
		else if (g_unichar_isspace(c))
		{
			start_word = true;
		}

		// Check if individual letters of query are in haystack
		if (c == g_utf8_get_char(query_string))
		{
			query_string = g_utf8_next_char(query_string);
		}
	}

	if (!*query_startwords_string)
	{
		return 0x100;
	}

	if (!*query_string)
	{
		return 0x200;
	}

	return UINT_MAX;
}

//-----------------------------------------------------------------------------

void Query::clear()
{
	m_raw_query.clear();
	m_query.clear();
	m_query_words.clear();
}

//-----------------------------------------------------------------------------

void Query::set(const std::string& query)
{
	m_query.clear();
	m_query_words.clear();

	m_raw_query = query;
	if (m_raw_query.empty())
	{
		return;
	}

	gchar* normalized = g_utf8_normalize(m_raw_query.c_str(), -1, G_NORMALIZE_DEFAULT);
	gchar* utf8 = g_utf8_casefold(normalized, -1);
	m_query = utf8;
	g_free(utf8);
	g_free(normalized);

	std::string buffer;
	std::stringstream ss(m_query);
	while (ss >> buffer)
	{
		m_query_words.push_back(buffer);
	}
}

//-----------------------------------------------------------------------------

unsigned int Query::match_fuzzy(const std::string& haystack, int max_errors) const
{
	// Only single-token queries: multi-token fuzzy produces too many false positives
	if (m_query.empty() || m_query_words.size() > 1)
		return UINT_MAX;

	const std::vector<gunichar> query_points = normalized_codepoints(m_query);
	if (query_points.empty())
	{
		return UINT_MAX;
	}

	// Compare query against each whitespace-delimited word of haystack
	std::string word;
	std::stringstream ss(haystack);
	while (ss >> word)
	{
		const std::vector<gunichar> word_points = normalized_codepoints(word);
		if (word_points.empty())
		{
			continue;
		}
		if (static_cast<int>(word_points.size())
				< static_cast<int>(query_points.size()) - max_errors)
		{
			continue;
		}
		if (levenshtein(query_points, word_points) <= max_errors)
			return 0x400;
	}
	return UINT_MAX;
}

//-----------------------------------------------------------------------------
