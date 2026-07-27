/*
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
 */

#include "migration.h"

#include <string>

namespace WhiskerMenu
{

// HACK: heuristic for "is upstream Whisker installed". Per research.md §R1
// distros disagree on which of these two paths Xfce uses for panel-plugin
// .desktop files; trying both covers Ubuntu/Debian/Fedora/openSUSE without
// spawning a subprocess. If neither file exists we fall back to "absent"
// rather than guessing, so a missing Whisker package is detected
// deterministically.
static const char* const k_whisker_desktop_paths[] = {
	"/usr/share/xfce4/panel/plugins/whiskermenu.desktop",
	"/usr/share/xfce4/panel-plugins/whiskermenu.desktop",
	nullptr,
};

bool whisker_present()
{
	// NOTE: the env-var override is exposed for the migration_test
	// fixture which cannot easily inject a Whisker .desktop into the
	// real system paths. It is intentionally honored unconditionally
	// (not gated on a debug build) so that the test surface is the
	// same code path that production runs.
	const gchar* env = g_getenv("MEOWMENU_TEST_WHISKER_PRESENT");
	if (env != nullptr && env[0] != '\0')
	{
		return env[0] == '1';
	}

	for (const char* const* p = k_whisker_desktop_paths; *p != nullptr; ++p)
	{
		if (g_file_test(*p, G_FILE_TEST_EXISTS))
		{
			return true;
		}
	}
	return false;
}

/* compute_legacy_base:
 * @current_base: per-instance base for this MeowMenu plugin instance,
 *   e.g. "/plugins/meowmenu-7".
 *
 * Returns the corresponding pre-rename per-instance base, e.g.
 * "/plugins/whiskermenu-7", or an empty string if the input is not in
 * the expected "<prefix>/meowmenu-<id>" form. The same numeric id is
 * preserved so multi-instance setups migrate 1:1.
 */
static std::string compute_legacy_base(const gchar* current_base)
{
	std::string s(current_base != nullptr ? current_base : "");
	const std::string from = "/meowmenu-";
	const std::string to = "/whiskermenu-";
	const std::string::size_type pos = s.find(from);
	if (pos == std::string::npos)
	{
		return std::string();
	}
	return s.substr(0, pos) + to + s.substr(pos + from.size());
}

bool migrate_legacy_xfconf(XfconfChannel* panel_channel, const gchar* current_base)
{
	if (panel_channel == nullptr || current_base == nullptr || *current_base == '\0')
	{
		return false;
	}

	g_debug("meowmenu: migration check at %s", current_base);

	// Sentinel path lives under the new MeowMenu base, never under the
	// legacy/Whisker base, so the legacy namespace stays bit-identical
	// after migration (the documented behavior).
	const std::string sentinel = std::string(current_base) + "/migration/legacy-imported";

	if (xfconf_channel_has_property(panel_channel, sentinel.c_str()))
	{
		return false;
	}

	if (whisker_present())
	{
		g_message("meowmenu: Whisker present, skipping legacy Xfconf migration at %s",
				current_base);
		return false;
	}

	const std::string legacy_base = compute_legacy_base(current_base);
	if (legacy_base.empty())
	{
		// Non-standard base (e.g. tests with custom paths): still write
		// the sentinel so we don't re-enter on the next launch.
		// NOTE: the sentinel write is itself an xfconf set_*() and is
		// atomic at the property level — if the process crashes here,
		// the next launch re-enters and writes it then (the documented behavior).
		if (!xfconf_channel_set_bool(panel_channel, sentinel.c_str(), TRUE))
		{
			g_warning("meowmenu: failed to write migration sentinel %s", sentinel.c_str());
		}
		return true;
	}

	GHashTable* legacy = xfconf_channel_get_properties(panel_channel, legacy_base.c_str());
	gsize copied = 0;
	if (legacy != nullptr)
	{
		GHashTableIter iter;
		gpointer key = nullptr;
		gpointer value = nullptr;
		g_hash_table_iter_init(&iter, legacy);
		while (g_hash_table_iter_next(&iter, &key, &value))
		{
			const gchar* legacy_path = static_cast<const gchar*>(key);
			const GValue* gval = static_cast<const GValue*>(value);
			if (legacy_path == nullptr || gval == nullptr)
			{
				continue;
			}

			// Skip the bare base node itself (xfconf returns the root path
			// when it carries a value; subpaths are what we copy).
			const std::string lp(legacy_path);
			if (lp.size() < legacy_base.size()
					|| lp.compare(0, legacy_base.size(), legacy_base) != 0)
			{
				continue;
			}

			const std::string sub = lp.substr(legacy_base.size());
			const std::string new_path = std::string(current_base) + sub;

			g_debug("meowmenu: copying %s", legacy_path);

			if (!xfconf_channel_set_property(panel_channel, new_path.c_str(), gval))
			{
				const gchar* type_name = G_VALUE_TYPE_NAME(gval);
				g_warning("meowmenu: failed to migrate property %s (type %s)",
						legacy_path,
						type_name != nullptr ? type_name : "(null)");
				continue; // the documented behavior: don't abort, just log.
			}
			++copied;
		}
		g_hash_table_destroy(legacy);
	}

	// Sentinel write happens after the copy loop completes, so a crash
	// mid-loop leaves the sentinel unset and the next launch re-runs the
	// copy. set_*() is last-writer-wins on identical values, so the re-run
	// is idempotent (the documented behavior, the documented behavior).
	if (!xfconf_channel_set_bool(panel_channel, sentinel.c_str(), TRUE))
	{
		g_warning("meowmenu: failed to write migration sentinel %s", sentinel.c_str());
	}
	g_message("meowmenu: migrated %" G_GSIZE_FORMAT " legacy properties to %s",
			copied, current_base);
	return true;
}

} // namespace WhiskerMenu
