/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_PLACES_ITEM_H
#define WHISKERMENU_PLACES_ITEM_H

#include "launcher/element.h"

#include <string>

#include <gio/gio.h>
#include <gtk/gtk.h>

namespace WhiskerMenu
{

/* PlacesItem:
 *
 * Represents a single file or folder rendered in a Places section. Extends
 * Element so it can live in the shared LauncherView GtkListStore column
 * (COLUMN_LAUNCHER, type Element*). PlacesPage intercepts row-activated and
 * dispatches to PlacesItem::open() instead of Element::run().
 */
class PlacesItem : public Element
{
public:
	PlacesItem(GFile* file, bool is_favourite = false);
	~PlacesItem();

	GFile* get_file() const     { return m_file; }
	const char* get_uri() const { return m_uri.c_str(); }
	bool exists() const         { return m_exists; }
	bool is_directory() const   { return m_is_directory; }
	bool is_favourite() const   { return m_is_favourite; }
	gint64 get_accessed() const { return m_accessed; }
	void set_accessed(gint64 t) { m_accessed = t; }
	void set_is_favourite(bool v) { m_is_favourite = v; }

	void open(GdkScreen* screen);
	void open_containing(GdkScreen* screen);
	void copy_path();
	void open_in_terminal(GdkScreen* screen);
	void open_with(GtkWidget* parent);
	void add_desktop_link(GtkWidget* parent);

	/* search:
	 * @filter: NUL-terminated UTF-8 string already case-folded by the caller
	 *          (use places_filter_casefold()), or empty for "match everything".
	 *
	 * Returns: true when the casefolded display name contains @filter.
	 */
	using Element::search;
	bool search(const gchar* casefolded_filter) const;

	/* places_filter_casefold:
	 * @filter: UTF-8 input; nullptr or empty treated as empty.
	 *
	 * Returns: g_malloc'd casefolded copy (caller frees), or nullptr for empty.
	 */
	static gchar* places_filter_casefold(const gchar* filter);

private:
	GFile* m_file;
	std::string m_uri;
	std::string m_casefolded_name;
	gint64 m_accessed;
	bool m_exists;
	bool m_is_directory;
	bool m_is_favourite;
};

}

#endif // WHISKERMENU_PLACES_ITEM_H
