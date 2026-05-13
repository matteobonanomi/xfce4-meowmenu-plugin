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

#include "preset-io.h"
#include "preset.h"
#include "settings.h"

#include <cstring>
#include <map>

#include <glib.h>

using namespace WhiskerMenu;

// ---------------------------------------------------------------------------
// Property schema table — used both for export ordering and import validation.
// ---------------------------------------------------------------------------

struct PropDef
{
	const char* name;
	PresetValue::Kind kind;
	// For Int: [min, max]. For Str: domain array. For Bool: unused.
	int min_i;
	int max_i;
	const char* const* domain;
	int domain_len;
};

static const char* SIDEBAR_DOMAIN[]         = { "left", "right", "hidden" };
static const char* SEARCHBAR_DOMAIN[]       = { "top", "bottom" };
static const char* PROFILE_DOMAIN[]         = { "top", "bottom", "bottom-right", "hidden" };
static const char* COMMANDS_DOMAIN[]        = { "top-right", "bottom-right", "hidden" };
static const char* GRID_DENSITY_DOMAIN[]    = { "low", "medium", "high" };
static const char* LAYOUT_MODE_DOMAIN[]     = { "docked", "fullscreen" };
static const char* VIEW_MODE_DOMAIN[]       = { "icons", "list", "tree" };
static const char* DEFAULT_CATEGORY_DOMAIN[] = { "favorites", "recent", "all" };

#define STR_DOMAIN(arr) (arr), (int)(sizeof(arr)/sizeof(arr[0]))
#define INT_RANGE(lo, hi) (lo), (hi), nullptr, 0

static const PropDef GOVERNED_PROPS[] = {
	{ "corner-radius",         PresetValue::Int, INT_RANGE(0, 24) },
	{ "panel-gap",             PresetValue::Int, INT_RANGE(0, 50) },
	{ "categories-opacity",    PresetValue::Int, INT_RANGE(0, 100) },
	{ "apps-opacity",          PresetValue::Int, INT_RANGE(0, 100) },
	{ "full-screen-opacity",   PresetValue::Int, INT_RANGE(0, 100) },
	{ "sidebar-position",      PresetValue::Str, 0, 0, STR_DOMAIN(SIDEBAR_DOMAIN) },
	{ "position-categories-horizontal", PresetValue::Bool, 0, 0, nullptr, 0 },
	{ "search-bar-position",   PresetValue::Str, 0, 0, STR_DOMAIN(SEARCHBAR_DOMAIN) },
	{ "profile-position",      PresetValue::Str, 0, 0, STR_DOMAIN(PROFILE_DOMAIN) },
	{ "commands-position",     PresetValue::Str, 0, 0, STR_DOMAIN(COMMANDS_DOMAIN) },
	{ "grid-density",          PresetValue::Str, 0, 0, STR_DOMAIN(GRID_DENSITY_DOMAIN) },
	{ "layout-mode",           PresetValue::Str, 0, 0, STR_DOMAIN(LAYOUT_MODE_DOMAIN) },
	{ "launcher-icon-size",    PresetValue::Int, INT_RANGE(-1, 6) },
	{ "view-mode-default",     PresetValue::Str, 0, 0, STR_DOMAIN(VIEW_MODE_DOMAIN) },
	{ "hover-switch-category", PresetValue::Bool, 0, 0, nullptr, 0 },
	{ "stay-on-focus-out",     PresetValue::Bool, 0, 0, nullptr, 0 },
	{ "menu-width",            PresetValue::Int, INT_RANGE(200, 2000) },
	{ "menu-height",           PresetValue::Int, INT_RANGE(200, 2000) },
	{ "default-category",      PresetValue::Str, 0, 0, STR_DOMAIN(DEFAULT_CATEGORY_DOMAIN) },
};
static const int GOVERNED_PROPS_COUNT = (int)(sizeof(GOVERNED_PROPS) / sizeof(GOVERNED_PROPS[0]));

