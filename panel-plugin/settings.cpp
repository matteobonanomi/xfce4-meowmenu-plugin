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

#include "settings.h"

#include "command.h"
#include "plugin.h"
#include "preset.h"
#include "search-action.h"
#include "slot.h"

#include <algorithm>
#include <sstream>

#include <cstdio>
#include <cstring>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

Settings::Settings(Plugin* plugin) :
	m_plugin(plugin),
	m_change_slot(0),
	m_button_title_default(_("Applications")),
	channel(nullptr),

	favorites(this, "/favorites", {
		"xfce4-web-browser.desktop",
		"xfce4-mail-reader.desktop",
		"xfce4-file-manager.desktop",
		"xfce4-terminal-emulator.desktop"
	}),
	recent(this, "/recent", { }),

	custom_menu_file(this, "/custom-menu-file"),

	button_title(this, "/button-title", m_button_title_default),
	button_icon_name(this, "/button-icon", "org.xfce.panel.meowmenu"),
	button_title_visible(this, "/show-button-title", false),
	button_icon_visible(this, "/show-button-icon", true),
	button_single_row(this, "/button-single-row", false),

	launcher_show_name(this, "/launcher-show-name", true),
	launcher_show_description(this, "/launcher-show-description", true),
	launcher_show_tooltip(this, "/launcher-show-tooltip", true),
	launcher_icon_size(this, "/launcher-icon-size", IconSize::Small),

	category_hover_activate(this, "/hover-switch-category", false),
	category_show_name(this, "/category-show-name", true),
	sort_categories(this, "/sort-categories", true),
	category_icon_size(this, "/category-icon-size", IconSize::Smaller),

	view_mode(this, "/view-mode", ViewAsList, ViewAsIcons, ViewAsTree),

	default_category(this, "/default-category", CategoryFavorites, CategoryFavorites, CategoryAll),

	recent_items_max(this, "/recent-items-max", 10, 0, 100),
	favorites_in_recent(this, "/favorites-in-recent", false),

	position_profile_alternate(this, "/position-profile-alternate", false),
	position_search_alternate(this, "/position-search-alternate", false),
	position_commands_alternate(this, "/position-commands-alternate", false),
	unified_bar(this, "/unified-bar", false),
	position_categories_alternate(this, "/position-categories-alternate", false),
	position_categories_horizontal(this, "/position-categories-horizontal", false),
	stay_on_focus_out(this, "/stay-on-focus-out", false),

	profile_shape(this, "/profile-shape", ProfileRound, ProfileRound, ProfileHidden),

	confirm_session_command(this, "/confirm-session-command", true),

	search_actions(this, {
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
		new SearchAction(this, _("Man Pages"), "#", "xfce-open --launch TerminalEmulator man %s", false),
		new SearchAction(this, _("Search the Web"), "?", "xfce-open --launch WebBrowser https://duckduckgo.com/?q=%u", false),
		new SearchAction(this, _("Search for Files"), "-", "catfish --path=~ --start %s", false),
		new SearchAction(this, _("Wikipedia"), "!w", "xfce-open --launch WebBrowser https://en.wikipedia.org/wiki/%u", false),
		new SearchAction(this, _("Run in Terminal"), "!", "xfce-open --launch TerminalEmulator %s", false),
		new SearchAction(this, _("Open URI"), "^(file|http|https):\\/\\/(.*)$", "xfce-open \\0", true)
#else
		new SearchAction(this, _("Man Pages"), "#", "exo-open --launch TerminalEmulator man %s", false),
		new SearchAction(this, _("Search the Web"), "?", "exo-open --launch WebBrowser https://duckduckgo.com/?q=%u", false),
		new SearchAction(this, _("Search for Files"), "-", "catfish --path=~ --start %s", false),
		new SearchAction(this, _("Wikipedia"), "!w", "exo-open --launch WebBrowser https://en.wikipedia.org/wiki/%u", false),
		new SearchAction(this, _("Run in Terminal"), "!", "exo-open --launch TerminalEmulator %s", false),
		new SearchAction(this, _("Open URI"), "^(file|http|https):\\/\\/(.*)$", "exo-open \\0", true)
#endif
	}),

	fuzzy_enabled(this, "/search/fuzzy-enabled", true),
	fuzzy_threshold(this, "/search/fuzzy-threshold", 0, 0, 2),
	favorites_boost_enabled(this, "/search/favorites-boost-enabled", true),
	favorites_boost_level(this, "/search/favorites-boost-level", 2, 1, 3),
	frecency_alpha(this, "/search/frecency-alpha", 70, 0, 100),

	menu_width(this, "/menu-width", 450, 10, SHRT_MAX),
	menu_height(this, "/menu-height", 500, 10, SHRT_MAX),
	menu_opacity(this, "/menu-opacity", 100, 0, 100),

	schema_version(this, "/schema-version", 0, 0, G_MAXINT),
	current_preset_id(this, "/current-preset-id"),

	corner_radius(this, "/corner-radius", 0, 0, 24),
	panel_gap(this, "/panel-gap", 0, 0, 50),
	categories_opacity(this, "/categories-opacity", 100, 0, 100),
	apps_opacity(this, "/apps-opacity", 100, 0, 100),
	full_screen_opacity(this, "/full-screen-opacity", 100, 0, 100),

	sidebar_position(this, "/sidebar-position", "left"),
	search_bar_position(this, "/search-bar-position", "top"),
	profile_position(this, "/profile-position", "top"),
	commands_position(this, "/commands-position", "top-right"),

	grid_auto_size(this, "/grid-auto-size", true),
	grid_columns(this, "/grid-columns", 4, 2, 10),
	grid_rows(this, "/grid-rows", 3, 1, 8),
	grid_density(this, "/grid-density", "medium"),

	layout_mode(this, "/layout-mode", "docked"),

	places_enabled(this, "/places/enabled", false),
	places_history_enabled(this, "/places/history-enabled", true),
	places_favourites_enabled(this, "/places/favourites-enabled", true),
	places_favourite_sync(this, "/places/favourite-sync", "meowmenu"),
	places_max_items(this, "/places/max-items", 20, 0, 30),
	places_remember_last_mode(this, "/places/remember-last-mode", false),
	places_show_metadata(this, "/places/show-metadata", false),
	places_last_mode(this, "/places/last-mode", "apps"),
	places_favourites(this, "/places/favourites", { })
{
	command[CommandSettings] = new Command(this, "/command-settings", "/show-command-settings",
			"org.xfce.settings.manager", "preferences-desktop",
			_("_Settings Manager"),
			"xfce4-settings-manager", true,
			_("Failed to open settings manager."));
	command[CommandLockScreen] = new Command(this, "/command-lockscreen", "/show-command-lockscreen",
			"xfsm-lock", "system-lock-screen",
			_("_Lock Screen"),
			"xflock4", true,
			_("Failed to lock screen."));
	command[CommandSwitchUser] = new Command(this, "/command-switchuser", "/show-command-switchuser",
			"xfsm-switch-user", "system-users",
			_("Switch _User"),
			"xfce4-session-logout --switch-user", false,
			_("Failed to switch user."));
	command[CommandLogOutUser] = new Command(this, "/command-logoutuser", "/show-command-logoutuser",
			"xfsm-logout", "system-log-out",
			_("Log _Out"),
			"xfce4-session-logout --logout --fast", false,
			_("Failed to log out."),
			_("Are you sure you want to log out?"),
			_("Logging out in %d seconds."));
	command[CommandRestart] = new Command(this, "/command-restart", "/show-command-restart",
			"xfsm-reboot", "system-reboot",
			_("_Restart"),
			"xfce4-session-logout --reboot --fast", false,
			_("Failed to restart."),
			_("Are you sure you want to restart?"),
			_("Restarting computer in %d seconds."));
	command[CommandShutDown] = new Command(this, "/command-shutdown", "/show-command-shutdown",
			"xfsm-shutdown", "system-shutdown",
			_("Shut _Down"),
			"xfce4-session-logout --halt --fast", false,
			_("Failed to shut down."),
			_("Are you sure you want to shut down?"),
			_("Turning off computer in %d seconds."));
	command[CommandSuspend] = new Command(this, "/command-suspend", "/show-command-suspend",
			"xfsm-suspend", "system-suspend",
			_("Suspe_nd"),
			"xfce4-session-logout --suspend", false,
			_("Failed to suspend."),
			_("Do you want to suspend to RAM?"),
			_("Suspending computer in %d seconds."));
	command[CommandHibernate] = new Command(this, "/command-hibernate", "/show-command-hibernate",
			"xfsm-hibernate", "system-hibernate",
			_("_Hibernate"),
			"xfce4-session-logout --hibernate", false,
			_("Failed to hibernate."),
			_("Do you want to suspend to disk?"),
			_("Hibernating computer in %d seconds."));
	command[CommandLogOut] = new Command(this, "/command-logout", "/show-command-logout",
			"xfsm-logout", "system-log-out",
			_("Log Ou_t..."),
			"xfce4-session-logout", true,
			_("Failed to log out."));
	command[CommandMenuEditor] = new Command(this, "/command-menueditor", "/show-command-menueditor",
			"menu-editor", "xfce4-menueditor",
			_("_Edit Applications"),
			"menulibre", true,
			_("Failed to launch menu editor."));
	command[CommandProfile] = new Command(this, "/command-profile", "/show-command-profile",
			"avatar-default", "preferences-desktop-user",
			_("Edit _Profile"),
			"mugshot", true,
			_("Failed to edit profile."));
}

