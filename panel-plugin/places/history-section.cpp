/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "history-section.h"

#include "places-item.h"

#include <algorithm>

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

HistorySection::HistorySection() :
	m_manager(gtk_recent_manager_get_default())
{
}

HistorySection::~HistorySection()
{
	clear_items();
}

//-----------------------------------------------------------------------------

const gchar* HistorySection::get_display_name() const
{
	return _("History");
}

//-----------------------------------------------------------------------------

void HistorySection::clear_items()
{
	for (auto* item : m_items)
	{
		delete item;
	}
	m_items.clear();
}

//-----------------------------------------------------------------------------

/* HistorySection::get_items:
 *
 * Reads the system GtkRecentManager store, filters out items marked private,
 * sorts by last-visited time (newest first), caps at @max, and constructs a
 * PlacesItem per file URI. Folders are not included (the documented behavior).
 */
std::vector<PlacesItem*> HistorySection::get_items(int max)
{
	// Rebuild fresh PlacesItem objects on every call (clear_items() + new
	// below), so each item's exists() — and therefore its muted markup and
	// "missing" tooltip — is re-evaluated against the live filesystem at every
	// list rebuild (the documented behavior). No filesystem watch is involved.
	clear_items();
	if (!m_manager)
	{
		return m_items;
	}

	GList* recents = gtk_recent_manager_get_items(m_manager);
	if (!recents)
	{
		return m_items;
	}

	struct Row { GtkRecentInfo* info; gint64 visited; };
	std::vector<Row> rows;
	rows.reserve(g_list_length(recents));
	for (GList* li = recents; li; li = li->next)
	{
		auto* info = static_cast<GtkRecentInfo*>(li->data);
		if (gtk_recent_info_get_private_hint(info))
		{
			gtk_recent_info_unref(info);
			continue;
		}
		const gchar* uri = gtk_recent_info_get_uri(info);
		if (!uri || !g_str_has_prefix(uri, "file:"))
		{
			gtk_recent_info_unref(info);
			continue;
		}
		rows.push_back({ info, (gint64) gtk_recent_info_get_visited(info) });
	}
	g_list_free(recents);

	std::stable_sort(rows.begin(), rows.end(),
		[](const Row& a, const Row& b) { return a.visited > b.visited; });

	if (max > 0 && (int) rows.size() > max)
	{
		for (size_t i = (size_t) max; i < rows.size(); ++i)
		{
			gtk_recent_info_unref(rows[i].info);
		}
		rows.resize((size_t) max);
	}

	for (auto& r : rows)
	{
		const gchar* uri = gtk_recent_info_get_uri(r.info);
		GFile* file = g_file_new_for_uri(uri);
		auto* item = new PlacesItem(file);
		item->set_accessed(r.visited);
		m_items.push_back(item);
		g_object_unref(file);
		gtk_recent_info_unref(r.info);
	}

	return m_items;
}

//-----------------------------------------------------------------------------
