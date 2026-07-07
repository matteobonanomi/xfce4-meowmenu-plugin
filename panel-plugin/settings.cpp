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

#include "launcher/command.h"
#include "core/plugin.h"
#include "core/user-session-layout.h"
#include "presets/preset.h"
#include "search/search-action.h"
#include "ui/slot.h"

#include <algorithm>
#include <sstream>

#include <cstdio>
#include <cstring>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

namespace
{

static bool property_matches_any(const gchar* property, const char* const* values)
{
	for (const char* const* value = values; *value; ++value)
	{
		if (g_strcmp0(property, *value) == 0)
		{
			return true;
		}
	}
	return false;
}

}

//-----------------------------------------------------------------------------

/* classify_reload_intent:
 * @property: base-relative Xfconf property path such as "/profile-position".
 *
 * Classifies a setting notification by the narrowest live refresh that can make
 * the current UI coherent. Layout refreshes reuse existing widgets and keep the
 * current Garcon category epoch; content refreshes rebuild application/category
 * data; button refreshes touch only the panel button.
 *
 * Returns: the refresh intent for @property, or ReloadIntent::None when the
 * setting has no immediate menu/window presentation side effect.
 */
ReloadIntent WhiskerMenu::classify_reload_intent(const gchar* property)
{
	if (!property)
	{
		return ReloadIntent::None;
	}

	static const char* const button_properties[] = {
		"/button-title",
		"/button-icon",
		"/show-button-title",
		"/show-button-icon",
		"/button-single-row",
		nullptr
	};
	static const char* const content_properties[] = {
		"/favorites",
		"/recent",
		"/custom-menu-file",
		"/launcher-show-name",
		"/launcher-show-description",
		"/sort-categories",
		"/view-mode",
		"/favorites-in-recent",
		nullptr
	};
	static const char* const layout_properties[] = {
		"/launcher-show-tooltip",
		"/launcher-icon-size",
		"/category-show-name",
		"/category-icon-size",
		"/default-category",
		"/recent-items-max",
		"/position-profile-alternate",
		"/position-search-alternate",
		"/position-commands-alternate",
		"/position-categories-alternate",
		"/position-categories-horizontal",
		"/stay-on-focus-out",
		"/profile-shape",
		"/confirm-session-command",
		"/menu-width",
		"/menu-height",
		"/menu-opacity",
		"/corner-radius",
		"/panel-gap",
		"/sidebar-position",
		"/sidebar-enabled",
		"/search-bar-position",
		"/profile-position",
		"/commands-position",
		"/grid-density",
		"/layout-mode",
		"/places/enabled",
		"/places/history-enabled",
		"/places/favourites-enabled",
		"/places/favourite-sync",
		"/places/max-items",
		"/places/remember-last-mode",
		"/places/last-mode",
		"/places/favourites",
		"/places/switch-show-icons",
		nullptr
	};

	if (property_matches_any(property, button_properties))
	{
		return ReloadIntent::Button;
	}
	if (property_matches_any(property, content_properties))
	{
		return ReloadIntent::Content;
	}
	if (property_matches_any(property, layout_properties)
			|| g_str_has_prefix(property, "/command-")
			|| g_str_has_prefix(property, "/show-command-"))
	{
		return ReloadIntent::Layout;
	}
	return ReloadIntent::None;
}

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
	initialized(this, "/initialized", false),

	corner_radius(this, "/corner-radius", 0, 0, 24),
	panel_gap(this, "/panel-gap", 0, 0, 50),

	// NOTE: GUI/preset write domain is {left,right,top,bottom}; the legacy
	// "hidden" value is tolerated on read (free-form String, migrated to
	// sidebar-enabled=false) and is never honoured as a position.
	sidebar_position(this, "/sidebar-position", "left"),
	sidebar_enabled(this, "/sidebar-enabled", true),
	search_bar_position(this, "/search-bar-position", "top"),
	profile_position(this, "/profile-position", "top-left"),
	commands_position(this, "/commands-position", "top-right"),

	grid_density(this, "/grid-density", "medium"),

	layout_mode(this, "/layout-mode", "docked"),

	places_enabled(this, "/places/enabled", false),
	places_history_enabled(this, "/places/history-enabled", true),
	places_favourites_enabled(this, "/places/favourites-enabled", true),
	places_favourite_sync(this, "/places/favourite-sync", "meowmenu"),
	places_max_items(this, "/places/max-items", 20, 0, 30),
	places_remember_last_mode(this, "/places/remember-last-mode", false),
	places_last_mode(this, "/places/last-mode", "apps"),
	places_favourites(this, "/places/favourites", { }),
	places_switch_show_icons(this, "/places/switch-show-icons", false)
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
	// NOTE: the persisted /initialized marker — not the raw property count — is
	// the authoritative fresh-vs-upgrade signal. The count is fragile: a
	// still-running xfconfd that served stale in-memory state (or panel-seeded
	// keys) makes a genuine first run look non-empty. Passing both lets
	// migrate_schema treat "marker absent AND empty channel" as the only fresh
	// case while still back-filling the marker for existing users.
	migrate_schema(initialized, loaded_property_count == 0);
}

