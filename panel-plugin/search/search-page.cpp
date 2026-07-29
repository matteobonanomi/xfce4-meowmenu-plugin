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

#include "search-page.h"

#include "calculator-result.h"
#include "core/window-keyboard.h"

#include "launcher/applications-page.h"
#include "launcher/launcher.h"
#include "ui/launcher-view.h"
#include "search-action.h"
#include "settings.h"
#include "ui/slot.h"
#include "core/window.h"

#include <algorithm>

#include <libxfce4ui/libxfce4ui.h>
#include <gdk/gdkkeysyms.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

// Favorites bonus values for boost_level 1 (Low), 2 (Medium), 3 (High).
// Must exceed maximum frecency [0,1] so favorites always precede non-favorites
// at equal textual score, while a much-worse textual match still loses (RF-02 R2.3).
static constexpr double kFavBonus[] = { 0.5, 1.0, 2.0 };

void SearchPage::Match::set_frecency(double frecency, bool is_favorite, int boost_level)
{
	const int level = CLAMP(boost_level, 1, 3) - 1;
	m_boost = frecency + (is_favorite ? kFavBonus[level] : 0.0);
}

//-----------------------------------------------------------------------------

SearchPage::SearchPage(Settings* settings, Window* window) :
	Page(settings, window, nullptr, nullptr),
	m_run_action(settings),
	m_calculator_result(new CalculatorResult()),
	m_calculator_generation(0),
	m_last_calculator_activation(0)
{
	view_created();

	m_calculator_result->set_activate_callback(
			[this]() { activate_calculator_result(); });
	connect(m_calculator_result->get_focus_widget(), "key-press-event",
			[this, window](GtkWidget*, GdkEventKey* event) -> gboolean
			{
				const bool up = event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up;
				const bool down = event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down;
				if (!up && !down)
					return GDK_EVENT_PROPAGATE;
				const Keyboard::CalculatorFocus target = Keyboard::calculator_vertical_target(
					true, Keyboard::CalculatorFocus::Banner, up, false);
				if (target == Keyboard::CalculatorFocus::Search)
				{
					gtk_widget_grab_focus(GTK_WIDGET(window->get_search_entry()));
					return GDK_EVENT_STOP;
				}
				if (target == Keyboard::CalculatorFocus::Results)
				{
					select_first();
					gtk_widget_grab_focus(get_view()->get_widget());
					return GDK_EVENT_STOP;
				}
				return GDK_EVENT_PROPAGATE;
			});
	connect(window->get_search_entry(), "stop-search",
		[](GtkSearchEntry* entry)
		{
			const gchar* text = gtk_entry_get_text(GTK_ENTRY(entry));
			if (!xfce_str_is_empty(text))
			{
				gtk_entry_set_text(GTK_ENTRY(entry), "");
			}
		});

	// Create message for when no applications are found
	m_message = gtk_info_bar_new();
	GtkInfoBar* bar = GTK_INFO_BAR(m_message);
	gtk_info_bar_set_message_type(bar, GTK_MESSAGE_INFO);

	GtkWidget* content_area = gtk_info_bar_get_content_area(bar);
	GtkWidget* label = gtk_label_new(_("No applications found"));
	gtk_container_add(GTK_CONTAINER(content_area), label);
}

//-----------------------------------------------------------------------------

SearchPage::~SearchPage()
{
	m_calculator_evaluator.cancel();
	delete m_calculator_result;
	unset_menu_items();
}

GtkWidget* SearchPage::get_calculator_result() const
{
	return m_calculator_result->get_widget();
}

GtkWidget* SearchPage::get_preferred_focus_widget() const
{
	return m_calculator_result->is_visible()
			? m_calculator_result->get_focus_widget()
			: get_view()->get_widget();
}

bool SearchPage::has_calculator_result() const
{
	return m_calculator_result->is_visible();
}

bool SearchPage::activate_first()
{
	if (m_calculator_result->is_visible())
	{
		activate_calculator_result();
		return true;
	}
	return Page::activate_first();
}

/* activate_calculator_result:
 *
 * Copies one successful normalized value and hides the menu. Visible missing-bc
 * guidance consumes activation as a strict no-op so it can never fall through
 * to the first ordinary launcher result.
 *
 * Returns: true when a visible Calculator state consumed activation.
 */
bool SearchPage::activate_calculator_result()
{
	if (!m_calculator_result->is_visible())
		return false;
	if (!m_calculator_result->is_activatable())
		return true;
	const gint64 now = g_get_monotonic_time();
	if (now - m_last_calculator_activation < 250000)
		return true;
	m_last_calculator_activation = now;
	GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, m_calculator_result->value().c_str(), -1);
	get_window()->hide();
	return true;
}

