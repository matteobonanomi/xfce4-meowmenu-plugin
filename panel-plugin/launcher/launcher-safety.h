/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_LAUNCHER_SAFETY_H
#define WHISKERMENU_LAUNCHER_SAFETY_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

/* launcher_model_get_iter:
 * @model: current launcher model.
 * @path: event-provided tree path.
 * @iter: receives the resolved iterator.
 *
 * Returns: true only when all inputs are valid and @path still resolves in
 * @model. Event handlers must stop before reading row data when false.
 */
bool launcher_model_get_iter(GtkTreeModel* model, GtkTreePath* path,
		GtkTreeIter* iter);

/* launcher_editor_argv:
 * @editor: desktop-file editor executable name.
 * @uri: selected launcher URI.
 *
 * Builds an argument vector that preserves @uri as one exact argument, without
 * shell interpolation.
 *
 * Returns: newly allocated NULL-terminated argv, or NULL for invalid input.
 * Free with g_strfreev().
 */
gchar** launcher_editor_argv(const gchar* editor, const gchar* uri);

/* launcher_hide_relpath_for_uri:
 * @uri: launcher file URI.
 * @applications_dir: one XDG data applications directory.
 *
 * Returns: a newly allocated applications-relative save path when @uri is a
 * file inside @applications_dir, or NULL for unsupported/outside locations.
 */
gchar* launcher_hide_relpath_for_uri(const gchar* uri,
		const gchar* applications_dir);

}

#endif // WHISKERMENU_LAUNCHER_SAFETY_H
