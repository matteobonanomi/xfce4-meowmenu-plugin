/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "home-section.h"

#include "places-item.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

HomeSection::HomeSection() :
	m_worker(nullptr)
{
}

HomeSection::~HomeSection()
{
	cancel_search();
	clear_items();
}

//-----------------------------------------------------------------------------

void HomeSection::start_search(const gchar* casefolded_query, int cap,
		HomeSearchWorker::ResultCallback on_result,
		HomeSearchWorker::DoneCallback on_done)
{
	// NOTE: caller (PlacesPage) is responsible for cancelling first.
	// We assert via cancel_search() defensively so a stale worker can't
	// silently overlap with a new one.
	cancel_search();
	m_worker = HomeSearchWorker::start(casefolded_query, cap,
			std::move(on_result), std::move(on_done));
}

void HomeSection::cancel_search()
{
	if (m_worker)
	{
		m_worker->cancel();
		delete m_worker;
		m_worker = nullptr;
	}
}

//-----------------------------------------------------------------------------

const gchar* HomeSection::get_display_name() const
{
	return _("Home");
}

//-----------------------------------------------------------------------------

void HomeSection::clear_items()
{
	for (auto* item : m_items)
	{
		delete item;
	}
	m_items.clear();
}

//-----------------------------------------------------------------------------

/* HomeSection::get_items:
 *
 * Builds a PlacesItem for each XDG user directory that exists on disk, in the
 * FR-011 order. Non-existent paths are skipped so the list reflects the user's
 * actual home layout.
 */
std::vector<PlacesItem*> HomeSection::get_items(int /*max*/)
{
	clear_items();

	const GUserDirectory xdg_dirs[] = {
		G_USER_DIRECTORY_DESKTOP,
		G_USER_DIRECTORY_DOCUMENTS,
		G_USER_DIRECTORY_DOWNLOAD,
		G_USER_DIRECTORY_MUSIC,
		G_USER_DIRECTORY_PICTURES,
		G_USER_DIRECTORY_VIDEOS,
		G_USER_DIRECTORY_TEMPLATES,
		G_USER_DIRECTORY_PUBLIC_SHARE,
	};

	auto push_path_if_exists = [this](const gchar* path)
	{
		if (!path || !*path || !g_file_test(path, G_FILE_TEST_IS_DIR))
		{
			return;
		}
		GFile* file = g_file_new_for_path(path);
		m_items.push_back(new PlacesItem(file));
		g_object_unref(file);
	};

	// $HOME first, then the eight XDG specials that exist on disk.
	push_path_if_exists(g_get_home_dir());
	for (GUserDirectory d : xdg_dirs)
	{
		push_path_if_exists(g_get_user_special_dir(d));
	}

	return m_items;
}

//-----------------------------------------------------------------------------