//-----------------------------------------------------------------------------

Settings::~Settings()
{
	for (auto i : command)
	{
		delete i;
	}

	if (channel)
	{
		g_object_unref(channel);
		xfconf_shutdown();
	}
}

//-----------------------------------------------------------------------------

void Settings::load(const gchar* file, bool is_default)
{
	if (!file)
	{
		return;
	}

	XfceRc* rc = xfce_rc_simple_open(file, true);
	if (!rc)
	{
		return;
	}
	xfce_rc_set_group(rc, nullptr);

	favorites.load(rc, is_default);
	recent.load(rc, is_default);

	custom_menu_file.load(rc, is_default);

	button_title.load(rc, is_default);
	button_icon_name.load(rc, is_default);
	button_single_row.load(rc, is_default);
	button_title_visible.load(rc, is_default);
	button_icon_visible.load(rc, is_default);

	launcher_show_name.load(rc, is_default);
	launcher_show_description.load(rc, is_default);
	launcher_show_tooltip.load(rc, is_default);
	if (xfce_rc_has_entry(rc, "item-icon-size"))
	{
		launcher_icon_size = xfce_rc_read_int_entry(rc, "item-icon-size", launcher_icon_size);
	}
	launcher_icon_size.load(rc, is_default);

	category_hover_activate.load(rc, is_default);
	category_show_name.load(rc, is_default);
	category_icon_size.load(rc, is_default);

	if (!xfce_rc_has_entry(rc, "view-mode"))
	{
		if (xfce_rc_read_bool_entry(rc, "load-hierarchy", view_mode == ViewAsTree))
		{
			view_mode = ViewAsTree;
			if (!xfce_rc_has_entry(rc, "sort-categories"))
			{
				sort_categories = false;
			}
		}
		else if (xfce_rc_read_bool_entry(rc, "view-as-icons", view_mode == ViewAsIcons))
		{
			view_mode = ViewAsIcons;
		}
	}
	view_mode.load(rc, is_default);
	sort_categories.load(rc, is_default);

	if (xfce_rc_has_entry(rc, "display-recent-default"))
	{
		default_category = xfce_rc_read_bool_entry(rc, "display-recent-default", default_category);
	}
	default_category.load(rc, is_default);

	recent_items_max.load(rc, is_default);
	favorites_in_recent.load(rc, is_default);

	position_profile_alternate.load(rc, is_default);
	position_search_alternate.load(rc, is_default);
	position_commands_alternate.load(rc, is_default);
	position_categories_alternate.load(rc, is_default);
	position_categories_horizontal.load(rc, is_default);
	stay_on_focus_out.load(rc, is_default);

	profile_shape.load(rc, is_default);

	confirm_session_command.load(rc, is_default);

	menu_width.load(rc, is_default);
	menu_height.load(rc, is_default);
	menu_opacity.load(rc, is_default);

	for (auto i : command)
	{
		i->load(rc, is_default);
	}

	search_actions.load(rc, is_default);

	xfce_rc_close(rc);

	prevent_invalid();

	if (!is_default)
	{
		favorites.save();
		recent.save();
		search_actions.save();
	}
	else if (!button_title.empty())
	{
		m_button_title_default = button_title;
	}
}

