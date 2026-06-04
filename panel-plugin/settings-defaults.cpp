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

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

/* migrate_schema:
 * @is_fresh_install: true when no Xfconf properties were present at load.
 *
 * Walks the channel forward through every known schema version, applying
 * additive migrations. Versions are cumulative: each block runs once per
 * upgrade. The defaults tables here are the single source of truth for
 * Xfconf key defaults seeded on schema upgrade; they MUST stay aligned
 * with the inline defaults supplied in the Settings constructor.
 */
void Settings::migrate_schema(bool is_fresh_install)
{
	if (!channel)
		return;

	const int current_schema = 4;
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

	end_property_update();
}

//-----------------------------------------------------------------------------