//-----------------------------------------------------------------------------

/* Settings::current_preset_name:
 *
 * Resolves the active preset's stored identity name for display as the
 * active-preset label. A single code path serves built-ins (localized display
 * name) and custom presets (user-entered stored name). When the id is
 * unset/empty or resolves to no known preset (e.g. a deleted custom one), the
 * localized "Custom" label is returned so the field is never blank.
 *
 * Returns: the name to show; never empty, never the hard-coded "Classic"
 * default. The "Custom" wording matches save_current_as_user_preset.
 */
std::string Settings::current_preset_name() const
{
	const gchar* id = static_cast<const gchar*>(current_preset_id);
	const std::string sid = id ? id : "";
	if (!sid.empty())
	{
		const LayoutPreset* p = find_preset_by_id(sid);
		if (p)
			return p->name.empty() ? p->display_name : p->name;
	}
	return _("Custom");
}

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

	// Normalise the Profile/Commands edge coupling toward the governing edge
	// (the Profile edge when docked, the search-bar edge when full-screen) and
	// persist any snapped value. The pure helper is the single authority shared
	// with the renderer and the Preferences combos, so a stored or live-edited
	// disallowed combination resolves the same way everywhere (FR-014/FR-017).
	//
	// NOTE: the snapped value is written back through the existing
	// /profile-position and /commands-position keys (no new key) so the stored
	// configuration and the rendered row stay in sync — reopening Preferences
	// shows the resolved value. A "hidden" component is never un-hidden here;
	// only the edge of an already-visible component moves.
	{
		const LayoutMode mode = (g_strcmp0(layout_mode, "fullscreen") == 0)
				? LayoutMode::FullScreen : LayoutMode::Docked;
		const UserSessionResolution res = normalize_user_session(
				mode, search_bar_position, profile_position, commands_position);
		if (res.profile_changed)
		{
			profile_position = res.profile_position;
		}
		if (res.commands_changed)
		{
			commands_position = res.commands_position;
		}
	}
}

//-----------------------------------------------------------------------------

void Settings::property_changed(const gchar* property, const GValue* value)
{
	bool reload = true;
	bool changed = false;
	if (favorites.load(property, value, reload)
			|| recent.load(property, value, reload)
			|| launcher_show_name.load(property, value)
			|| launcher_show_description.load(property, value)
			|| sort_categories.load(property, value)
			|| view_mode.load(property, value))
	{
		changed = true;
	}

	else if (button_title.load(property, value)
			|| button_icon_name.load(property, value)
			|| button_title_visible.load(property, value)
			|| button_icon_visible.load(property, value)
			|| button_single_row.load(property, value))
	{
		changed = true;
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
			|| initialized.load(property, value)
			|| corner_radius.load(property, value)
			|| panel_gap.load(property, value)
			|| sidebar_position.load(property, value)
			|| sidebar_enabled.load(property, value)
			|| search_bar_position.load(property, value)
			|| profile_position.load(property, value)
			|| commands_position.load(property, value)
			|| grid_density.load(property, value)
			|| layout_mode.load(property, value)
			|| places_enabled.load(property, value)
			|| places_history_enabled.load(property, value)
			|| places_favourites_enabled.load(property, value)
			|| places_favourite_sync.load(property, value)
			|| places_max_items.load(property, value)
			|| places_remember_last_mode.load(property, value)
			|| places_last_mode.load(property, value)
			|| places_favourites.load(property, value, reload)
			|| places_switch_show_icons.load(property, value))
	{
		changed = true;
	}

	else
	{
		for (auto i : command)
		{
			if (i->load(property, value))
			{
				changed = true;
				break;
			}
		}
	}

	if (!changed)
	{
		return;
	}

	switch (classify_reload_intent(property))
	{
	case ReloadIntent::Button:
		m_plugin->reload_button();
		break;
	case ReloadIntent::Layout:
		m_plugin->refresh_layout();
		break;
	case ReloadIntent::Content:
		if (reload)
		{
			m_plugin->reload_menu();
		}
		break;
	case ReloadIntent::None:
		break;
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
