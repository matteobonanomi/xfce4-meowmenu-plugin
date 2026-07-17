/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_DESKTOP_DRAG_H
#define WHISKERMENU_DESKTOP_DRAG_H

#include <gio/gio.h>
#include <glib.h>

namespace WhiskerMenu
{

static const guint WHISKERMENU_DESKTOP_DRAG_URI_LIST_INFO = 1;

bool desktop_drag_external_uri_enabled(const char* layout_mode);
bool desktop_drag_application_uri_available(const char* layout_mode,
		const char* uri);
bool desktop_drag_places_uri_available(const char* layout_mode,
		bool item_exists, const char* uri);
gchar* desktop_drag_create_folder_launcher_uri(GFile* folder,
		const char* display_name);
void desktop_drag_cleanup_folder_launcher_uri(const char* artifact_uri);
guint desktop_drag_schedule_folder_launcher_cleanup(const char* artifact_uri,
		guint delay_msec);
int desktop_drag_preview_size();
bool desktop_drag_should_hide_menu_after_end();
bool desktop_drag_context_menu_allows_overwrite();

}

#endif // WHISKERMENU_DESKTOP_DRAG_H