//-----------------------------------------------------------------------------

void Settings::load(const gchar* base)
{
	// Set up Xfconf channel
	if (base && xfconf_init(nullptr))
	{
		channel = xfconf_channel_new_with_property_base(xfce_panel_get_channel_name(), base);
		m_change_slot = connect(channel, "property-changed",
			[this](XfconfChannel*, const gchar* property, const GValue* value)
			{
				property_changed(property, value);
				prevent_invalid();
			});
	}
	else
	{
		return;
	}

	// Fetch all settings
	GHashTable* properties = xfconf_channel_get_properties(channel, nullptr);
	if (!properties)
	{
		return;
	}

	// Fetch length of property base
	const int base_len = strlen(base);

	// Load settings
	GHashTableIter iter;
	gpointer key, value;
	g_hash_table_iter_init(&iter, properties);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		property_changed(static_cast<const gchar*>(key) + base_len, static_cast<GValue*>(value));
	}

	const guint loaded_property_count = g_hash_table_size(properties);
	g_hash_table_destroy(properties);
	prevent_invalid();
	load_aliases(channel);
	migrate_schema(loaded_property_count == 0);
}

//-----------------------------------------------------------------------------

void Settings::prevent_invalid()
{
	// Prevent empty categories
	if (!category_show_name && (category_icon_size == -1))
	{
		category_show_name = true;
	}

	// Reset default category if recent is hidden
	if (!recent_items_max && (default_category == CategoryRecent))
	{
		default_category = CategoryFavorites;
	}

	// Prevent empty panel button
	if (!button_icon_visible)
	{
		if (!button_title_visible)
		{
			button_icon_visible = true;
		}
		else if (button_title.empty())
		{
			button_title = m_button_title_default;
		}
	}
}