static const PropDef* find_prop_def(const char* name)
{
	for (int i = 0; i < GOVERNED_PROPS_COUNT; ++i)
	{
		if (strcmp(GOVERNED_PROPS[i].name, name) == 0)
			return &GOVERNED_PROPS[i];
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// export_user_preset
// ---------------------------------------------------------------------------

bool WhiskerMenu::export_user_preset(const std::string& uuid,
	const std::string& dest_path,
	Settings& settings)
{
	enumerate_user_presets(settings.channel);
	const LayoutPreset* p = find_preset_by_id(uuid);
	if (!p || p->is_builtin)
		return false;

	GKeyFile* kf = g_key_file_new();

	// [Preset] section
	g_key_file_set_string(kf, "Preset", "Name", p->display_name.c_str());
	g_key_file_set_integer(kf, "Preset", "SchemaVersion", 1);
	g_key_file_set_string(kf, "Preset", "CreatedBy", "meowmenu-" PACKAGE_VERSION);

	GDateTime* now = g_date_time_new_now_utc();
	gchar* ts = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
	g_key_file_set_string(kf, "Preset", "ExportedAt", ts ? ts : "");
	g_free(ts);
	g_date_time_unref(now);

	// [Settings] section — write known properties in definition order
	for (int i = 0; i < GOVERNED_PROPS_COUNT; ++i)
	{
		const PropDef& pd = GOVERNED_PROPS[i];
		auto it = p->values.find(pd.name);
		if (it == p->values.end())
			continue;
		const PresetValue& v = it->second;
		if (pd.kind == PresetValue::Int && v.kind == PresetValue::Int)
			g_key_file_set_integer(kf, "Settings", pd.name, v.i);
		else if (pd.kind == PresetValue::Str && v.kind == PresetValue::Str)
			g_key_file_set_string(kf, "Settings", pd.name, v.s.c_str());
		else if (pd.kind == PresetValue::Bool && v.kind == PresetValue::Bool)
			g_key_file_set_boolean(kf, "Settings", pd.name, v.b ? TRUE : FALSE);
	}

	gsize len;
	gchar* data = g_key_file_to_data(kf, &len, nullptr);
	g_key_file_free(kf);

	GError* err = nullptr;
	bool ok = g_file_set_contents(dest_path.c_str(), data, (gssize)len, &err) != FALSE;
	if (err)
		g_error_free(err);
	g_free(data);
	return ok;
}

// ---------------------------------------------------------------------------
// import_user_preset
// ---------------------------------------------------------------------------

ImportResult WhiskerMenu::import_user_preset(const std::string& file_path,
	Settings& settings,
	const std::string& display_name_override,
	const std::string& overwrite_uuid)
{
	ImportResult result;

	// Step 1: parse file
	GKeyFile* kf = g_key_file_new();
	GError* err = nullptr;
	if (!g_key_file_load_from_file(kf, file_path.c_str(), G_KEY_FILE_NONE, &err))
	{
		result.status = ImportStatus::ParseError;
		result.error_message = err ? err->message : "GKeyFile parse failed";
		if (err) g_error_free(err);
		g_key_file_free(kf);
		return result;
	}

	// Step 2: required sections
	if (!g_key_file_has_group(kf, "Preset") || !g_key_file_has_group(kf, "Settings"))
	{
		result.status = ImportStatus::MissingSection;
		result.error_message = "Missing [Preset] or [Settings] section";
		g_key_file_free(kf);
		return result;
	}

	// Step 3: required keys
	if (!g_key_file_has_key(kf, "Preset", "Name", nullptr) ||
		!g_key_file_has_key(kf, "Preset", "SchemaVersion", nullptr))
	{
		result.status = ImportStatus::MissingKey;
		result.error_message = "Missing required key in [Preset] (Name or SchemaVersion)";
		g_key_file_free(kf);
		return result;
	}

	gchar* name_raw = g_key_file_get_string(kf, "Preset", "Name", nullptr);
	std::string display_name = display_name_override.empty()
		? (name_raw ? std::string(name_raw) : std::string())
		: display_name_override;
	g_free(name_raw);
	result.display_name = display_name;

	// Step 4: conflict check (skip when overwriting explicitly)
	if (overwrite_uuid.empty())
	{
		for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
		{
			if (g_ascii_strcasecmp(BUILTIN_PRESETS[i].display_name.c_str(), display_name.c_str()) == 0)
			{
				result.status = ImportStatus::ConflictBuiltin;
				result.error_message = "Name matches a built-in preset";
				g_key_file_free(kf);
				return result;
			}
		}
		const auto& user = enumerate_user_presets(settings.channel);
		for (const auto& p : user)
		{
			if (g_ascii_strcasecmp(p.display_name.c_str(), display_name.c_str()) == 0)
			{
				result.status = ImportStatus::ConflictUser;
				result.conflict_uuid = p.id;
				g_key_file_free(kf);
				return result;
			}
		}
	}

	// Step 5: parse [Settings] with per-key validation
	PresetValueMap values;
	gsize nkeys = 0;
	gchar** keys = g_key_file_get_keys(kf, "Settings", &nkeys, nullptr);
	if (keys)
	{
		for (gsize ki = 0; ki < nkeys; ++ki)
		{
			const gchar* key = keys[ki];
			const PropDef* pd = find_prop_def(key);
			if (!pd)
			{
				g_message("meowmenu: import: unknown setting key '%s', skipping", key);
				continue;
			}

			if (pd->kind == PresetValue::Int)
			{
				GError* verr = nullptr;
				int v = g_key_file_get_integer(kf, "Settings", key, &verr);
				if (verr || v < pd->min_i || v > pd->max_i)
				{
					g_message("meowmenu: import: invalid value for '%s', skipping", key);
					if (verr) g_error_free(verr);
					continue;
				}
				values[key] = PresetValue::from_int(v);
			}
			else if (pd->kind == PresetValue::Bool)
			{
				GError* verr = nullptr;
				gboolean v = g_key_file_get_boolean(kf, "Settings", key, &verr);
				if (verr)
				{
					g_message("meowmenu: import: invalid bool for '%s', skipping", key);
					g_error_free(verr);
					continue;
				}
				values[key] = PresetValue::from_bool(v != FALSE);
			}
			else // Str
			{
				gchar* sv = g_key_file_get_string(kf, "Settings", key, nullptr);
				bool valid = false;
				for (int di = 0; di < pd->domain_len; ++di)
				{
					if (sv && strcmp(sv, pd->domain[di]) == 0)
					{
						valid = true;
						break;
					}
				}
				if (!valid)
				{
					g_message("meowmenu: import: invalid value '%s' for '%s', skipping",
						sv ? sv : "(null)", key);
					g_free(sv);
					continue;
				}
				values[key] = PresetValue::from_str(sv);
				g_free(sv);
			}
		}
		g_strfreev(keys);
	}

	g_key_file_free(kf);

	// Step 6: write to Xfconf
	XfconfChannel* ch = settings.channel;

	std::string uuid;
	if (!overwrite_uuid.empty())
	{
		uuid = overwrite_uuid;
		// Clear existing values first (reset to allow partial overwrite)
		xfconf_channel_reset_property(ch, ("/presets/" + uuid).c_str(), TRUE);
	}
	else
	{
		// Generate new uuid
		gchar buf[17];
		g_snprintf(buf, sizeof(buf), "%08x%08x", g_random_int(), g_random_int());
		uuid = buf;
	}

	std::string prefix = "/presets/" + uuid + "/";
	xfconf_channel_set_string(ch, (prefix + "display-name").c_str(), display_name.c_str());
	xfconf_channel_set_string(ch, (prefix + "created-by").c_str(), "meowmenu-" PACKAGE_VERSION);

	for (const auto& kv : values)
	{
		const std::string& pname = kv.first;
		const PresetValue& pval = kv.second;
		if (pval.kind == PresetValue::Int)
			xfconf_channel_set_int(ch, (prefix + pname).c_str(), pval.i);
		else if (pval.kind == PresetValue::Bool)
			xfconf_channel_set_bool(ch, (prefix + pname).c_str(), pval.b ? TRUE : FALSE);
		else
			xfconf_channel_set_string(ch, (prefix + pname).c_str(), pval.s.c_str());
	}

	enumerate_user_presets(ch);

	result.status = ImportStatus::Ok;
	result.new_uuid = uuid;
	return result;
}

// ---------------------------------------------------------------------------
// enumerate_preset_files: walk system_dir then user_dir, parse every
// .meowpreset file, deduplicate by id (user wins), return the result list.
// ---------------------------------------------------------------------------

/* parse_preset_file_internal:
 * @path:   absolute path to the .meowpreset file to parse.
 * @out:    filled on success.
 *
 * Parses a single .meowpreset GKeyFile. On any validation failure the file
 * is silently ignored (g_debug() only) and false is returned. The [Settings]
 * section is parsed with the same per-key validation used by import_user_preset.
 *
 * Returns: true on success; false if the file should be skipped.
 */
static bool parse_preset_file_internal(const std::string& path, LayoutPreset& out)
{
	GKeyFile* kf = g_key_file_new();
	GError* err = nullptr;

	if (!g_key_file_load_from_file(kf, path.c_str(), G_KEY_FILE_NONE, &err))
	{
		g_debug("meowmenu: preset file '%s': GKeyFile parse failed: %s",
			path.c_str(), err ? err->message : "unknown");
		if (err) g_error_free(err);
		g_key_file_free(kf);
		return false;
	}

	if (!g_key_file_has_group(kf, "Preset") || !g_key_file_has_group(kf, "Settings"))
	{
		g_debug("meowmenu: preset file '%s': missing [Preset] or [Settings] section", path.c_str());
		g_key_file_free(kf);
		return false;
	}

	if (!g_key_file_has_key(kf, "Preset", "Name", nullptr) ||
		!g_key_file_has_key(kf, "Preset", "SchemaVersion", nullptr))
	{
		g_debug("meowmenu: preset file '%s': missing Name or SchemaVersion", path.c_str());
		g_key_file_free(kf);
		return false;
	}

	GError* verr = nullptr;
	int schema_ver = g_key_file_get_integer(kf, "Preset", "SchemaVersion", &verr);
	if (verr || schema_ver < 1)
	{
		g_debug("meowmenu: preset file '%s': invalid SchemaVersion", path.c_str());
		if (verr) g_error_free(verr);
		g_key_file_free(kf);
		return false;
	}
	if (schema_ver > 1)
	{
		g_message("meowmenu: preset file '%s' requires schema version %d (we know 1); skipping — upgrade meowmenu",
			path.c_str(), schema_ver);
		g_key_file_free(kf);
		return false;
	}

	gchar* name_raw = g_key_file_get_string(kf, "Preset", "Name", nullptr);
	if (!name_raw || !*name_raw)
	{
		g_debug("meowmenu: preset file '%s': empty Name", path.c_str());
		g_free(name_raw);
		g_key_file_free(kf);
		return false;
	}
	std::string display_name(name_raw);
	g_free(name_raw);

	// Derive id from filename stem (everything before the last dot).
	std::string id;
	{
		const gchar* base = g_path_get_basename(path.c_str());
		std::string bname(base ? base : "");
		g_free((gpointer)base);
		const size_t dot = bname.rfind('.');
		id = (dot != std::string::npos) ? bname.substr(0, dot) : bname;
	}

	// NOTE: If the optional Id key is present it MUST match the filename stem.
	if (g_key_file_has_key(kf, "Preset", "Id", nullptr))
	{
		gchar* file_id = g_key_file_get_string(kf, "Preset", "Id", nullptr);
		bool match = file_id && (id == file_id);
		g_free(file_id);
		if (!match)
		{
			g_debug("meowmenu: preset file '%s': Id key does not match filename stem '%s'",
				path.c_str(), id.c_str());
			g_key_file_free(kf);
			return false;
		}
	}

	gchar* desc_raw = g_key_file_get_string(kf, "Preset", "Description", nullptr);
	std::string description = desc_raw ? desc_raw : "";
	g_free(desc_raw);

	// Parse [Settings] — same per-key validation as import_user_preset.
	PresetValueMap values;
	gsize nkeys = 0;
	gchar** keys = g_key_file_get_keys(kf, "Settings", &nkeys, nullptr);
	if (keys)
	{
		for (gsize ki = 0; ki < nkeys; ++ki)
		{
			const gchar* key = keys[ki];
			const PropDef* pd = find_prop_def(key);
			if (!pd)
			{
				g_debug("meowmenu: preset file '%s': unknown key '%s', skipping",
					path.c_str(), key);
				continue;
			}
			if (pd->kind == PresetValue::Int)
			{
				GError* ke = nullptr;
				int v = g_key_file_get_integer(kf, "Settings", key, &ke);
				if (ke || v < pd->min_i || v > pd->max_i)
				{
					g_debug("meowmenu: preset file '%s': invalid int for '%s', skipping", path.c_str(), key);
					if (ke) g_error_free(ke);
					continue;
				}
				values[key] = PresetValue::from_int(v);
			}
			else if (pd->kind == PresetValue::Bool)
			{
				GError* ke = nullptr;
				gboolean v = g_key_file_get_boolean(kf, "Settings", key, &ke);
				if (ke)
				{
					g_debug("meowmenu: preset file '%s': invalid bool for '%s', skipping", path.c_str(), key);
					g_error_free(ke);
					continue;
				}
				values[key] = PresetValue::from_bool(v != FALSE);
			}
			else // Str
			{
				gchar* sv = g_key_file_get_string(kf, "Settings", key, nullptr);
				bool valid = false;
				for (int di = 0; di < pd->domain_len; ++di)
				{
					if (sv && strcmp(sv, pd->domain[di]) == 0)
					{
						valid = true;
						break;
					}
				}
				if (!valid)
				{
					g_debug("meowmenu: preset file '%s': invalid value '%s' for '%s', skipping",
						path.c_str(), sv ? sv : "(null)", key);
					g_free(sv);
					continue;
				}
				values[key] = PresetValue::from_str(sv);
				g_free(sv);
			}
		}
		g_strfreev(keys);
	}

	g_key_file_free(kf);

	out.id           = id;
	out.display_name = display_name;
	out.description  = description;
	out.is_builtin   = true;
	out.values       = std::move(values);
	return true;
}

/* enumerate_preset_files:
 * @system_dir: path to the installed read-only preset directory.
 * @user_dir:   path to the user-writable preset directory.
 *
 * Walks system_dir then user_dir, parsing every .meowpreset file. Entries
 * with the same id are deduplicated: user_dir wins (FR-061). Files that
 * fail validation are silently skipped — no error dialog, no crash (FR-063).
 *
 * Returns: list of successfully parsed LayoutPreset objects; may be empty.
 */
std::vector<LayoutPreset> WhiskerMenu::enumerate_preset_files(const std::string& system_dir,
	const std::string& user_dir)
{
	// keyed by id; second pass (user dir) overwrites first pass (system dir).
	std::map<std::string, LayoutPreset> by_id;

	auto scan_dir = [&](const std::string& dir_path)
	{
		GError* err = nullptr;
		GDir* d = g_dir_open(dir_path.c_str(), 0, &err);
		if (!d)
		{
			if (err)
			{
				g_debug("meowmenu: enumerate_preset_files: cannot open '%s': %s",
					dir_path.c_str(), err->message);
				g_error_free(err);
			}
			return;
		}

		const gchar* fname;
		while ((fname = g_dir_read_name(d)) != nullptr)
		{
			if (!g_str_has_suffix(fname, ".meowpreset"))
				continue;

			std::string full = dir_path + G_DIR_SEPARATOR_S + fname;
			LayoutPreset p;
			if (parse_preset_file_internal(full, p))
				by_id[p.id] = std::move(p);
		}
		g_dir_close(d);
	};

	scan_dir(system_dir);
	scan_dir(user_dir); // user wins on id collision

	std::vector<LayoutPreset> result;
	result.reserve(by_id.size());
	for (auto& pair : by_id)
		result.push_back(std::move(pair.second));
	return result;
}
