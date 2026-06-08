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

#ifndef WHISKERMENU_PRESET_H
#define WHISKERMENU_PRESET_H

#include <map>
#include <string>
#include <vector>

#include <xfconf/xfconf.h>

namespace WhiskerMenu
{

class Settings;

// A single setting value: bool, int, or string.
struct PresetValue
{
	enum Kind { Bool, Int, Str } kind;
	bool  b;
	int   i;
	std::string s;

	static PresetValue from_bool(bool v)   { PresetValue p; p.kind = Bool; p.b = v; return p; }
	static PresetValue from_int(int v)     { PresetValue p; p.kind = Int;  p.i = v; return p; }
	static PresetValue from_str(const char* v) { PresetValue p; p.kind = Str; p.s = v; return p; }
};

typedef std::map<std::string, PresetValue> PresetValueMap;

struct LayoutPreset
{
	std::string   id;
	std::string   display_name;
	// Stored identity name surfaced as the active-preset label. For built-ins
	// it equals the localized display name (from the .meowpreset Name= key or
	// the C++ table); for custom presets it is the user's chosen name.
	std::string   name;
	std::string   description;
	bool          is_builtin;
	PresetValueMap values;
};

// Indices into BUILTIN_PRESETS
enum BuiltinPresetIndex
{
	PRESET_CLASSIC   = 0,
	PRESET_MODERN    = 1,
	PRESET_FULLSCREEN = 2,
	PRESET_BUILTIN_COUNT = 3,
};

extern const LayoutPreset BUILTIN_PRESETS[PRESET_BUILTIN_COUNT];

// Authoritative, complete set of settings a built-in preset fully determines.
// This is the single source of truth for "what a preset governs": every
// built-in (both its .meowpreset seed and the C++ fallback table) MUST define
// a value for every key returned here, and a unit test enforces it.
const std::vector<std::string>& governed_keys();

// The set of governed keys the Properties dialog re-syncs onto its widgets when
// a preset is applied (sync_preset_widgets). Kept as a display-free static list
// so a unit test can assert it equals governed_keys() — i.e. no governed key is
// left unsynced — without instantiating a GTK display.
const std::vector<std::string>& synced_keys();

// Apply all values from preset to settings; set current_preset_id at the end.
void apply_preset(const LayoutPreset& preset, Settings& settings);

// Find a preset by id (file presets first, then C++ fallback table, then user presets).
// Returns nullptr if not found.
const LayoutPreset* find_preset_by_id(const std::string& id);

// Load or reload built-in presets from on-disk .meowpreset files.
// Falls back per-id to BUILTIN_PRESETS[] when a file is absent or malformed.
// Should be called once at startup, e.g. from SettingsDialog constructor.
void initialize_file_presets();

// Return the in-memory cache of file-seeded built-in presets.
// Empty before initialize_file_presets() is called.
const std::vector<LayoutPreset>& get_file_presets();

// True if any setting governed by preset differs from the live settings values.
bool compute_preset_diff(const LayoutPreset& preset, const Settings& settings);

// Rebuild the in-memory user-preset cache from Xfconf.
// Returns a reference to the cache; callers must not store across mutations.
const std::vector<LayoutPreset>& enumerate_user_presets(XfconfChannel* channel);

// Save all current governed settings as a new user preset named display_name.
// Returns the new uuid on success, empty string if name conflicts or is empty.
std::string save_current_as_user_preset(const std::string& display_name, Settings& settings);

// Rename an existing user preset. Returns false if uuid not found or name conflicts.
bool rename_user_preset(const std::string& uuid, const std::string& new_name, Settings& settings);

// Delete a user preset. Clears current_preset_id if it matches uuid.
void delete_user_preset(const std::string& uuid, Settings& settings);

// Reset every plugin property on channel to its compiled default, EXCEPT saved
// user presets under /presets/<uuid>/, which are preserved.
//
// @channel: the plugin's Xfconf channel; may be anchored on a property base.
// @property_base: that channel's property base (e.g. "/plugins/meowmenu-N"),
//   or "" for a base-less channel.
//
// xfconf_channel_get_properties() returns FULL paths that include the channel
// base, but xfconf_channel_reset_property() on a based channel expects a
// base-relative path; this helper strips the base so the reset is not
// double-prefixed (which silently no-ops). Settings-free so it is unit-testable.
//
// Returns: the number of properties reset.
int reset_settings_to_defaults(XfconfChannel* channel, const std::string& property_base);

} // namespace WhiskerMenu

#endif // WHISKERMENU_PRESET_H
