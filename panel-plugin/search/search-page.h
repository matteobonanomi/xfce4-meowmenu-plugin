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

#ifndef WHISKERMENU_SEARCH_PAGE_H
#define WHISKERMENU_SEARCH_PAGE_H

#include "launcher/page.h"
#include "query.h"
#include "run-action.h"
#include "calculator/calculator-evaluator.h"

#include <string>
#include <vector>

namespace WhiskerMenu
{

class SearchPage : public Page
{
public:
	SearchPage(Settings* settings, Window* window);
	~SearchPage();

	GtkWidget* get_message() const
	{
		return m_message;
	}
	GtkWidget* get_calculator_result() const;
	GtkWidget* get_preferred_focus_widget() const;
	bool has_calculator_result() const;
	bool focus_first_visual_result();
	bool activate_first();
	void refresh_calculator_presentation();

	void set_filter(const gchar* filter);
	void set_menu_items();
	void unset_menu_items();

private:
	unsigned int move_launcher(const std::string& desktop_id, unsigned int pos);
	void update_search_order();
	void populate_search_results();
	void update_calculator_presentation();
	void view_created() override;
	bool activate_calculator_result();

private:
	Query m_query;
	std::vector<Launcher*> m_launchers;
	RunAction m_run_action;
	GtkWidget* m_message;
	class CalculatorResult* m_calculator_result;
	CalculatorEvaluator m_calculator_evaluator;
	unsigned int m_calculator_generation;
	gint64 m_last_calculator_activation;

	class Match
	{
	public:
		Match(Element* element = nullptr) :
			m_element(element),
			m_relevancy(UINT_MAX),
			m_boost(0.0)
		{
		}

		Element* element() const
		{
			return m_element;
		}

		// Composite sort: lower textual relevancy wins; at tie, higher boost wins.
		bool operator<(const Match& other) const
		{
			if (m_relevancy != other.m_relevancy)
				return m_relevancy < other.m_relevancy;
			return m_boost > other.m_boost;
		}

		bool operator==(const Match& match) const
		{
			return m_element == match.m_element;
		}

		void update(const Query& query)
		{
			g_assert(m_element);
			m_relevancy = m_element->search(query);
			m_boost = 0.0;
		}

		// Populate frecency+favorites boost for the composite sort key.
		// boost_level: 1=Low, 2=Medium, 3=High (maps to FAVORITES_BONUS table).
		void set_frecency(double frecency, bool is_favorite, int boost_level);

		static bool invalid(const Match& match)
		{
			return match.m_relevancy == UINT_MAX;
		}

	private:
		Element*     m_element;
		unsigned int m_relevancy;
		double       m_boost;
	};
	std::vector<Match> m_matches;
	std::vector<Match> m_search_action_matches;
};

}

#endif // WHISKERMENU_SEARCH_PAGE_H
