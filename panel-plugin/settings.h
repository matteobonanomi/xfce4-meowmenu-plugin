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

#ifndef WHISKERMENU_SETTINGS_H
#define WHISKERMENU_SETTINGS_H

#include "ui/icon-size.h"
#include "config/usage-stats.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <xfconf/xfconf.h>

namespace WhiskerMenu
{

class Command;
class Plugin;
class SearchAction;

enum class ReloadIntent
{
	None,
	Button,
	Layout,
	Content
};

ReloadIntent classify_reload_intent(const gchar* property);


// Boolean setting
class Boolean
{
public:
	Boolean(Settings* settings, const gchar* property, bool data);

	operator bool() const
	{
		return m_data;
	}

	Boolean& operator=(bool data)
	{
		set(data, true);
		return *this;
	}

	void load(XfceRc* rc, bool is_default);
	bool load(const gchar* property, const GValue* value);

private:
	void set(bool data, bool store);

private:
	Settings* m_settings;
	const gchar* const m_property;
	bool m_default;
	bool m_data;
};


// Integer setting
class Integer
{
public:
	Integer(Settings* settings, const gchar* property, int data, int min, int max);

	operator int() const
	{
		return m_data;
	}

	Integer& operator=(int data)
	{
		set(data, true);
		return *this;
	}

	void load(XfceRc* rc, bool is_default);
	bool load(const gchar* property, const GValue* value);

private:
	void set(int data, bool store);

private:
	Settings* m_settings;
	const gchar* const m_property;
	const int m_min;
	const int m_max;
	int m_default;
	int m_data;
};


// String setting
class String
{
public:
	String(Settings* settings, const gchar* property, const std::string& data = std::string());

	bool empty() const
	{
		return m_data.empty();
	}

	operator const char*() const
	{
		return m_data.c_str();
	}

	bool operator==(const char* data) const
	{
		return data ? (m_data == data) : m_data.empty();
	}

	String& operator=(const std::string& data)
	{
		set(data, true);
		return *this;
	}

	String& operator=(const char* data)
	{
		return *this = std::string(data ? data : "");
	}

	void load(XfceRc* rc, bool is_default);
	bool load(const gchar* property, const GValue* value);

private:
	void set(const std::string& data, bool store);

private:
	Settings* m_settings;
	const gchar* const m_property;
	std::string m_default;
	std::string m_data;
};


// String list setting
class StringList
{
public:
	StringList(Settings* settings, const gchar* property, std::initializer_list<std::string> data);

	bool empty() const
	{
		return m_data.empty();
	}

	std::vector<std::string>::const_iterator begin() const
	{
		return m_data.cbegin();
	}

	std::vector<std::string>::const_iterator end() const
	{
		return m_data.cend();
	}

	int find(const std::string& value) const;

	const std::string& operator[](int pos) const
	{
		return m_data[pos];
	}

	int size() const
	{
		return m_data.size();
	}

	void clear();
	void erase(int pos);
	void insert(int pos, const std::string& value);
	void push_back(const std::string& value);
	void resize(int count);
	void set(int pos, const std::string& value);
	void swap(int i, int j);

	bool is_order_unchanged() const
	{
		return m_order_unchanged;
	}

	void set_order_unchaged()
	{
		m_order_unchanged = true;
	}

	void load(XfceRc* rc, bool is_default);
	bool load(const gchar* property, const GValue* value, bool& reload_menu);
	void save();

private:
	void set(std::vector<std::string>& data, bool store);

private:
	Settings* m_settings;
	const gchar* const m_property;
	std::vector<std::string> m_default;
	std::vector<std::string> m_data;
	bool m_modified;
	bool m_saved;
	bool m_order_unchanged;
};


// SearchAction list setting
class SearchActionList
{
public:
	SearchActionList(Settings* settings, std::initializer_list<SearchAction*> data);
	~SearchActionList();

	bool empty() const
	{
		return m_data.empty();
	}

	std::vector<SearchAction*>::const_iterator begin() const
	{
		return m_data.cbegin();
	}

	std::vector<SearchAction*>::const_iterator end() const
	{
		return m_data.cend();
	}

	SearchAction* operator[](int pos) const
	{
		return m_data[pos];
	}

	int size() const
	{
		return m_data.size();
	}

	void erase(SearchAction* value);
	void push_back(SearchAction* value);

	void set_modified()
	{
		m_modified = true;
	}

	void load(XfceRc* rc, bool is_default);
	void load();
	bool load(const gchar* property, const GValue* value);
	void save();

private:
	void clone(const std::vector<SearchAction*>& in, std::vector<SearchAction*>& out) const;

private:
	Settings* m_settings;
	std::vector<SearchAction*> m_default;
	std::vector<SearchAction*> m_data;
	bool m_modified;
};


// Settings class
class Settings
{
	explicit Settings(Plugin* plugin);
	~Settings();

	Settings(const Settings&) = delete;
	Settings(Settings&&) = delete;
	Settings& operator=(const Settings&) = delete;
	Settings& operator=(Settings&&) = delete;

	void load(const gchar* file, bool is_default);
	void load(const gchar* base);

