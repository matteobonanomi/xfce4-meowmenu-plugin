/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "desktop-drag.h"

#include "layout-mode.h"
#include "ui/icon-size.h"

extern "C"
{
#include <libxfce4util/libxfce4util.h>
}

#include <glib/gstdio.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

bool WhiskerMenu::desktop_drag_external_uri_enabled(const char* layout_mode)
{
	return layout_mode_from_key(layout_mode) != LayoutMode::FullScreen;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::desktop_drag_application_uri_available(
		const char* layout_mode, const char* uri)
{
	return desktop_drag_external_uri_enabled(layout_mode)
			&& !xfce_str_is_empty(uri);
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::desktop_drag_places_uri_available(const char* layout_mode,
		bool item_exists, const char* uri)
{
	return item_exists
			&& desktop_drag_external_uri_enabled(layout_mode)
			&& !xfce_str_is_empty(uri);
}

//-----------------------------------------------------------------------------

static bool desktop_drag_is_managed_folder_launcher_path(const char* path)
{
	if (xfce_str_is_empty(path))
	{
		return false;
	}

	gchar* parent = g_path_get_dirname(path);
	gchar* basename = g_path_get_basename(parent);
	gchar* grandparent = g_path_get_dirname(parent);
	const bool managed = g_str_has_prefix(basename,
			"meowmenu-folder-drag-")
			&& g_strcmp0(grandparent, g_get_tmp_dir()) == 0;
	g_free(grandparent);
	g_free(basename);
	g_free(parent);
	return managed;
}

//-----------------------------------------------------------------------------

/* desktop_drag_create_folder_launcher_uri:
 * @folder: existing folder exported by a Places desktop drag.
 * @display_name: label to use for the transient launcher; NULL falls back to
 *                the folder basename.
 *
 * Creates one temporary freedesktop Link launcher that points at @folder. The
 * helper never enumerates or copies folder contents; it writes only metadata
 * for the selected folder so external destinations copy a launcher/link rather
 * than the raw directory.
 *
 * Returns: a newly allocated file URI for the artifact, or NULL on failure.
 */
gchar* WhiskerMenu::desktop_drag_create_folder_launcher_uri(GFile* folder,
		const char* display_name)
{
	if (!folder)
	{
		return nullptr;
	}
	if (g_file_query_file_type(folder, G_FILE_QUERY_INFO_NONE, nullptr)
			!= G_FILE_TYPE_DIRECTORY)
	{
		return nullptr;
	}

	gchar* folder_uri = g_file_get_uri(folder);
	if (xfce_str_is_empty(folder_uri))
	{
		g_free(folder_uri);
		return nullptr;
	}

	gchar* tmpdir = g_dir_make_tmp("meowmenu-folder-drag-XXXXXX", nullptr);
	if (!tmpdir)
	{
		g_free(folder_uri);
		return nullptr;
	}

	gchar* basename = g_file_get_basename(folder);
	const char* name = !xfce_str_is_empty(display_name)
			? display_name : basename;

	GKeyFile* key_file = g_key_file_new();
	g_key_file_set_string(key_file, "Desktop Entry", "Type", "Link");
	g_key_file_set_string(key_file, "Desktop Entry", "Name",
			name ? name : "Folder");
	g_key_file_set_string(key_file, "Desktop Entry", "URL", folder_uri);
	g_key_file_set_string(key_file, "Desktop Entry", "Icon", "folder");

	gsize length = 0;
	gchar* contents = g_key_file_to_data(key_file, &length, nullptr);
	gchar* path = g_build_filename(tmpdir, "folder.desktop", nullptr);

	gchar* artifact_uri = nullptr;
	if (contents && g_file_set_contents(path, contents, length, nullptr))
	{
		GFile* artifact = g_file_new_for_path(path);
		artifact_uri = g_file_get_uri(artifact);
		g_object_unref(artifact);
	}

	g_free(path);
	g_free(contents);
	g_key_file_unref(key_file);
	g_free(basename);
	g_free(tmpdir);
	g_free(folder_uri);
	return artifact_uri;
}

//-----------------------------------------------------------------------------

/* desktop_drag_cleanup_folder_launcher_uri:
 * @artifact_uri: URI returned by desktop_drag_create_folder_launcher_uri(), or
 *                NULL when no artifact was exported.
 *
 * Removes the transient folder launcher and its private temp directory. The
 * directory cleanup is limited to MeowMenu-created drag temp directories so an
 * unexpected URI cannot remove unrelated parent folders.
 */
void WhiskerMenu::desktop_drag_cleanup_folder_launcher_uri(
		const char* artifact_uri)
{
	if (xfce_str_is_empty(artifact_uri))
	{
		return;
	}

	GFile* artifact = g_file_new_for_uri(artifact_uri);
	gchar* path = g_file_get_path(artifact);
	if (!path)
	{
		g_object_unref(artifact);
		return;
	}

	if (desktop_drag_is_managed_folder_launcher_path(path))
	{
		g_remove(path);
		gchar* parent = g_path_get_dirname(path);
		g_rmdir(parent);
		g_free(parent);
	}

	g_free(path);
	g_object_unref(artifact);
}

//-----------------------------------------------------------------------------

static gboolean desktop_drag_cleanup_folder_launcher_uri_cb(gpointer data)
{
	desktop_drag_cleanup_folder_launcher_uri(static_cast<const char*>(data));
	return G_SOURCE_REMOVE;
}

//-----------------------------------------------------------------------------

/* desktop_drag_schedule_folder_launcher_cleanup:
 * @artifact_uri: URI returned by desktop_drag_create_folder_launcher_uri(), or
 *                NULL when no artifact was exported.
 * @delay_msec: delay before cleanup, in milliseconds.
 *
 * Defers cleanup long enough for the external desktop or file manager to stat
 * and copy the drag payload after GTK reports drag-end. This preserves the
 * export-only contract while avoiding stale temporary files.
 *
 * Returns: the GLib source id for the scheduled cleanup, or 0 for no-op input.
 */
guint WhiskerMenu::desktop_drag_schedule_folder_launcher_cleanup(
		const char* artifact_uri, guint delay_msec)
{
	if (xfce_str_is_empty(artifact_uri))
	{
		return 0;
	}

	return g_timeout_add_full(G_PRIORITY_DEFAULT, delay_msec,
			&desktop_drag_cleanup_folder_launcher_uri_cb,
			g_strdup(artifact_uri),
			reinterpret_cast<GDestroyNotify>(g_free));
}

//-----------------------------------------------------------------------------

int WhiskerMenu::desktop_drag_preview_size()
{
	return IconSize::pixels_for(IconSize::Small);
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::desktop_drag_should_hide_menu_after_end()
{
	return false;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::desktop_drag_context_menu_allows_overwrite()
{
	return false;
}

//-----------------------------------------------------------------------------
