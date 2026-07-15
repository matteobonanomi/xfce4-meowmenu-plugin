/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "favourites-section.h"

#include "places-item.h"
#include "settings.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

static bool favourite_uri_exists(const std::string& uri)
{
	GFile* file = g_file_new_for_uri(uri.c_str());
	const bool exists = file && g_file_query_exists(file, nullptr);
	if (file)
	{
		g_object_unref(file);
	}
	return exists;
}

//-----------------------------------------------------------------------------

FavouritesSection::FavouritesSection(Settings* settings) :
	m_settings(settings),
	m_mode(MeowMenuOnly),
	m_monitor(nullptr)
{
	refresh_mode();
}

FavouritesSection::~FavouritesSection()
{
	uninstall_monitor();
	clear_items();
}

//-----------------------------------------------------------------------------

const gchar* FavouritesSection::get_display_name() const
{
	return _("Favourites");
}

//-----------------------------------------------------------------------------

void FavouritesSection::clear_items()
{
	for (auto* item : m_items)
	{
		delete item;
	}
	m_items.clear();
}

//-----------------------------------------------------------------------------

void FavouritesSection::refresh_mode()
{
	const bool wants_thunar = m_settings
			&& (m_settings->places_favourite_sync == "thunar");
	const Mode wanted = wants_thunar ? ThunarBookmarks : MeowMenuOnly;
	if (wanted == m_mode && (m_monitor || !wants_thunar))
	{
		return;
	}
	uninstall_monitor();
	m_mode = wanted;
	if (m_mode == ThunarBookmarks)
	{
		install_monitor();
	}
}

void FavouritesSection::install_monitor()
{
	// NOTE: GTK 3 stores bookmarks as plain-text URIs (file://path [optional label])
	// at ~/.config/gtk-3.0/bookmarks. Watching the file lets us pick up edits made
	// from Thunar's sidebar without polling.
	gchar* path = g_build_filename(g_get_user_config_dir(), "gtk-3.0", "bookmarks", nullptr);
	GFile* file = g_file_new_for_path(path);
	m_monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, nullptr, nullptr);
	g_object_unref(file);
	g_free(path);
}

void FavouritesSection::uninstall_monitor()
{
	if (m_monitor)
	{
		g_object_unref(m_monitor);
		m_monitor = nullptr;
	}
}

//-----------------------------------------------------------------------------

std::vector<std::string> FavouritesSection::read_thunar_bookmarks() const
{
	std::vector<std::string> uris;
	gchar* path = g_build_filename(g_get_user_config_dir(), "gtk-3.0", "bookmarks", nullptr);
	std::ifstream in(path);
	g_free(path);
	if (!in.is_open())
	{
		return uris;
	}
	std::string line;
	while (std::getline(in, line))
	{
		// Strip optional label after the first space.
		auto sp = line.find(' ');
		if (sp != std::string::npos)
		{
			line.resize(sp);
		}
		if (line.empty() || line[0] == '#')
		{
			continue;
		}
		uris.push_back(line);
	}
	return uris;
}

//-----------------------------------------------------------------------------

bool FavouritesSection::contains(const char* uri) const
{
	if (!uri || !*uri || !m_settings)
	{
		return false;
	}
	if (m_mode == MeowMenuOnly)
	{
		return m_settings->places_favourites.find(uri) >= 0;
	}
	// Thunar mode — read-only check.
	auto bookmarks = read_thunar_bookmarks();
	for (const auto& b : bookmarks)
	{
		if (b == uri)
		{
			return true;
		}
	}
	return false;
}

void FavouritesSection::add_favourite(const char* uri)
{
	if (m_mode != MeowMenuOnly || !m_settings || !uri || !*uri)
	{
		return;
	}
	if (m_settings->places_favourites.find(uri) >= 0)
	{
		return;
	}
	m_settings->places_favourites.push_back(uri);
}

void FavouritesSection::remove_favourite(const char* uri)
{
	if (m_mode != MeowMenuOnly || !m_settings || !uri || !*uri)
	{
		return;
	}
	int idx = m_settings->places_favourites.find(uri);
	if (idx >= 0)
	{
		m_settings->places_favourites.erase(idx);
	}
}

//-----------------------------------------------------------------------------

/* FavouritesSection::get_items:
 *
 * Reads the active sync source (Xfconf StringList or ~/.config/gtk-3.0/bookmarks),
 * hides missing URIs before row construction, caps at @max, and builds a
 * PlacesItem per visible URI. MeowMenu-owned stale URIs are removed from
 * Xfconf; Thunar bookmarks remain read-only and are only omitted from the
 * visible model.
 */
std::vector<PlacesItem*> FavouritesSection::get_items(int max)
{
	clear_items();

	std::vector<std::string> uris;
	if (m_mode == MeowMenuOnly && m_settings)
	{
		for (int i = 0; i < m_settings->places_favourites.size(); ++i)
		{
			const std::string uri = m_settings->places_favourites[i];
			if (!favourite_uri_exists(uri))
			{
				m_settings->places_favourites.erase(i);
				--i;
				continue;
			}

			if (max <= 0 || (int) uris.size() < max)
			{
				uris.push_back(uri);
			}
		}
	}
	else
	{
		for (const auto& uri : read_thunar_bookmarks())
		{
			if (!favourite_uri_exists(uri))
			{
				continue;
			}
			if (max > 0 && (int) uris.size() >= max)
			{
				break;
			}
			uris.push_back(uri);
		}
	}

	for (const auto& uri : uris)
	{
		GFile* file = g_file_new_for_uri(uri.c_str());
		if (!file)
		{
			continue;
		}
		m_items.push_back(new PlacesItem(file, /*is_favourite*/ true));
		g_object_unref(file);
	}

	return m_items;
}

//-----------------------------------------------------------------------------