	void prevent_invalid();
	void property_changed(const gchar* property, const GValue* value);

	Plugin* m_plugin;
	gulong m_change_slot;
	std::string m_button_title_default;

public:
	void begin_property_update()
	{
		g_signal_handler_block(channel, m_change_slot);
	}

	void end_property_update()
	{
		g_signal_handler_unblock(channel, m_change_slot);
	}

public:
	XfconfChannel* channel;

	StringList favorites;
	StringList recent;

	String custom_menu_file;

	String button_title;
	String button_icon_name;
	Boolean button_title_visible;
	Boolean button_icon_visible;
	Boolean button_single_row;

	Boolean launcher_show_name;
	Boolean launcher_show_description;
	Boolean launcher_show_tooltip;
	Boolean transparent_grid;
	IconSize launcher_icon_size;

	Boolean category_hover_activate;
	Boolean category_show_name;
	Boolean sort_categories;
	IconSize category_icon_size;

	enum ViewMode
	{
		ViewAsIcons = 0,
		ViewAsList,
		ViewAsTree
	};
	Integer view_mode;

	enum DefaultCategory
	{
		CategoryFavorites = 0,
		CategoryRecent,
		CategoryAll
	};
	Integer default_category;

	Integer recent_items_max;
	Boolean favorites_in_recent;

	Boolean position_profile_alternate;
	Boolean position_search_alternate;
	Boolean position_commands_alternate;
	Boolean unified_bar;
	Boolean position_categories_alternate;
	Boolean position_categories_horizontal;
	Boolean stay_on_focus_out;

	enum ProfileShape
	{
		ProfileRound = 0,
		ProfileSquare,
		ProfileHidden
	};
	Integer profile_shape;

	enum Commands
	{
		CommandSettings = 0,
		CommandLockScreen,
		CommandSwitchUser,
		CommandLogOutUser,
		CommandRestart,
		CommandShutDown,
		CommandSuspend,
		CommandHibernate,
		CommandLogOut,
		CommandMenuEditor,
		CommandProfile,
		CountCommands
	};
	Command* command[CountCommands];
	Boolean confirm_session_command;

	SearchActionList search_actions;

	// Search Ranking 2.0 — Xfconf-backed options
	Boolean fuzzy_enabled;           // /search/fuzzy-enabled,           true
	Integer fuzzy_threshold;         // /search/fuzzy-threshold,          0 (adaptive), 0–2
	Boolean favorites_boost_enabled; // /search/favorites-boost-enabled,  true
	Integer favorites_boost_level;   // /search/favorites-boost-level,    2, 1–3
	Integer frecency_alpha;          // /search/frecency-alpha,           70, 0–100 (÷100.0)

	// App-launch statistics for frecency (XDG_CACHE_HOME, not Xfconf)
	UsageStats usage_stats;

	// Alias map: desktop-id → list of extra search terms
	const std::vector<std::string>& get_aliases(const char* desktop_id) const;
	void set_aliases(const std::string& desktop_id,
	                 const std::vector<std::string>& terms);
	void load_aliases(XfconfChannel* ch);
	void save_aliases(XfconfChannel* ch);

	Integer menu_width;
	Integer menu_height;
	Integer menu_opacity;

	// Layout Presets (milestone 002) — schema versioning & preset tracking
	Integer schema_version;
	String  current_preset_id;

	// Persisted first-run marker. Records that the launcher has completed
	// initialization. This — not the raw Xfconf property count — is the
	// authoritative fresh-vs-upgrade signal: it stays correct even when a
	// still-running xfconfd serves stale in-memory state for the channel.
	// Internal flag only; not exposed in the GUI.
	Boolean initialized;

	// Active preset's stored identity name, surfaced as the active-preset label.
	// Built-ins return their localized display name, custom presets their stored
	// name; falls back to the stored id when no preset matches.
	std::string current_preset_name() const;

	// Layout Presets — visual properties
	Integer corner_radius;
	Integer panel_gap;

	// Layout Presets — element positions
	String sidebar_position;
	// Whether the category sidebar is shown at all (FR-032/033). OFF relocates
	// the Apps/Places switch into the search-bar row; the legacy "hidden"
	// sidebar-position migrates to this being false.
	Boolean sidebar_enabled;
	String search_bar_position;
	String profile_position;
	String commands_position;

	// Layout Presets — grid (FullScreen mode)
	String  grid_density;

	// Layout Presets — window mode
	String layout_mode;

	// Places mode (milestone 005)
	Boolean places_enabled;
	Boolean places_history_enabled;
	Boolean places_favourites_enabled;
	String  places_favourite_sync;
	Integer places_max_items;
	Boolean places_remember_last_mode;
	String  places_last_mode;
	StringList places_favourites;
	// Render the Apps/Places switch as two themed icon buttons instead of text
	// (FR-001/006). Stored value is the user's intent; layouts that force
	// icon-only mode never overwrite it.
	Boolean places_switch_show_icons;

	void migrate_schema(bool marker, bool empty_channel);

	friend class Plugin;

private:
	std::unordered_map<std::string, std::vector<std::string>> m_aliases;
};

}

#endif // WHISKERMENU_SETTINGS_H