//-----------------------------------------------------------------------------

void SearchPage::set_filter(const gchar* filter)
{
	const std::string raw_query = filter ? filter : "";
	if (m_query.raw_query() == raw_query)
		return;

	gtk_widget_hide(m_message);
	m_calculator_evaluator.cancel();
	m_calculator_result->clear();
	++m_calculator_generation;

	// Clear search results for empty filter
	if (!filter)
	{
		m_query.clear();
		m_matches.clear();
		return;
	}

	// Make sure this is a new search
	// Reset search results if new search does not start with previous search
	if (m_query.raw_query().empty() || !g_str_has_prefix(filter, m_query.raw_query().c_str()))
	{
		update_search_order();
		m_matches.clear();
		m_matches.push_back(&m_run_action);
		for (auto launcher : m_launchers)
		{
			m_matches.push_back(launcher);
		}
	}
	else if (std::find(m_matches.cbegin(), m_matches.cend(), &m_run_action) == m_matches.cend())
	{
		m_matches.insert(m_matches.begin(), &m_run_action);
	}
	m_query.set(raw_query);

	// Create search results
	m_search_action_matches.clear();
	m_search_action_matches.reserve(m_settings->search_actions.size());
	for (auto action : m_settings->search_actions)
	{
		Match match(action);
		match.update(m_query);
		if (!Match::invalid(match))
		{
			m_search_action_matches.push_back(std::move(match));
		}
	}
	std::stable_sort(m_search_action_matches.begin(), m_search_action_matches.end());
	std::reverse(m_search_action_matches.begin(), m_search_action_matches.end());

	for (auto& match : m_matches)
	{
		match.update(m_query);
	}

	// Populate frecency+favorites boost for composite sort key
	{
		const double alpha = static_cast<int>(m_settings->frecency_alpha) / 100.0;
		for (auto& match : m_matches)
		{
			if (Match::invalid(match))
				continue;
			const Launcher* launcher = dynamic_cast<const Launcher*>(match.element());
			if (!launcher)
				continue;
			const char* id = launcher->get_desktop_id();
			const bool is_fav = m_settings->favorites_boost_enabled
			                    && (m_settings->favorites.find(id) >= 0);
			const double frecency = m_settings->usage_stats.get_frecency(id, alpha);
			match.set_frecency(frecency, is_fav,
			                   static_cast<int>(m_settings->favorites_boost_level));
		}
	}

	m_matches.erase(std::remove_if(m_matches.begin(), m_matches.end(), &Match::invalid), m_matches.end());
	std::stable_sort(m_matches.begin(), m_matches.end());

	// Fall back to non-regex search actions if there are no search results
	if (m_search_action_matches.empty() && m_matches.empty())
	{
		gtk_widget_show(m_message);

		for (auto action : m_settings->search_actions)
		{
			if (action->get_is_regex())
			{
				continue;
			}

			std::string new_filter(action->get_pattern());
			new_filter += filter;
			Query query(new_filter);

			Match match(action);
			match.update(query);
			m_search_action_matches.push_back(std::move(match));
		}
	}

	populate_search_results();

	std::string expression;
	const CalculatorEngine engine = calculator_engine_from_id(m_settings->calculator_engine);
	if (calculator_query_is_candidate(engine, raw_query, expression))
	{
		if (!calculator_engine_is_available(engine))
		{
			if (engine == CalculatorEngine::Bc)
			{
				m_calculator_result->set_missing_bc();
				update_calculator_presentation();
				gtk_widget_hide(m_message);
			}
		}
		else
		{
			m_calculator_result->set_pending();
			const unsigned int generation = m_calculator_generation;
			const int maximum_decimals = m_settings->calculator_max_decimal_places;
			const int font_size = m_settings->calculator_result_font_size;
			m_calculator_evaluator.evaluate(engine, expression,
					maximum_decimals, generation,
					[this, expression, engine, maximum_decimals, font_size](
							const CalculatorEvaluation& evaluation)
					{
						if (evaluation.generation != m_calculator_generation
								|| evaluation.expression != expression
								|| evaluation.engine != engine
								|| evaluation.maximum_decimals != maximum_decimals
								|| calculator_engine_from_id(m_settings->calculator_engine) != engine
								|| static_cast<int>(m_settings->calculator_max_decimal_places)
										!= maximum_decimals
								|| static_cast<int>(m_settings->calculator_result_font_size)
										!= font_size
								|| evaluation.state != CalculatorEvaluationState::Success)
							return;
						const CalculatorEngineDescriptor& descriptor =
								calculator_engine_descriptor(evaluation.engine);
						m_calculator_result->set_result(
							_(descriptor.label), descriptor.icon_name,
							descriptor.fallback_icon_name, evaluation.value, font_size);
						update_calculator_presentation();
						populate_search_results();
						gtk_widget_hide(m_message);
					});
		}
	}
}

