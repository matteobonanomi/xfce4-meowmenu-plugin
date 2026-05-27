/*
 * Copyright (C) 2026 MeowMenu contributors
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

#include "preset.h"
#include "preset-io.h"
#include "settings.h"

#include <cstring>
#include <map>

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

// ---------------------------------------------------------------------------
// In-memory cache of user presets (rebuilt by enumerate_user_presets).
// ---------------------------------------------------------------------------
static std::vector<LayoutPreset> g_user_presets;

// ---------------------------------------------------------------------------
// In-memory cache of file-seeded built-in presets (filled by initialize_file_presets).
// When non-empty, these shadow BUILTIN_PRESETS[] for matching ids.
// ---------------------------------------------------------------------------
static std::vector<LayoutPreset> g_file_presets;

// ---------------------------------------------------------------------------
// Helper: build a PresetValueMap from a brace-enclosed initializer list.
// ---------------------------------------------------------------------------

static PresetValueMap make_values(std::initializer_list<std::pair<const char*, PresetValue>> items)
{
	PresetValueMap m;
	for (auto& item : items)
		m[item.first] = item.second;
	return m;
}

// ---------------------------------------------------------------------------
// Built-in preset definitions — values from data-model.md §"Mappa proprietà ↔ preset"
// Properties with (=) are absent from the map (inherit from current config).
// ---------------------------------------------------------------------------

const LayoutPreset WhiskerMenu::BUILTIN_PRESETS[PRESET_BUILTIN_COUNT] = {
	// PRESET_CLASSIC
	{
		"classic",
		N_("Classic"),
		N_("Traditional Whisker Menu layout. Compact window, sidebar on the right, apps as a list."),
		true,
		make_values({
			{ "corner-radius",        PresetValue::from_int(0)              },
			{ "panel-gap",            PresetValue::from_int(0)              },
			{ "categories-opacity",   PresetValue::from_int(100)            },
			{ "apps-opacity",         PresetValue::from_int(100)            },
			{ "sidebar-position",     PresetValue::from_str("right")        },
			{ "position-categories-horizontal", PresetValue::from_bool(false) },
			{ "search-bar-position",  PresetValue::from_str("top")          },
			{ "profile-position",     PresetValue::from_str("top")          },
			{ "commands-position",    PresetValue::from_str("top-right")    },
			{ "layout-mode",          PresetValue::from_str("docked")       },
			{ "unified-bar",          PresetValue::from_bool(false)         },
			{ "launcher-icon-size",   PresetValue::from_int(2)              }, // Small
			{ "hover-switch-category",PresetValue::from_bool(false)         },
			{ "view-mode-default",    PresetValue::from_str("list")         },
			{ "default-category",     PresetValue::from_str("favorites")    },
			{ "stay-on-focus-out",    PresetValue::from_bool(false)         },
			{ "menu-width",           PresetValue::from_int(450)            },
			{ "menu-height",          PresetValue::from_int(500)            },
			{ "places-enabled",       PresetValue::from_bool(false)         },
		})
	},
	// PRESET_MODERN
	{
		"modern",
		N_("Modern"),
		N_("Contemporary layout with rounded corners, categories on the left, and hover-to-switch enabled."),
		true,
		make_values({
			{ "corner-radius",        PresetValue::from_int(12)            },
			{ "panel-gap",            PresetValue::from_int(8)             },
			{ "categories-opacity",   PresetValue::from_int(80)            },
			{ "apps-opacity",         PresetValue::from_int(70)            },
			{ "sidebar-position",     PresetValue::from_str("left")        },
			{ "position-categories-horizontal", PresetValue::from_bool(false) },
			{ "search-bar-position",  PresetValue::from_str("bottom")      },
			{ "profile-position",     PresetValue::from_str("top")         },
			{ "commands-position",    PresetValue::from_str("top-right")   },
			{ "layout-mode",          PresetValue::from_str("docked")      },
			{ "unified-bar",          PresetValue::from_bool(false)        },
			{ "launcher-icon-size",   PresetValue::from_int(3)             }, // Normal
			{ "grid-density",         PresetValue::from_str("medium")      },
			{ "hover-switch-category",PresetValue::from_bool(true)         },
			{ "view-mode-default",    PresetValue::from_str("icons")       },
			{ "default-category",     PresetValue::from_str("recent")      },
			{ "stay-on-focus-out",    PresetValue::from_bool(false)        },
			{ "menu-width",           PresetValue::from_int(520)           },
			{ "menu-height",          PresetValue::from_int(500)           },
			{ "places-enabled",       PresetValue::from_bool(true)         },
		})
	},
	// PRESET_FULLSCREEN
	{
		"fullscreen",
		N_("Full Screen"),
		N_("Launcher fills the whole screen with a centered grid and categories on the left."),
		true,
		make_values({
			{ "corner-radius",        PresetValue::from_int(0)              },
			{ "panel-gap",            PresetValue::from_int(0)              },
			{ "categories-opacity",   PresetValue::from_int(100)            },
			{ "apps-opacity",         PresetValue::from_int(100)            },
			{ "sidebar-position",     PresetValue::from_str("left")         },
			{ "position-categories-horizontal", PresetValue::from_bool(false) },
			{ "search-bar-position",  PresetValue::from_str("top")          },
			{ "profile-position",     PresetValue::from_str("top")          },
			{ "commands-position",    PresetValue::from_str("top-right")    },
			{ "launcher-icon-size",   PresetValue::from_int(4)              }, // Large
			{ "grid-density",         PresetValue::from_str("medium")       },
			{ "layout-mode",          PresetValue::from_str("fullscreen")   },
			{ "unified-bar",          PresetValue::from_bool(true)          },
			{ "hover-switch-category",PresetValue::from_bool(true)          },
			{ "view-mode-default",    PresetValue::from_str("icons")        },
			{ "default-category",     PresetValue::from_str("all")          },
			{ "stay-on-focus-out",    PresetValue::from_bool(false)         },
			{ "places-enabled",       PresetValue::from_bool(true)          },
		})
	},
};

// ---------------------------------------------------------------------------
// apply_preset: write each preset value to the matching Settings field.
// One reload_menu() call is expected from the caller after this returns.
// ---------------------------------------------------------------------------

void WhiskerMenu::apply_preset(const LayoutPreset& preset, Settings& settings)
{
	for (const auto& kv : preset.values)
	{
		const std::string& prop = kv.first;
		const PresetValue& val  = kv.second;

		if (prop == "corner-radius" && val.kind == PresetValue::Int)
			settings.corner_radius = val.i;
		else if (prop == "panel-gap" && val.kind == PresetValue::Int)
			settings.panel_gap = val.i;
		else if (prop == "categories-opacity" && val.kind == PresetValue::Int)
			settings.categories_opacity = val.i;
		else if (prop == "apps-opacity" && val.kind == PresetValue::Int)
			settings.apps_opacity = val.i;
		else if (prop == "sidebar-position" && val.kind == PresetValue::Str)
			settings.sidebar_position = val.s;
		else if (prop == "position-categories-horizontal" && val.kind == PresetValue::Bool)
			settings.position_categories_horizontal = val.b;
		else if (prop == "search-bar-position" && val.kind == PresetValue::Str)
			settings.search_bar_position = val.s;
		else if (prop == "profile-position" && val.kind == PresetValue::Str)
			settings.profile_position = val.s;
		else if (prop == "commands-position" && val.kind == PresetValue::Str)
			settings.commands_position = val.s;
		else if (prop == "unified-bar" && val.kind == PresetValue::Bool)
			settings.unified_bar = val.b;
		else if (prop == "grid-density" && val.kind == PresetValue::Str)
			settings.grid_density = val.s;
		else if (prop == "layout-mode" && val.kind == PresetValue::Str)
			settings.layout_mode = val.s;
		else if (prop == "launcher-icon-size" && val.kind == PresetValue::Int)
			settings.launcher_icon_size = val.i;
		else if (prop == "hover-switch-category" && val.kind == PresetValue::Bool)
			settings.category_hover_activate = val.b;
		else if (prop == "view-mode-default" && val.kind == PresetValue::Str)
		{
			if (val.s == "icons")
				settings.view_mode = Settings::ViewAsIcons;
			else if (val.s == "tree")
				settings.view_mode = Settings::ViewAsTree;
			else
				settings.view_mode = Settings::ViewAsList;
		}
		else if (prop == "default-category" && val.kind == PresetValue::Str)
		{
			if (val.s == "recent")
				settings.default_category = Settings::CategoryRecent;
			else if (val.s == "all")
				settings.default_category = Settings::CategoryAll;
			else
				settings.default_category = Settings::CategoryFavorites;
		}
		else if (prop == "stay-on-focus-out" && val.kind == PresetValue::Bool)
			settings.stay_on_focus_out = val.b;
		else if (prop == "menu-width" && val.kind == PresetValue::Int)
			settings.menu_width = val.i;
		else if (prop == "menu-height" && val.kind == PresetValue::Int)
			settings.menu_height = val.i;
		else if (prop == "full-screen-opacity" && val.kind == PresetValue::Int)
			settings.full_screen_opacity = val.i;
		else if (prop == "places-enabled" && val.kind == PresetValue::Bool)
			settings.places_enabled = val.b;
	}

	settings.current_preset_id = preset.id;
}

// ---------------------------------------------------------------------------
// initialize_file_presets / get_file_presets — T040
// ---------------------------------------------------------------------------

/* initialize_file_presets:
 *
 * Loads built-in .meowpreset files from the system data directory and from
 * the user-level drop-in folder. File-loaded entries shadow BUILTIN_PRESETS[]
 * by id; BUILTIN_PRESETS[] remains the fallback when files are absent or all
 * malformed (FR-063).
 *
 * Call once at startup (SettingsDialog constructor) before the preset combo
 * is populated.
 */
