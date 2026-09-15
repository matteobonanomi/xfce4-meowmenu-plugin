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

#include "settings-defaults.h"

#include "core/sidebar-layout.h"
#include "presets/preset.h"
#include "settings.h"

#include <glib/gi18n-lib.h>

#include <string>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

const IntegerSettingDefault WhiskerMenu::DEFAULT_CORNER_RADIUS = {
	"/corner-radius", 0, 0, 24, false
};
const IntegerSettingDefault WhiskerMenu::DEFAULT_PANEL_GAP = {
	"/panel-gap", 0, 0, 50, false
};
const StringSettingDefault WhiskerMenu::DEFAULT_SIDEBAR_POSITION = {
	"/sidebar-position", "left"
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_SIDEBAR_ENABLED = {
	"/sidebar-enabled", true
};
const StringSettingDefault WhiskerMenu::DEFAULT_SEARCH_BAR_POSITION = {
	"/search-bar-position", "top"
};
const StringSettingDefault WhiskerMenu::DEFAULT_GRID_DENSITY = {
	"/grid-density", "medium"
};
const StringSettingDefault WhiskerMenu::DEFAULT_LAYOUT_MODE = {
	"/layout-mode", "docked"
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_PLACES_ENABLED = {
	"/places/enabled", false
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_PLACES_HISTORY_ENABLED = {
	"/places/history-enabled", true
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_PLACES_FAVOURITES_ENABLED = {
	"/places/favourites-enabled", true
};
const StringSettingDefault WhiskerMenu::DEFAULT_PLACES_FAVOURITE_SYNC = {
	"/places/favourite-sync", "meowmenu"
};
const IntegerSettingDefault WhiskerMenu::DEFAULT_PLACES_MAX_ITEMS = {
	"/places/max-items", 20, 0, 30, false
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_PLACES_REMEMBER_LAST_MODE = {
	"/places/remember-last-mode", false
};
const StringSettingDefault WhiskerMenu::DEFAULT_PLACES_LAST_MODE = {
	"/places/last-mode", "apps"
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_TRANSPARENT_GRID = {
	"/transparent-grid", false
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_SHOW_PROFILE = {
	"/show-profile", SETTINGS_SHOW_PROFILE_DEFAULT
};
const BooleanSettingDefault WhiskerMenu::DEFAULT_SHOW_SESSION = {
	"/show-session", SETTINGS_SHOW_SESSION_DEFAULT
};
const IntegerSettingDefault WhiskerMenu::DEFAULT_CALCULATOR_RESULT_FONT_SIZE = {
	"/extras/calculator-result-font-size", -1, -1, 6, true
};
const IntegerSettingDefault WhiskerMenu::DEFAULT_CALCULATOR_MAX_DECIMAL_PLACES = {
	"/extras/calculator-max-decimal-places", 4, 0, 10, true
};

namespace
{

void seed_if_missing(XfconfChannel* channel,
		const BooleanSettingDefault& descriptor)
{
	if (!xfconf_channel_has_property(channel, descriptor.property))
		xfconf_channel_set_bool(channel, descriptor.property, descriptor.value);
}

void seed_if_missing(XfconfChannel* channel,
		const IntegerSettingDefault& descriptor)
{
	if (!xfconf_channel_has_property(channel, descriptor.property))
		xfconf_channel_set_int(channel, descriptor.property, descriptor.value);
}

void seed_if_missing(XfconfChannel* channel,
		const StringSettingDefault& descriptor)
{
	if (!xfconf_channel_has_property(channel, descriptor.property))
		xfconf_channel_set_string(channel, descriptor.property, descriptor.value);
}

}

//-----------------------------------------------------------------------------

const char* WhiskerMenu::migrate_layout_schema_v13(XfconfChannel* channel)
{
	if (!channel)
		return nullptr;

	for (const char* key : RETIRED_SETTINGS_KEYS)
		xfconf_channel_reset_property(channel, key, FALSE);

	const char* canonical = nullptr;
	gchar* sidebar = xfconf_channel_get_string(channel,
			"/sidebar-position", nullptr);
	if (g_strcmp0(sidebar, "top") == 0
			|| g_strcmp0(sidebar, "bottom") == 0)
		canonical = "horizontal";
	else if (!meow_sidebar_position_key_is_supported(sidebar))
		canonical = "left";

	if (canonical)
		xfconf_channel_set_string(channel, "/sidebar-position", canonical);
	g_free(sidebar);
	return canonical;
}

//-----------------------------------------------------------------------------

/* migrate_schema:
 * @marker:        value of the persisted /initialized key at load time.
 * @empty_channel: true when no plugin Xfconf properties were present at load.
 *
 * Decides fresh-vs-upgrade from the marker (authoritative), not the raw
 * property count, then walks the channel forward through every known schema
 * version applying additive migrations. Versions are cumulative: each block
 * runs once per upgrade. Ordinary construction and migration seeds consume
 * the typed descriptors above; preset-derived and historical seeds remain
 * explicit at the migration step that owns their distinct behavior.
 *
 * Decision (the documented interface):
 *   - marker absent AND empty channel  ⇒ FRESH: apply the Modern preset.
 *   - marker present                   ⇒ UPGRADE: preserve the user's layout.
 *   - marker absent with stored config ⇒ UPGRADE (existing user's first
 *                                        marker-aware run): preserve layout,
 *                                        derive identity, back-fill the marker.
 *   - present but unmigratable config  ⇒ degrades to a safe Modern-equivalent
 *                                        state (xfconf getters fall back to
 *                                        defaults); never crashes, never
 *                                        forces Classic.
 *
 * INVARIANT: a `true` marker NEVER causes a layout reset. The marker is
 * back-filled on every path so a still-running xfconfd serving stale state can
 * never trigger a later reset.
 */
void Settings::migrate_schema(bool marker, bool empty_channel)
{
	if (!channel)
		return;

	// Marker is authoritative: only a never-initialized, genuinely empty
	// channel is a fresh install. Everything else is an upgrade.
	const bool is_fresh_install = should_apply_fresh_preset(marker, empty_channel);

	begin_property_update();

	// Fresh installs land on Modern (applied once, up front, so it runs
	// regardless of the stored schema version). Upgrades intentionally skip
	// this and keep the user's stored layout untouched.
	if (is_fresh_install)
		apply_preset(BUILTIN_PRESETS[PRESET_MODERN], *this);

	if (schema_version < 1)
	{
		// Map legacy menu-opacity → categories-opacity if present and categories-opacity missing.
		// NOTE: this historical v1 step is left intact; /categories-opacity is now
		// a retired key with no Settings member, so only the channel value is
		// seeded here — the v7 block below resets it and derives the single
		// /menu-opacity from the active preset.
		if (xfconf_channel_has_property(channel, "/menu-opacity")
				&& !xfconf_channel_has_property(channel, "/categories-opacity"))
		{
			const int legacy_opacity = xfconf_channel_get_int(channel, "/menu-opacity", 100);
			xfconf_channel_set_int(channel, "/categories-opacity", legacy_opacity);
		}

		// Write defaults for V1 properties not yet in the channel
		// NOTE: /grid-columns and /grid-rows were orphaned config (no control, no
		// consumer) and are removed; they are intentionally not seeded here, and
		// the schema-v6 block deletes any pre-existing values.
		seed_if_missing(channel, DEFAULT_CORNER_RADIUS);
		seed_if_missing(channel, DEFAULT_PANEL_GAP);
		seed_if_missing(channel, DEFAULT_SIDEBAR_POSITION);
		seed_if_missing(channel, DEFAULT_SEARCH_BAR_POSITION);
		seed_if_missing(channel, DEFAULT_GRID_DENSITY);
		seed_if_missing(channel, DEFAULT_LAYOUT_MODE);

		if (!xfconf_channel_has_property(channel, "/categories-opacity"))
			xfconf_channel_set_int(channel, "/categories-opacity",
					HISTORICAL_CATEGORIES_OPACITY_SEED);
		if (!xfconf_channel_has_property(channel, "/apps-opacity"))
			xfconf_channel_set_int(channel, "/apps-opacity",
					HISTORICAL_APPS_OPACITY_SEED);

		// NOTE: the fresh-install Modern preset is applied up front (see the
		// top of this function), not here. Upgrades intentionally leave
		// current_preset_id unset in this block — the v5 step derives the
		// active-preset identity from the user's actual layout instead of
		// hard-defaulting to "classic" (which would mislabel a non-classic
		// layout). The user's stored layout values are never touched on upgrade.

		schema_version = 1;
	}

	if (schema_version < 2)
	{
		// Seed /full-screen-opacity (new key). Default 100; honour active preset if it pins one.
		// NOTE: at this point preset.cpp's file-seeded values are not yet read — the active
		// preset's compiled default is used as the fallback. Users who want a custom value
		// can edit it via the Properties dialog after upgrade.
		if (!xfconf_channel_has_property(channel, "/full-screen-opacity"))
			xfconf_channel_set_int(channel, "/full-screen-opacity",
					HISTORICAL_FULL_SCREEN_OPACITY_SEED);

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
		}

		// The former Hidden avatar shape now maps to the explicit visibility key.
		if (xfconf_channel_has_property(channel, "/profile-shape")
				&& xfconf_channel_get_int(channel, "/profile-shape", ProfileRound) == 2)
		{
			xfconf_channel_set_bool(channel, "/show-profile", FALSE);
			show_profile = false;
			xfconf_channel_set_int(channel, "/profile-shape", ProfileRound);
			profile_shape = ProfileRound;
		}

		schema_version = 2;
	}

	if (schema_version < 3)
	{
		// Current behavior — Places mode keys. Seed defaults on first upgrade so
		// existing installs see consistent values without re-applying a preset.
		// NOTE: /places/show-metadata had no consumer and is removed; it is
		// intentionally not seeded here, and the schema-v6 block deletes any
		// pre-existing value.
		seed_if_missing(channel, DEFAULT_PLACES_ENABLED);
		seed_if_missing(channel, DEFAULT_PLACES_HISTORY_ENABLED);
		seed_if_missing(channel, DEFAULT_PLACES_FAVOURITES_ENABLED);
		seed_if_missing(channel, DEFAULT_PLACES_REMEMBER_LAST_MODE);
		seed_if_missing(channel, DEFAULT_PLACES_FAVOURITE_SYNC);
		seed_if_missing(channel, DEFAULT_PLACES_LAST_MODE);
		seed_if_missing(channel, DEFAULT_PLACES_MAX_ITEMS);

		schema_version = 3;
	}

	if (schema_version < 4)
	{
		// Sidebar behavior — "Enable sidebar" switch replaces the legacy "hidden"
		// sidebar position. Map a stored hidden sidebar to the switch being OFF
		// and restore a valid Position so the dropdown never shows "hidden".
		gchar* current_sidebar = xfconf_channel_get_string(channel, "/sidebar-position", nullptr);
		if (g_strcmp0(current_sidebar, "hidden") == 0)
		{
			xfconf_channel_set_bool(channel, "/sidebar-enabled", FALSE);
			sidebar_enabled = false;
			xfconf_channel_set_string(channel, "/sidebar-position", "left");
			sidebar_position = "left";
		}
		g_free(current_sidebar);

		// Seed the new keys when absent. switch-show-icons follows the active
		// preset's default if a preset is set; otherwise the classic OFF.
		seed_if_missing(channel, DEFAULT_SIDEBAR_ENABLED);
		if (!xfconf_channel_has_property(channel, "/places/switch-show-icons"))
		{
			const LayoutPreset* preset = find_preset_by_id(
					std::string(static_cast<const char*>(current_preset_id)));
			gboolean show_icons = PRESET_SWITCH_SHOW_ICONS_SEED_FALLBACK;
			if (preset)
			{
				auto it = preset->values.find("places-show-icons");
				if (it != preset->values.end() && it->second.kind == PresetValue::Bool)
					show_icons = it->second.b ? TRUE : FALSE;
			}
			xfconf_channel_set_bool(channel, "/places/switch-show-icons", show_icons);
			places_switch_show_icons = show_icons;
		}

		schema_version = 4;
	}

	if (schema_version < 5)
	{
		// Feature 021 — every preset carries a stored identity name surfaced as
		// the active-preset label, and the upgrade path no longer hard-defaults
		// to "classic". This step NEVER writes layout values; it only resolves
		// the active-preset identity and seeds per-preset name metadata.

		// 1. Seed /presets/<uuid>/name for any existing custom preset lacking
		//    it, defaulting to the preset's display-name.
		const auto& user_presets = enumerate_user_presets(channel);
		for (const auto& p : user_presets)
		{
			std::string name_key = "/presets/" + p.id + "/name";
			if (!xfconf_channel_has_property(channel, name_key.c_str()))
				xfconf_channel_set_string(channel, name_key.c_str(), p.display_name.c_str());
		}

		// 2. Derive the active-preset identity when none is stored (the case
		//    the old hard "classic" default used to clobber). Match the live
		//    layout against the built-ins; on an exact match adopt that
		//    built-in, otherwise record a distinct custom preset named "Custom"
		//    capturing the running layout so the label reflects reality.
		const gchar* stored_id = static_cast<const gchar*>(current_preset_id);
		if (!stored_id || !*stored_id)
		{
			const LayoutPreset* match = nullptr;
			for (int i = 0; i < PRESET_BUILTIN_COUNT; ++i)
			{
				if (!compute_preset_diff(BUILTIN_PRESETS[i], *this))
				{
					match = &BUILTIN_PRESETS[i];
					break;
				}
			}
			if (match)
			{
				// Adopt the built-in identity only — no layout values written.
				current_preset_id = match->id;
			}
			else
			{
				// NOTE: save_current_as_user_preset writes the snapshot under
				// /presets/<uuid>/ (not the live keys) and sets
				// current_preset_id to the new uuid, giving a truthful label
				// the user can later rename or save over.
				save_current_as_user_preset(_("Custom"), *this);
			}
		}

		schema_version = 5;
	}

	if (schema_version < 6)
	{
		// Audit cleanup: drop orphaned/removed preference keys and normalise the
		// redundant profile-position value. Resetting an absent key is a no-op,
		// so this block is idempotent and touches only the named keys.
		const char* removed_keys[] = {
			"/grid-auto-size",
			"/grid-columns",
			"/grid-rows",
			"/places/show-metadata",
		};
		for (const char* key : removed_keys)
			xfconf_channel_reset_property(channel, key, FALSE);

		// "bottom-right" rendered identically to "bottom" for the profile and is
		// no longer offered; normalise any stored value so the combo never sees
		// an option it cannot display.
		gchar* profile_pos = xfconf_channel_get_string(channel, "/profile-position", nullptr);
		if (g_strcmp0(profile_pos, "bottom-right") == 0)
			xfconf_channel_set_string(channel, "/profile-position", "bottom");
		g_free(profile_pos);

		schema_version = 6;
	}

	if (schema_version < 7)
	{
		// Collapse the three per-region opacities to one menu-wide value. Derive
		// it from the active preset (supported behavior): the value the preset pins, else
		// fully opaque (100) when no preset governs opacity. The retired keys no
		// longer drive rendering, so reset them — the channel then carries only
		// /menu-opacity. Resetting an absent key is a no-op, so this block is
		// idempotent and runs once (guarded by < 7), never clobbering a later
		// user customisation of /menu-opacity.
		int derived = PRESET_MENU_OPACITY_SEED_FALLBACK;
		const gchar* preset_id = static_cast<const gchar*>(current_preset_id);
		if (preset_id && *preset_id)
		{
			const LayoutPreset* p = find_preset_by_id(preset_id);
			if (p)
			{
				auto it = p->values.find("menu-opacity");
				if (it != p->values.end() && it->second.kind == PresetValue::Int)
					derived = it->second.i;
			}
		}
		xfconf_channel_set_int(channel, "/menu-opacity", derived);
		menu_opacity = derived;

		for (const char* k : { "/categories-opacity", "/apps-opacity",
		                       "/full-screen-opacity" })
			xfconf_channel_reset_property(channel, k, FALSE);

		schema_version = 7;
	}

	if (schema_version < 8)
	{
		// Canonicalize the Profile row vocabulary without changing the key path.
		// Visible legacy aliases remain accepted on input elsewhere, but the
		// persisted value is rewritten once here so reopened Properties, presets,
		// and later exports all speak the same explicit left-anchored domain.
		gchar* profile_pos = xfconf_channel_get_string(channel, "/profile-position", nullptr);
		const char* rewritten = nullptr;
		if (g_strcmp0(profile_pos, "top") == 0)
			rewritten = "top-left";
		else if ((g_strcmp0(profile_pos, "bottom") == 0)
				|| (g_strcmp0(profile_pos, "bottom-right") == 0))
			rewritten = "bottom-left";

		if (rewritten)
		{
			xfconf_channel_set_string(channel, "/profile-position", rewritten);
		}
		g_free(profile_pos);

		schema_version = 8;
	}

	if (schema_version < 9)
	{
		// NOTE: /transparent-grid defaults to false so existing installs keep
		// their solid resting grid tiles until the user opts into transparency.
		seed_if_missing(channel, DEFAULT_TRANSPARENT_GRID);

		schema_version = 9;
	}

	if (schema_version < 10)
	{
		// Preserve the historical version step without seeding its retired key.
		schema_version = 10;
	}

	if (schema_version < 11)
	{
		const char* id = current_preset_id;
		const bool known_nonclassic = g_strcmp0(id, "modern") == 0
			|| g_strcmp0(id, "fullscreen") == 0 || g_strcmp0(id, "minimal") == 0;
		if (!xfconf_channel_has_property(channel, "/extras/calculator-engine"))
			xfconf_channel_set_string(channel, "/extras/calculator-engine",
					known_nonclassic ? CALCULATOR_ENGINE_NONCLASSIC_SEED
							: CALCULATOR_ENGINE_RUNTIME_DEFAULT);
		seed_if_missing(channel, DEFAULT_CALCULATOR_RESULT_FONT_SIZE);
		seed_if_missing(channel, DEFAULT_CALCULATOR_MAX_DECIMAL_PLACES);
		schema_version = 11;
	}

	if (schema_version < 12)
	{
		// NOTE: both visibility keys default on so adding intent storage does not
		// change the current renderer before it starts consuming these values.
		seed_if_missing(channel, DEFAULT_SHOW_PROFILE);
		seed_if_missing(channel, DEFAULT_SHOW_SESSION);
		schema_version = 12;
	}

	if (settings_schema_needs_upgrade(schema_version))
	{
		const char* canonical = migrate_layout_schema_v13(channel);
		if (canonical)
			sidebar_position = canonical;
		schema_version = SETTINGS_SCHEMA_VERSION;
	}

	// Back-fill the marker on every path (fresh, upgrade, or already-current
	// schema) so the next load is unambiguously an upgrade and the user's
	// layout is never reset again. Written inside the active begin/end batch.
	if (!marker)
		initialized = true;

	end_property_update();
}

//-----------------------------------------------------------------------------