//-----------------------------------------------------------------------------

/* populate_search_results:
 *
 * Rebuilds the launcher model from the current query matches. A committed
 * Calculator success removes only custom actions and Run; launcher matches are
 * always retained and every non-success state restores the ordinary fallbacks.
 */
void SearchPage::populate_search_results()
{
	GtkListStore* store = gtk_list_store_new(
			LauncherView::N_COLUMNS,
			G_TYPE_ICON,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);
	Element* element = nullptr;
	if (!m_calculator_result->suppresses_fallbacks())
	{
		for (const auto& match : m_search_action_matches)
		{
			element = match.element();
			gtk_list_store_insert_with_values(
					store, nullptr, G_MAXINT,
					LauncherView::COLUMN_ICON, element->get_icon(),
					LauncherView::COLUMN_TEXT, element->get_text(),
					LauncherView::COLUMN_TOOLTIP, element->get_tooltip(),
					LauncherView::COLUMN_LAUNCHER, element,
					-1);
		}
	}
	for (const auto& match : m_matches)
	{
		element = match.element();
		if (m_calculator_result->suppresses_fallbacks() && element == &m_run_action)
			continue;
		gtk_list_store_insert_with_values(
				store, nullptr, G_MAXINT,
				LauncherView::COLUMN_ICON, element->get_icon(),
				LauncherView::COLUMN_TEXT, element->get_text(),
				LauncherView::COLUMN_TOOLTIP, element->get_tooltip(),
				LauncherView::COLUMN_LAUNCHER, element,
				-1);
	}
	get_view()->set_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	select_first();
}

//-----------------------------------------------------------------------------

/* update_calculator_presentation:
 *
 * Copies the active launcher view's metrics into the external Calculator row.
 * It is called after result-state changes and view allocations so list/tree and
 * grid sizing remains synchronized while the menu is resized.
 */
void SearchPage::update_calculator_presentation()
{
	LauncherView* view = get_view();
	const bool is_grid = view->is_grid_view();
	m_calculator_result->set_presentation_metrics(view->get_item_height(),
			view->get_icon_size(), is_grid,
			calculator_auto_font_size(m_settings->current_preset_id));
}

//-----------------------------------------------------------------------------

/* refresh_calculator_presentation:
 *
 * Re-resolves the derived Auto size after a layout-only setting notification.
 * This restyles an existing Calculator result without rerunning its engine.
 */
void SearchPage::refresh_calculator_presentation()
{
	update_calculator_presentation();
}

//-----------------------------------------------------------------------------

void SearchPage::set_menu_items()
{
	m_launchers = get_window()->get_applications()->find_all();

	get_view()->unset_model();

	m_matches.clear();
	m_matches.reserve(m_launchers.size() + 1);
}

//-----------------------------------------------------------------------------

void SearchPage::unset_menu_items()
{
	m_launchers.clear();
	m_matches.clear();
	get_view()->unset_model();
}

//-----------------------------------------------------------------------------

unsigned int SearchPage::move_launcher(const std::string& desktop_id, unsigned int pos)
{
	for (auto launcher = m_launchers.begin() + pos, end = m_launchers.end(); launcher != end; ++launcher)
	{
		if (desktop_id == (*launcher)->get_desktop_id())
		{
			std::rotate(m_launchers.begin() + pos, launcher, launcher + 1);
			pos++;
			break;
		}
	}
	return pos;
}

//-----------------------------------------------------------------------------

void SearchPage::update_search_order()
{
	if (m_settings->recent.is_order_unchanged() && m_settings->favorites.is_order_unchanged())
	{
		return;
	}
	m_settings->recent.set_order_unchaged();
	m_settings->favorites.set_order_unchaged();

	// Reset in case a launcher is no longer in favorites or recent
	std::sort(m_launchers.begin(), m_launchers.end(), &Element::less_than);

	// Move launchers for favorites and recent to front
	unsigned int pos = 0;
	for (const std::string& desktop_id : m_settings->recent)
	{
		pos = move_launcher(desktop_id, pos);
	}
	for (const std::string& desktop_id : m_settings->favorites)
	{
		pos = move_launcher(desktop_id, pos);
	}
}

//-----------------------------------------------------------------------------

void SearchPage::view_created()
{
	get_view()->set_selection_mode(GTK_SELECTION_BROWSE);
	connect(get_view()->get_widget(), "size-allocate",
			[this](GtkWidget*, GtkAllocation*)
			{
				update_calculator_presentation();
			});
	update_calculator_presentation();
}

//-----------------------------------------------------------------------------