//-----------------------------------------------------------------------------

void Settings::property_changed(const gchar* property, const GValue* value)
{
	bool reload = true;
	if (favorites.load(property, value, reload)
			|| recent.load(property, value, reload)
			|| launcher_show_name.load(property, value)
			|| launcher_show_description.load(property, value)
			|| sort_categories.load(property, value)
			|| view_mode.load(property, value))
	{
		if (reload)
		{
			m_plugin->reload_menu();
		}
	}

	else if (button_title.load(property, value)
			|| button_icon_name.load(property, value)
			|| button_title_visible.load(property, value)
			|| button_icon_visible.load(property, value)
			|| button_single_row.load(property, value))
	{
		m_plugin->reload_button();
	}

	else if (custom_menu_file.load(property, value)
			|| launcher_show_tooltip.load(property, value)
			|| launcher_icon_size.load(property, value)
			|| category_hover_activate.load(property, value)
			|| category_show_name.load(property, value)
			|| category_icon_size.load(property, value)
			|| default_category.load(property, value)
			|| recent_items_max.load(property, value)
			|| favorites_in_recent.load(property, value)
			|| position_profile_alternate.load(property, value)
			|| position_search_alternate.load(property, value)
			|| position_commands_alternate.load(property, value)
			|| unified_bar.load(property, value)
			|| position_categories_alternate.load(property, value)
			|| position_categories_horizontal.load(property, value)
			|| stay_on_focus_out.load(property, value)
			|| profile_shape.load(property, value)
			|| confirm_session_command.load(property, value)
			|| menu_width.load(property, value)
			|| menu_height.load(property, value)
			|| menu_opacity.load(property, value)
			|| search_actions.load(property, value)
			|| fuzzy_enabled.load(property, value)
			|| fuzzy_threshold.load(property, value)
			|| favorites_boost_enabled.load(property, value)
			|| favorites_boost_level.load(property, value)
			|| frecency_alpha.load(property, value)
			|| schema_version.load(property, value)
			|| current_preset_id.load(property, value)
			|| corner_radius.load(property, value)
			|| panel_gap.load(property, value)
			|| categories_opacity.load(property, value)
			|| apps_opacity.load(property, value)
			|| full_screen_opacity.load(property, value)
			|| sidebar_position.load(property, value)
			|| search_bar_position.load(property, value)
			|| profile_position.load(property, value)
			|| commands_position.load(property, value)
			|| grid_auto_size.load(property, value)
			|| grid_columns.load(property, value)
			|| grid_rows.load(property, value)
			|| grid_density.load(property, value)
			|| layout_mode.load(property, value)
			|| places_enabled.load(property, value)
			|| places_history_enabled.load(property, value)
			|| places_favourites_enabled.load(property, value)
			|| places_favourite_sync.load(property, value)
			|| places_max_items.load(property, value)
			|| places_remember_last_mode.load(property, value)
			|| places_show_metadata.load(property, value)
			|| places_last_mode.load(property, value)
			|| places_favourites.load(property, value, reload))
	{
	}

	else
	{
		for (auto i : command)
		{
			if (i->load(property, value))
			{
				break;
			}
		}
	}
}

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

