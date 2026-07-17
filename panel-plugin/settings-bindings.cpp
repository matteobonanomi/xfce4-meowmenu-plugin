/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
 * Copyright (C) 2026 Matteo Bonanomi
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

#include "settings-bindings.h"

#include "search/search-action.h"
#include "settings.h"

#include <algorithm>
#include <cstdio>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

Boolean::Boolean(WhiskerMenu::Settings* settings, const gchar* property, bool data) :
	m_settings(settings),
	m_property(property),
	m_default(data),
	m_data(m_default)
{
}

//-----------------------------------------------------------------------------

void Boolean::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_bool_entry(rc, m_property + 1, m_data), !is_default);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool Boolean::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_BOOLEAN(value) ? g_value_get_boolean(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void Boolean::set(bool data, bool store)
{
	if (m_data == data)
	{
		return;
	}

	m_data = data;

	if (store && m_settings->channel)
	{
		m_settings->begin_property_update();
		xfconf_channel_set_bool(m_settings->channel, m_property, m_data);
		m_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------

Integer::Integer(WhiskerMenu::Settings* settings, const gchar* property, int data, int min, int max,
		bool reject_to_default) :
	m_settings(settings),
	m_property(property),
	m_min(min),
	m_max(max),
	m_reject_to_default(reject_to_default),
	m_default(CLAMP(data, min, max)),
	m_data(m_default)
{
}

//-----------------------------------------------------------------------------

void Integer::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_int_entry(rc, m_property + 1, m_data), !is_default);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool Integer::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_INT(value) ? g_value_get_int(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void Integer::set(int data, bool store)
{
	data = (m_reject_to_default && (data < m_min || data > m_max))
			? m_default : CLAMP(data, m_min, m_max);
	if (m_data == data)
	{
		return;
	}

	m_data = data;

	if (store && m_settings->channel)
	{
		m_settings->begin_property_update();
		xfconf_channel_set_int(m_settings->channel, m_property, m_data);
		m_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------

String::String(WhiskerMenu::Settings* settings, const gchar* property, const std::string& data) :
	m_settings(settings),
	m_property(property),
	m_default(data),
	m_data(m_default)
{
}

//-----------------------------------------------------------------------------

void String::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_entry(rc, m_property + 1, m_data.c_str()), !is_default);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool String::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_STRING(value) ? g_value_get_string(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void String::set(const std::string& data, bool store)
{
	if (m_data == data)
	{
		return;
	}

	m_data = data;

	if (store && m_settings->channel)
	{
		m_settings->begin_property_update();
		xfconf_channel_set_string(m_settings->channel, m_property, m_data.c_str());
		m_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------

StringList::StringList(WhiskerMenu::Settings* settings, const gchar* property, std::initializer_list<std::string> data) :
	m_settings(settings),
	m_property(property),
	m_default(data),
	m_data(m_default),
	m_modified(false),
	m_saved(false),
	m_order_unchanged(false)
{
}

//-----------------------------------------------------------------------------

int StringList::find(const std::string& value) const
{
	const auto iter = std::find(m_data.begin(), m_data.end(), value);
	if (iter != m_data.end())
	{
		return std::distance(m_data.begin(), iter);
	}
	else
	{
		return -1;
	}
}

//-----------------------------------------------------------------------------

void StringList::clear()
{
	m_data.clear();
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::erase(int pos)
{
	m_data.erase(m_data.begin() + pos);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::insert(int pos, const std::string& value)
{
	m_data.insert(m_data.begin() + pos, value);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::push_back(const std::string& value)
{
	m_data.push_back(value);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::resize(int count)
{
	m_data.resize(count);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::set(int pos, const std::string& value)
{
	m_data[pos] = value;
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::swap(int i, int j)
{
	if (i < 0 || i >= size() || j < 0 || j >= size())
	{
		return;
	}

	std::swap(m_data[i], m_data[j]);
	m_modified = true;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

void StringList::load(XfceRc* rc, bool is_default)
{
	if (!xfce_rc_has_entry(rc, m_property + 1))
	{
		return;
	}

	gchar** data = xfce_rc_read_list_entry(rc, m_property + 1, ",");
	if (!data)
	{
		return;
	}

	std::vector<std::string> strings;
	for (int i = 0; data[i]; ++i)
	{
		strings.push_back(data[i]);
	}
	set(strings, !is_default);

	g_strfreev(data);

	if (is_default)
	{
		m_default = m_data;
	}
}

//-----------------------------------------------------------------------------

bool StringList::load(const gchar* property, const GValue* value, bool& reload_menu)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	// Ignore own changes to prevent extra menu reload
	if (m_saved)
	{
		m_saved = false;
		reload_menu = false;
		return true;
	}

	// Handle resetting to default
	if (G_VALUE_TYPE(value) == G_TYPE_INVALID)
	{
		m_modified = false;
		m_order_unchanged = false;
		m_data = m_default;
		reload_menu = true;
		return true;
	}

	// Convert GValue to string list
	std::vector<std::string> strings;
	if (G_VALUE_HOLDS(value, G_TYPE_PTR_ARRAY))
	{
		const GPtrArray* values = static_cast<const GPtrArray*>(g_value_get_boxed(value));
		for (guint i = 0; i < values->len; ++i)
		{
			const GValue* string = static_cast<const GValue*>(g_ptr_array_index(values, i));
			if (G_VALUE_HOLDS_STRING(string))
			{
				strings.push_back(g_value_get_string(string));
			}
		}
	}
	else if (G_VALUE_HOLDS(value, G_TYPE_STRV))
	{
		const gchar** values = static_cast<const gchar**>(g_value_get_boxed(value));
		for (int i = 0; values[i]; ++i)
		{
			strings.push_back(values[i]);
		}
	}
	else if (G_VALUE_HOLDS_STRING(value))
	{
		strings.push_back(g_value_get_string(value));
	}

	// Load string list
	set(strings, false);
	reload_menu = true;

	return true;
}

//-----------------------------------------------------------------------------

void StringList::save()
{
	if (!m_modified || !m_settings->channel)
	{
		return;
	}

	m_settings->begin_property_update();

	const int size = m_data.size();
	GPtrArray* array = g_ptr_array_sized_new(size);

	for (int i = 0; i < size; ++i)
	{
		GValue* value = g_new0(GValue, 1);
		g_value_init(value, G_TYPE_STRING);
		g_value_set_static_string(value, m_data[i].c_str());
		g_ptr_array_add(array, value);
	}

	xfconf_channel_set_arrayv(m_settings->channel, m_property, array);
	xfconf_array_free(array);

	m_saved = true;
	m_modified = false;

	m_settings->end_property_update();
}

//-----------------------------------------------------------------------------

void StringList::set(std::vector<std::string>& data, bool store)
{
	m_data.clear();

	for (auto& desktop_id : data)
	{
		if (desktop_id == "exo-web-browser.desktop")
		{
			desktop_id = "xfce4-web-browser.desktop";
		}
		else if (desktop_id == "exo-mail-reader.desktop")
		{
			desktop_id = "xfce4-mail-reader.desktop";
		}
		else if (desktop_id == "exo-file-manager.desktop")
		{
			desktop_id = "xfce4-file-manager.desktop";
		}
		else if (desktop_id == "exo-terminal-emulator.desktop")
		{
			desktop_id = "xfce4-terminal-emulator.desktop";
		}
		if (std::find(m_data.begin(), m_data.end(), desktop_id) == m_data.end())
		{
			m_data.push_back(std::move(desktop_id));
		}
	}

	m_modified = store;
	m_order_unchanged = false;
}

//-----------------------------------------------------------------------------

SearchActionList::SearchActionList(WhiskerMenu::Settings* settings, std::initializer_list<SearchAction*> data) :
	m_settings(settings),
	m_data(data),
	m_modified(false)
{
	clone(m_data, m_default);
}

//-----------------------------------------------------------------------------

SearchActionList::~SearchActionList()
{
	for (auto action : m_default)
	{
		delete action;
	}

	for (auto action : m_data)
	{
		delete action;
	}
}

//-----------------------------------------------------------------------------

void SearchActionList::erase(SearchAction* value)
{
	m_data.erase(std::find(m_data.begin(), m_data.end(), value));
	m_modified = true;
}

//-----------------------------------------------------------------------------

void SearchActionList::push_back(SearchAction* value)
{
	m_data.push_back(value);
	m_modified = true;
}

//-----------------------------------------------------------------------------

void SearchActionList::load(XfceRc* rc, bool is_default)
{
	const int size = xfce_rc_read_int_entry(rc, "search-actions", -1);
	if (size < 0)
	{
		return;
	}

	for (int i = 0; i < size; ++i)
	{
		gchar* key = g_strdup_printf("action%i", i);
		if (!xfce_rc_has_group(rc, key))
		{
			g_free(key);
			continue;
		}
		xfce_rc_set_group(rc, key);
		g_free(key);

		SearchAction* action = new SearchAction(m_settings,
				xfce_rc_read_entry(rc, "name", ""),
				xfce_rc_read_entry(rc, "pattern", ""),
				xfce_rc_read_entry(rc, "command", ""),
				xfce_rc_read_bool_entry(rc, "regex", false));

		bool found = false;
		for (auto current : m_data)
		{
			if (*current == *action)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			m_data.push_back(action);
			m_modified = true;
		}
		else
		{
			delete action;
		}
	}

	if (is_default)
	{
		clone(m_data, m_default);
		m_modified = false;
	}
}

//-----------------------------------------------------------------------------

void SearchActionList::load()
{
	const int size = xfconf_channel_get_int(m_settings->channel, "/search-actions", -1);
	if (size < 0)
	{
		return;
	}

	for (auto action : m_data)
	{
		delete action;
	}
	m_data.clear();

	gchar* property = nullptr;
	gchar* name = nullptr;
	gchar* pattern = nullptr;
	gchar* command = nullptr;
	bool regex;

	for (int i = 0; i < size; ++i)
	{
		property = g_strdup_printf("/search-actions/action-%d/name", i);
		name = xfconf_channel_get_string(m_settings->channel, property, nullptr);
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/pattern", i);
		pattern = xfconf_channel_get_string(m_settings->channel, property, nullptr);
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/command", i);
		command = xfconf_channel_get_string(m_settings->channel, property, nullptr);
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/regex", i);
		regex = xfconf_channel_get_bool(m_settings->channel, property, false);
		g_free(property);

		m_data.push_back(new SearchAction(m_settings, name, pattern, command, regex));

		g_free(name);
		g_free(pattern);
		g_free(command);
	}

	m_modified = false;
}

//-----------------------------------------------------------------------------

bool SearchActionList::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0("/search-actions", property) == 0)
	{
		if (G_VALUE_TYPE(value) != G_TYPE_INVALID)
		{
			load();
		}
		else
		{
			clone(m_default, m_data);
		}
		return true;
	}

	int index = -1;
	SearchActionPropertyField field = SearchActionPropertyField::Invalid;
	if (!parse_search_action_property(property, size(), &index, &field))
	{
		return false;
	}

	if (field == SearchActionPropertyField::Invalid)
	{
		return true;
	}
	SearchAction* action = m_data[index];

	if ((field == SearchActionPropertyField::Name) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_name(g_value_get_string(value));
	}
	else if ((field == SearchActionPropertyField::Pattern) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_pattern(g_value_get_string(value));
	}
	else if ((field == SearchActionPropertyField::Command) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_command(g_value_get_string(value));
	}
	else if ((field == SearchActionPropertyField::Regex) && G_VALUE_HOLDS_BOOLEAN(value))
	{
		action->set_is_regex(g_value_get_boolean (value));
	}

	return true;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::parse_search_action_property(const gchar* property, int size,
		int* index, SearchActionPropertyField* field)
{
	if (index)
	{
		*index = -1;
	}
	if (field)
	{
		*field = SearchActionPropertyField::Invalid;
	}

	int parsed_index = -1;
	char parsed_field[16];
	char trailing = '\0';
	if (std::sscanf(property, "/search-actions/action-%d/%15[^/ ]%c",
				&parsed_index, parsed_field, &trailing) != 2)
	{
		return false;
	}

	if ((parsed_index < 0) || (parsed_index >= size))
	{
		return true;
	}

	SearchActionPropertyField parsed = SearchActionPropertyField::Invalid;
	if (g_strcmp0(parsed_field, "name") == 0)
	{
		parsed = SearchActionPropertyField::Name;
	}
	else if (g_strcmp0(parsed_field, "pattern") == 0)
	{
		parsed = SearchActionPropertyField::Pattern;
	}
	else if (g_strcmp0(parsed_field, "command") == 0)
	{
		parsed = SearchActionPropertyField::Command;
	}
	else if (g_strcmp0(parsed_field, "regex") == 0)
	{
		parsed = SearchActionPropertyField::Regex;
	}

	if (parsed == SearchActionPropertyField::Invalid)
	{
		return true;
	}

	if (index)
	{
		*index = parsed_index;
	}
	if (field)
	{
		*field = parsed;
	}

	return true;
}

//-----------------------------------------------------------------------------

void SearchActionList::save()
{
	if (!m_modified || !m_settings->channel)
	{
		return;
	}

	m_settings->begin_property_update();

	xfconf_channel_reset_property(m_settings->channel, "/search-actions", true);

	const int size = m_data.size();
	xfconf_channel_set_int(m_settings->channel, "/search-actions", size);

	gchar* property = nullptr;
	const SearchAction* action = nullptr;

	for (int i = 0; i < size; ++i)
	{
		action = m_data[i];

		property = g_strdup_printf("/search-actions/action-%d/name", i);
		xfconf_channel_set_string(m_settings->channel, property, action->get_name());
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/pattern", i);
		xfconf_channel_set_string(m_settings->channel, property, action->get_pattern());
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/command", i);
		xfconf_channel_set_string(m_settings->channel, property, action->get_command());
		g_free(property);

		property = g_strdup_printf("/search-actions/action-%d/regex", i);
		xfconf_channel_set_bool(m_settings->channel, property, action->get_is_regex());
		g_free(property);
	}

	m_modified = false;

	m_settings->end_property_update();
}

//-----------------------------------------------------------------------------

void SearchActionList::clone(const std::vector<SearchAction*>& in, std::vector<SearchAction*>& out) const
{
	// Remove previous actions
	for (auto action : out)
	{
		delete action;
	}
	out.clear();

	// Copy actions
	out.reserve(in.size());
	for (auto action : in)
	{
		out.push_back(new SearchAction(m_settings,
				action->get_name(),
				action->get_pattern(),
				action->get_command(),
				action->get_is_regex()));
	}
}
