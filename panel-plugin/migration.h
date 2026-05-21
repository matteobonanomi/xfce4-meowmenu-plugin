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

#ifndef MEOWMENU_MIGRATION_H
#define MEOWMENU_MIGRATION_H

#include <glib.h>
#include <xfconf/xfconf.h>

namespace WhiskerMenu
{

/* migrate_legacy_xfconf:
 * @panel_channel: the Xfconf channel for the panel (typically
 *   xfconf_channel_new(xfce_panel_get_channel_name())). Must be non-null
 *   and already initialised by the caller.
 * @current_base: the per-instance property base for THIS MeowMenu plugin
 *   instance (e.g. "/plugins/meowmenu-7"), as returned by
 *   xfce_panel_plugin_get_property_base(). Must be non-empty.
 *
 * Performs a one-shot copy of every Xfconf property under the
 * corresponding legacy base "/plugins/whiskermenu-<id>" into
 * @current_base, if and only if (a) the migration sentinel at
 * "<current_base>/migration/legacy-imported" is not already set, and
 * (b) the upstream Whisker plugin is not currently present on the
 * system (see whisker_present()).
 *
 * Properties are copied with last-writer-wins set semantics; the source
 * namespace is never written or deleted (FR-012). Individual set
 * failures are logged via g_warning() and do not abort the loop
 * (FR-014). The sentinel is written only after the copy loop completes,
 * making the migration resumable (FR-011).
 *
 * Returns: true if migration ran to completion (including the trivial
 * "no legacy data" case, which still writes the sentinel) and the
 * sentinel was written; false if migration was skipped because Whisker
 * is present or the sentinel was already set.
 */
bool migrate_legacy_xfconf(XfconfChannel* panel_channel,
                           const gchar* current_base);

/* whisker_present:
 *
 * Returns: true if the upstream Whisker panel-plugin appears to be
 * installed on this system, per the heuristic documented in
 * research.md §R1: the panel-plugin .desktop file exists at one of the
 * two well-known Xfce paths.
 *
 * For test use only: setting the env var MEOWMENU_TEST_WHISKER_PRESENT
 * to "1" or "0" overrides the filesystem check. The override is honored
 * in every build and is never set in production.
 */
bool whisker_present();

} // namespace WhiskerMenu

#endif // MEOWMENU_MIGRATION_H
