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

#include "presets/preset.h"
#include "settings.h"

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* migrate_schema:
 * @marker:        value of the persisted /initialized key at load time.
 * @empty_channel: true when no plugin Xfconf properties were present at load.
 *
 * Decides fresh-vs-upgrade from the marker (authoritative), not the raw
 * property count, then walks the channel forward through every known schema
 * version applying additive migrations. Versions are cumulative: each block
 * runs once per upgrade. The defaults tables here are the single source of
 * truth for Xfconf key defaults seeded on schema upgrade; they MUST stay
 * aligned with the inline defaults supplied in the Settings constructor.
 *
 * Decision (contracts/fresh-vs-upgrade-decision.md):
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
	const bool is_fresh_install = !marker && empty_channel;

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
		struct { const char* prop; int val; } int_props[] = {
			{ "/corner-radius",       0   },
			{ "/panel-gap",           0   },
			{ "/categories-opacity",  100 },
			{ "/apps-opacity",        100 },
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
			{ "/profile-position",     "top-left"  },
			{ "/commands-position",    "top-right" },
			{ "/grid-density",         "medium"    },
			{ "/layout-mode",          "docked"    },
		};
		for (auto& p : str_props)
		{
			if (!xfconf_channel_has_property(channel, p.prop))
				xfconf_channel_set_string(channel, p.prop, p.val);
		}

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
		// NOTE: /places/show-metadata had no consumer and is removed; it is
		// intentionally not seeded here, and the schema-v6 block deletes any
		// pre-existing value.
		struct { const char* prop; gboolean val; } bool_props[] = {
			{ "/places/enabled",            FALSE },
			{ "/places/history-enabled",    TRUE  },
			{ "/places/favourites-enabled", TRUE  },
			{ "/places/remember-last-mode", FALSE },
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

	if (schema_version < 4)
	{
		// Feature 020 — "Enable sidebar" switch replaces the legacy "hidden"
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
		if (!xfconf_channel_has_property(channel, "/sidebar-enabled"))
			xfconf_channel_set_bool(channel, "/sidebar-enabled", TRUE);
		if (!xfconf_channel_has_property(channel, "/places/switch-show-icons"))
		{
			const LayoutPreset* preset = find_preset_by_id(
					std::string(static_cast<const char*>(current_preset_id)));
			gboolean show_icons = FALSE;
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
		// it from the active preset (FR-012): the value the preset pins, else
		// fully opaque (100) when no preset governs opacity. The retired keys no
		// longer drive rendering, so reset them — the channel then carries only
		// /menu-opacity. Resetting an absent key is a no-op, so this block is
		// idempotent and runs once (guarded by < 7), never clobbering a later
		// user customisation of /menu-opacity.
		int derived = 100;
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
			profile_position = rewritten;
		}
		g_free(profile_pos);

		schema_version = 8;
	}

	if (schema_version < 9)
	{
		// NOTE: /transparent-grid defaults to false so existing installs keep
		// their solid resting grid tiles until the user opts into transparency.
		if (!xfconf_channel_has_property(channel, "/transparent-grid"))
			xfconf_channel_set_bool(channel, "/transparent-grid", FALSE);

		schema_version = 9;
	}

	if (schema_version < 10)
	{
		// NOTE: default to the active GTK theme's button shape. The explicit
		// rounded shape remains available for users who prefer the older pill.
		if (!xfconf_channel_has_property(channel, "/places/switch-button-shape"))
			xfconf_channel_set_string(channel, "/places/switch-button-shape",
					PLACES_SWITCH_SHAPE_GTK_THEME);

		schema_version = 10;
	}

	if (schema_version < 11)
	{
		const char* id = current_preset_id;
		const bool known_nonclassic = g_strcmp0(id, "modern") == 0
			|| g_strcmp0(id, "fullscreen") == 0 || g_strcmp0(id, "minimal") == 0;
		if (!xfconf_channel_has_property(channel, "/extras/calculator-engine"))
			xfconf_channel_set_string(channel, "/extras/calculator-engine",
					known_nonclassic ? "bc" : "none");
		if (!xfconf_channel_has_property(channel, "/extras/calculator-result-font-size"))
			xfconf_channel_set_int(channel, "/extras/calculator-result-font-size", -1);
		if (!xfconf_channel_has_property(channel, "/extras/calculator-max-decimal-places"))
			xfconf_channel_set_int(channel, "/extras/calculator-max-decimal-places", 4);
		schema_version = 11;
	}

	// Back-fill the marker on every path (fresh, upgrade, or already-current
	// schema) so the next load is unambiguously an upgrade and the user's
	// layout is never reset again. Written inside the active begin/end batch.
	if (!marker)
		initialized = true;

	end_property_update();
}

//-----------------------------------------------------------------------------
