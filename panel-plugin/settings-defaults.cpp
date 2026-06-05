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

	// Back-fill the marker on every path (fresh, upgrade, or already-current
	// schema) so the next load is unambiguously an upgrade and the user's
	// layout is never reset again. Written inside the active begin/end batch.
	if (!marker)
		initialized = true;

	end_property_update();
}

//-----------------------------------------------------------------------------