void WhiskerMenu::initialize_file_presets()
{
	std::string sys_dir  = std::string(PACKAGE_DATADIR) + G_DIR_SEPARATOR_S + "presets";
	std::string user_dir = std::string(g_get_user_data_dir())
		+ G_DIR_SEPARATOR_S + "meowmenu" + G_DIR_SEPARATOR_S + "presets";

	g_file_presets = enumerate_preset_files(sys_dir, user_dir);

	// Merge any built-in id not covered by a file entry from BUILTIN_PRESETS[].
	for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
	{
		bool covered = false;
		for (const auto& p : g_file_presets)
		{
			if (p.id == BUILTIN_PRESETS[i].id)
			{
				covered = true;
				break;
			}
		}
		if (!covered)
			g_file_presets.push_back(BUILTIN_PRESETS[i]);
	}
}

const std::vector<LayoutPreset>& WhiskerMenu::get_file_presets()
{
	return g_file_presets;
}

// ---------------------------------------------------------------------------
// find_preset_by_id: file presets first, then C++ fallback, then user presets.
// ---------------------------------------------------------------------------

const LayoutPreset* WhiskerMenu::find_preset_by_id(const std::string& id)
{
	// NOTE: g_file_presets already contains merged fallbacks from initialize_file_presets().
	for (const auto& p : g_file_presets)
	{
		if (id == p.id)
			return &p;
	}
	// Fallback for callers that query before initialize_file_presets() runs.
	for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
	{
		if (id == BUILTIN_PRESETS[i].id)
			return &BUILTIN_PRESETS[i];
	}
	for (const auto& p : g_user_presets)
	{
		if (id == p.id)
			return &p;
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// compute_preset_diff: true if any governed setting differs from preset values.
// ---------------------------------------------------------------------------

bool WhiskerMenu::compute_preset_diff(const LayoutPreset& preset, const Settings& settings)
{
	for (const auto& kv : preset.values)
	{
		const std::string& prop = kv.first;
		const PresetValue& val  = kv.second;

		if (prop == "corner-radius")
		{
			if (val.kind == PresetValue::Int && static_cast<int>(settings.corner_radius) != val.i)
				return true;
		}
		else if (prop == "panel-gap")
		{
			if (val.kind == PresetValue::Int && static_cast<int>(settings.panel_gap) != val.i)
				return true;
		}
		else if (prop == "categories-opacity")
		{
			if (val.kind == PresetValue::Int && static_cast<int>(settings.categories_opacity) != val.i)
				return true;
		}
		else if (prop == "apps-opacity")
		{
			if (val.kind == PresetValue::Int && static_cast<int>(settings.apps_opacity) != val.i)
				return true;
		}
		else if (prop == "sidebar-position")
		{
			if (val.kind == PresetValue::Str && !(settings.sidebar_position == val.s.c_str()))
				return true;
		}
		else if (prop == "position-categories-horizontal")
		{
			if (val.kind == PresetValue::Bool
					&& static_cast<bool>(settings.position_categories_horizontal) != val.b)
				return true;
		}
		else if (prop == "search-bar-position")
		{
			if (val.kind == PresetValue::Str && !(settings.search_bar_position == val.s.c_str()))
				return true;
		}
		else if (prop == "profile-position")
		{
			if (val.kind == PresetValue::Str && !(settings.profile_position == val.s.c_str()))
				return true;
		}
		else if (prop == "commands-position")
		{
			if (val.kind == PresetValue::Str && !(settings.commands_position == val.s.c_str()))
				return true;
		}
		else if (prop == "unified-bar")
		{
			if (val.kind == PresetValue::Bool
					&& static_cast<bool>(settings.unified_bar) != val.b)
				return true;
		}
		else if (prop == "grid-density")
		{
			if (val.kind == PresetValue::Str && !(settings.grid_density == val.s.c_str()))
				return true;
		}
		else if (prop == "layout-mode")
		{
			if (val.kind == PresetValue::Str && !(settings.layout_mode == val.s.c_str()))
				return true;
		}
		else if (prop == "launcher-icon-size")
		{
			if (val.kind == PresetValue::Int && static_cast<int>(settings.launcher_icon_size) != val.i)
				return true;
		}
		else if (prop == "hover-switch-category")
		{
			if (val.kind == PresetValue::Bool && static_cast<bool>(settings.category_hover_activate) != val.b)
				return true;
		}
		else if (prop == "view-mode-default")
		{
			if (val.kind == PresetValue::Str)
			{
				const int live = settings.view_mode;
				const std::string& s = val.s;
				if ((s == "icons" && live != Settings::ViewAsIcons)
						|| (s == "list" && live != Settings::ViewAsList)
						|| (s == "tree" && live != Settings::ViewAsTree))
					return true;
			}
		}
		else if (prop == "default-category")
		{
			if (val.kind == PresetValue::Str)
			{
				const int live = settings.default_category;
				const std::string& s = val.s;
				if ((s == "recent"    && live != Settings::CategoryRecent)
						|| (s == "all" && live != Settings::CategoryAll)
						|| (s == "favorites" && live != Settings::CategoryFavorites))
					return true;
			}
		}
		else if (prop == "stay-on-focus-out")
		{
			if (val.kind == PresetValue::Bool
					&& static_cast<bool>(settings.stay_on_focus_out) != val.b)
				return true;
		}
		else if (prop == "menu-width")
		{
			if (val.kind == PresetValue::Int
					&& static_cast<int>(settings.menu_width) != val.i)
				return true;
		}
		else if (prop == "menu-height")
		{
			if (val.kind == PresetValue::Int
					&& static_cast<int>(settings.menu_height) != val.i)
				return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// enumerate_user_presets: rebuild g_user_presets from Xfconf /presets/.
// ---------------------------------------------------------------------------

const std::vector<LayoutPreset>& WhiskerMenu::enumerate_user_presets(XfconfChannel* channel)
{
	g_user_presets.clear();

	GHashTable* props = xfconf_channel_get_properties(channel, "/presets");
	if (!props)
	{
		return g_user_presets;
	}

	// Group properties by uuid: uuid → (display_name, PresetValueMap)
	std::map<std::string, std::pair<std::string, PresetValueMap>> by_uuid;

	GHashTableIter iter;
	gpointer key_ptr, value_ptr;
	g_hash_table_iter_init(&iter, props);
	while (g_hash_table_iter_next(&iter, &key_ptr, &value_ptr))
	{
		const gchar* path = static_cast<const gchar*>(key_ptr);
		const GValue* gval = static_cast<const GValue*>(value_ptr);

		// path is like "/presets/<uuid>/<prop>" — need at least 3 segments
		if (strncmp(path, "/presets/", 9) != 0)
		{
			continue;
		}
		const gchar* uuid_start = path + 9;
		const gchar* slash = strchr(uuid_start, '/');
		if (!slash || slash == uuid_start || !*(slash + 1))
		{
			continue;
		}
		std::string uuid(uuid_start, slash);
		const gchar* prop_name = slash + 1;

		auto& entry = by_uuid[uuid];
		if (strcmp(prop_name, "display-name") == 0 && G_VALUE_HOLDS_STRING(gval))
		{
			entry.first = g_value_get_string(gval);
		}
		else if (strcmp(prop_name, "created-by") == 0)
		{
			// metadata — skip
		}
		else if (G_VALUE_HOLDS_INT(gval))
		{
			entry.second[prop_name] = PresetValue::from_int(g_value_get_int(gval));
		}
		else if (G_VALUE_HOLDS_BOOLEAN(gval))
		{
			entry.second[prop_name] = PresetValue::from_bool(g_value_get_boolean(gval) != FALSE);
		}
		else if (G_VALUE_HOLDS_STRING(gval))
		{
			const gchar* sv = g_value_get_string(gval);
			entry.second[prop_name] = PresetValue::from_str(sv ? sv : "");
		}
	}

	g_hash_table_unref(props);

	for (auto& pair : by_uuid)
	{
		const std::string& display_name = pair.second.first;
		if (display_name.empty())
		{
			continue; // entry without display-name is invalid
		}
		LayoutPreset p;
		p.id           = pair.first;
		p.display_name = display_name;
		p.description  = "";
		p.is_builtin   = false;
		p.values       = std::move(pair.second.second);
		g_user_presets.push_back(std::move(p));
	}

	return g_user_presets;
}

// ---------------------------------------------------------------------------
// save_current_as_user_preset
// ---------------------------------------------------------------------------

static bool preset_name_conflicts(const std::string& name)
{
	for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
	{
		if (g_ascii_strcasecmp(BUILTIN_PRESETS[i].display_name.c_str(), name.c_str()) == 0)
		{
			return true;
		}
	}
	for (const auto& p : g_user_presets)
	{
		if (g_ascii_strcasecmp(p.display_name.c_str(), name.c_str()) == 0)
		{
			return true;
		}
	}
	return false;
}

static std::string generate_preset_uuid()
{
	gchar buf[17];
	g_snprintf(buf, sizeof(buf), "%08x%08x", g_random_int(), g_random_int());
	return std::string(buf);
}

std::string WhiskerMenu::save_current_as_user_preset(const std::string& display_name,
	Settings& settings)
{
	if (display_name.empty() || preset_name_conflicts(display_name))
	{
		return std::string();
	}

	std::string uuid = generate_preset_uuid();
	std::string prefix = "/presets/" + uuid + "/";
	XfconfChannel* ch = settings.channel;

	// Metadata
	xfconf_channel_set_string(ch, (prefix + "display-name").c_str(), display_name.c_str());
	xfconf_channel_set_string(ch, (prefix + "created-by").c_str(), "meowmenu-" PACKAGE_VERSION);

	// Governed properties
	xfconf_channel_set_int(ch, (prefix + "corner-radius").c_str(), settings.corner_radius);
	xfconf_channel_set_int(ch, (prefix + "panel-gap").c_str(), settings.panel_gap);
	xfconf_channel_set_int(ch, (prefix + "categories-opacity").c_str(), settings.categories_opacity);
	xfconf_channel_set_int(ch, (prefix + "apps-opacity").c_str(), settings.apps_opacity);
	xfconf_channel_set_string(ch, (prefix + "sidebar-position").c_str(), settings.sidebar_position);
	xfconf_channel_set_bool(ch, (prefix + "position-categories-horizontal").c_str(),
		static_cast<bool>(settings.position_categories_horizontal));
	xfconf_channel_set_string(ch, (prefix + "search-bar-position").c_str(), settings.search_bar_position);
	xfconf_channel_set_string(ch, (prefix + "profile-position").c_str(), settings.profile_position);
	xfconf_channel_set_string(ch, (prefix + "commands-position").c_str(), settings.commands_position);
	xfconf_channel_set_string(ch, (prefix + "grid-density").c_str(), settings.grid_density);
	xfconf_channel_set_string(ch, (prefix + "layout-mode").c_str(), settings.layout_mode);
	xfconf_channel_set_bool(ch, (prefix + "unified-bar").c_str(),
		static_cast<bool>(settings.unified_bar));
	xfconf_channel_set_int(ch, (prefix + "launcher-icon-size").c_str(), settings.launcher_icon_size);
	xfconf_channel_set_bool(ch, (prefix + "hover-switch-category").c_str(),
		static_cast<bool>(settings.category_hover_activate));
	const gchar* vm_str = "list";
	if (static_cast<int>(settings.view_mode) == Settings::ViewAsIcons) vm_str = "icons";
	else if (static_cast<int>(settings.view_mode) == Settings::ViewAsTree) vm_str = "tree";
	xfconf_channel_set_string(ch, (prefix + "view-mode-default").c_str(), vm_str);

	settings.current_preset_id = uuid;
	enumerate_user_presets(ch);
	return uuid;
}

// ---------------------------------------------------------------------------
// rename_user_preset
// ---------------------------------------------------------------------------

bool WhiskerMenu::rename_user_preset(const std::string& uuid, const std::string& new_name,
	Settings& settings)
{
	if (new_name.empty())
	{
		return false;
	}

	// Check the preset exists in cache
	bool found = false;
	for (const auto& p : g_user_presets)
	{
		if (p.id == uuid) { found = true; break; }
	}
	if (!found)
	{
		return false;
	}

	// Name conflict check (excluding this preset)
	for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
	{
		if (g_ascii_strcasecmp(BUILTIN_PRESETS[i].display_name.c_str(), new_name.c_str()) == 0)
		{
			return false;
		}
	}
	for (const auto& p : g_user_presets)
	{
		if (p.id != uuid &&
			g_ascii_strcasecmp(p.display_name.c_str(), new_name.c_str()) == 0)
		{
			return false;
		}
	}

	std::string path = "/presets/" + uuid + "/display-name";
	xfconf_channel_set_string(settings.channel, path.c_str(), new_name.c_str());
	enumerate_user_presets(settings.channel);
	return true;
}

// ---------------------------------------------------------------------------
// delete_user_preset
// ---------------------------------------------------------------------------

void WhiskerMenu::delete_user_preset(const std::string& uuid, Settings& settings)
{
	// Constraint 4: if the deleted preset is current, clear current_preset_id
	const gchar* current = static_cast<const gchar*>(settings.current_preset_id);
	if (current && uuid == current)
	{
		settings.current_preset_id = "";
	}

	std::string path = "/presets/" + uuid;
	xfconf_channel_reset_property(settings.channel, path.c_str(), TRUE);
	enumerate_user_presets(settings.channel);
}