Integer::Integer(WhiskerMenu::Settings* settings, const gchar* property, int data, int min, int max) :
	m_settings(settings),
	m_property(property),
	m_min(min),
	m_max(max),
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
	data = CLAMP(data, m_min, m_max);
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

	int index = 0;
	char field[16];
	if (std::sscanf(property, "/search-actions/action-%d/%14s", &index, field) != 2)
	{
		return false;
	}

	if (index >= size())
	{
		return true;
	}
	SearchAction* action = m_data[index];

	if ((g_strcmp0(field, "name") == 0) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_name(g_value_get_string(value));
	}
	else if ((g_strcmp0(field, "pattern") == 0) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_pattern(g_value_get_string(value));
	}
	else if ((g_strcmp0(field, "command") == 0) && G_VALUE_HOLDS_STRING(value))
	{
		action->set_command(g_value_get_string(value));
	}
	else if ((g_strcmp0(field, "regex") == 0) && G_VALUE_HOLDS_BOOLEAN(value))
	{
		action->set_is_regex(g_value_get_boolean (value));
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

//-----------------------------------------------------------------------------

const std::vector<std::string>& Settings::get_aliases(const char* desktop_id) const
{
	static const std::vector<std::string> empty;
	if (!desktop_id)
		return empty;
	auto it = m_aliases.find(desktop_id);
	return (it != m_aliases.end()) ? it->second : empty;
}

//-----------------------------------------------------------------------------

void Settings::set_aliases(const std::string& desktop_id,
                            const std::vector<std::string>& terms)
{
	if (terms.empty())
		m_aliases.erase(desktop_id);
	else
		m_aliases[desktop_id] = terms;
}

//-----------------------------------------------------------------------------

void Settings::load_aliases(XfconfChannel* ch)
{
	m_aliases.clear();
	if (!ch)
		return;

	GHashTable* props = xfconf_channel_get_properties(ch, nullptr);
	if (!props)
		return;

	GHashTableIter iter;
	gpointer key_ptr, val_ptr;
	g_hash_table_iter_init(&iter, props);
	while (g_hash_table_iter_next(&iter, &key_ptr, &val_ptr))
	{
		const gchar* prop = static_cast<const gchar*>(key_ptr);

		// Locate "/search/aliases/" anywhere in the full property path
		// (the hash table keys include the channel's property base prefix)
		const gchar* aliases_prefix = "/search/aliases/";
		const gchar* start = strstr(prop, aliases_prefix);
		if (!start)
			continue;

		const gchar* rest = start + strlen(aliases_prefix);
		if (!*rest)
			continue;

		// Must end with "/terms"
		const gchar* terms_suffix = "/terms";
		const gsize rest_len = strlen(rest);
		const gsize suf_len  = strlen(terms_suffix);
		if (rest_len <= suf_len)
			continue;
		if (strcmp(rest + rest_len - suf_len, terms_suffix) != 0)
			continue;

		// Extract desktop-id between "/search/aliases/" and "/terms"
		std::string desktop_id(rest, rest_len - suf_len);
		if (desktop_id.empty())
			continue;

		// Read the string list via a relative path (channel handles base prefix)
		std::string rel = "/search/aliases/" + desktop_id + "/terms";
		gchar** arr = xfconf_channel_get_string_list(ch, rel.c_str());
		if (!arr)
			continue;

		std::vector<std::string> terms_vec;
		for (int i = 0; arr[i]; ++i)
		{
			if (arr[i][0])
				terms_vec.emplace_back(arr[i]);
		}
		g_strfreev(arr);

		if (!terms_vec.empty())
			m_aliases[desktop_id] = std::move(terms_vec);
	}

	g_hash_table_destroy(props);
}

//-----------------------------------------------------------------------------

void Settings::save_aliases(XfconfChannel* ch)
{
	if (!ch)
		return;

	// Clear all existing alias entries
	xfconf_channel_reset_property(ch, "/search/aliases", true);

	begin_property_update();
	for (const auto& kv : m_aliases)
	{
		if (kv.second.empty())
			continue;

		std::string prop = "/search/aliases/" + kv.first + "/terms";

		const int count = static_cast<int>(kv.second.size());
		GPtrArray* array = g_ptr_array_sized_new(count);
		for (const auto& term : kv.second)
		{
			GValue* gval = g_new0(GValue, 1);
			g_value_init(gval, G_TYPE_STRING);
			g_value_set_static_string(gval, term.c_str());
			g_ptr_array_add(array, gval);
		}
		xfconf_channel_set_arrayv(ch, prop.c_str(), array);
		xfconf_array_free(array);
	}
	end_property_update();
}

//-----------------------------------------------------------------------------

/* migrate_schema:
 * @is_fresh_install: true when no Xfconf properties were present at load.
 *
 * Walks the channel forward through every known schema version, applying
 * additive migrations. Versions are cumulative: each block runs once per
 * upgrade. See .specify/specs/003-properties-refactor/contracts/xfconf-keys.md
 * for the v2 contract.
 */
void Settings::migrate_schema(bool is_fresh_install)
{
	if (!channel)
		return;

	const int current_schema = 3;
	if (schema_version >= current_schema)
		return;

	begin_property_update();

	if (schema_version < 1)
	{
		// Map legacy menu-opacity → categories-opacity if present and categories-opacity missing
		if (xfconf_channel_has_property(channel, "/menu-opacity")
				&& !xfconf_channel_has_property(channel, "/categories-opacity"))
		{
			const int legacy_opacity = xfconf_channel_get_int(channel, "/menu-opacity", 100);
			xfconf_channel_set_int(channel, "/categories-opacity", legacy_opacity);
			categories_opacity = legacy_opacity;
		}

		// Write defaults for V1 properties not yet in the channel
		struct { const char* prop; int val; } int_props[] = {
			{ "/corner-radius",       0   },
			{ "/panel-gap",           0   },
			{ "/categories-opacity",  100 },
			{ "/apps-opacity",        100 },
			{ "/grid-columns",        4   },
			{ "/grid-rows",           3   },
		};
		for (auto& p : int_props)
		{
			if (!xfconf_channel_has_property(channel, p.prop))
				xfconf_channel_set_int(channel, p.prop, p.val);
		}

		// NOTE: defaults for V1 Boolean properties not yet in the channel;
		// /unified-bar default false matches the C++ Settings ctor and lets
		// downstream consumers read the key explicitly after first migration.
		struct { const char* prop; gboolean val; } bool_props[] = {
			{ "/unified-bar", FALSE },
		};
		for (auto& p : bool_props)
		{
			if (!xfconf_channel_has_property(channel, p.prop))
				xfconf_channel_set_bool(channel, p.prop, p.val);
		}

		struct { const char* prop; const char* val; } str_props[] = {
			{ "/sidebar-position",     "left"      },
			{ "/search-bar-position",  "top"       },
			{ "/profile-position",     "top"       },
			{ "/commands-position",    "top-right" },
			{ "/grid-density",         "medium"    },
			{ "/layout-mode",          "docked"    },
		};
		for (auto& p : str_props)
		{
			if (!xfconf_channel_has_property(channel, p.prop))
				xfconf_channel_set_string(channel, p.prop, p.val);
		}

		if (is_fresh_install)
			apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *this);
		else
			current_preset_id = "classic";

		schema_version = 1;
	}

	if (schema_version < 2)
	{
		// Seed /full-screen-opacity (new key). Default 100; honour active preset if it pins one.
		// NOTE: at this point preset.cpp's file-seeded values are not yet read — the active
		// preset's compiled default is used as the fallback. Users who want a custom value
		// can edit it via the Properties dialog after upgrade.
		if (!xfconf_channel_has_property(channel, "/full-screen-opacity"))
			xfconf_channel_set_int(channel, "/full-screen-opacity", 100);

		// Deprecate /position-categories-horizontal: subsumed by /sidebar-position ∈ {top, bottom}.
		// If the user had it on AND sidebar-position is left|right (or unset), promote sidebar-position
		// to "top". Then reset the dead key to false so future reads are inert.
		if (xfconf_channel_has_property(channel, "/position-categories-horizontal")
				&& xfconf_channel_get_bool(channel, "/position-categories-horizontal", false))
		{
			gchar* current_sidebar = xfconf_channel_get_string(channel, "/sidebar-position", nullptr);
			const bool sidebar_is_vertical = !current_sidebar
					|| g_strcmp0(current_sidebar, "left") == 0
					|| g_strcmp0(current_sidebar, "right") == 0;
			if (sidebar_is_vertical)
			{
				xfconf_channel_set_string(channel, "/sidebar-position", "top");
				sidebar_position = "top";
			}
			g_free(current_sidebar);
			xfconf_channel_set_bool(channel, "/position-categories-horizontal", false);
			position_categories_horizontal = false;
		}

		// Deprecate profile-shape = Hidden (enum value 2): subsumed by /profile-position = "hidden".
		// HACK: we keep the ProfileHidden enum value compiled in to avoid breaking any third-party
		// preset file that still references the integer 2, but the UI no longer exposes it.
		if (xfconf_channel_has_property(channel, "/profile-shape")
				&& xfconf_channel_get_int(channel, "/profile-shape", ProfileRound) == ProfileHidden)
		{
			xfconf_channel_set_string(channel, "/profile-position", "hidden");
			profile_position = "hidden";
			xfconf_channel_set_int(channel, "/profile-shape", ProfileRound);
			profile_shape = ProfileRound;
		}

		schema_version = 2;
	}

	if (schema_version < 3)
	{
		// Milestone 005 — Places mode keys. Seed defaults on first upgrade so
		// existing installs see consistent values without re-applying a preset.
		struct { const char* prop; gboolean val; } bool_props[] = {
			{ "/places/enabled",            FALSE },
			{ "/places/history-enabled",    TRUE  },
			{ "/places/favourites-enabled", TRUE  },
			{ "/places/remember-last-mode", FALSE },
			{ "/places/show-metadata",      FALSE },
		};
		for (auto& p : bool_props)
		{
			if (!xfconf_channel_has_property(channel, p.prop))
				xfconf_channel_set_bool(channel, p.prop, p.val);
		}

		struct { const char* prop; const char* val; } str_props[] = {
			{ "/places/favourite-sync", "meowmenu" },
			{ "/places/last-mode",      "apps"     },
		};
		for (auto& p : str_props)
		{
			if (!xfconf_channel_has_property(channel, p.prop))
				xfconf_channel_set_string(channel, p.prop, p.val);
		}

		if (!xfconf_channel_has_property(channel, "/places/max-items"))
			xfconf_channel_set_int(channel, "/places/max-items", 20);

		schema_version = 3;
	}

	end_property_update();
}

//-----------------------------------------------------------------------------
