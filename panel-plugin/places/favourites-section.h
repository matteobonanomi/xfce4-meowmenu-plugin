/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_FAVOURITES_SECTION_H
#define WHISKERMENU_FAVOURITES_SECTION_H

#include "places-section.h"

#include <string>

#include <gio/gio.h>

namespace WhiskerMenu
{

class Settings;

class FavouritesSection : public PlacesSection
{
public:
	enum Mode { MeowMenuOnly, ThunarBookmarks };

	explicit FavouritesSection(Settings* settings);
	~FavouritesSection() override;

	std::vector<PlacesItem*> get_items(int max) override;
	void clear_items() override;
	const gchar* get_icon_name() const override { return "starred"; }
	const gchar* get_display_name() const override;

	Mode get_mode() const { return m_mode; }
	void refresh_mode();

	bool contains(const char* uri) const;
	void add_favourite(const char* uri);    // no-op in ThunarBookmarks mode
	void remove_favourite(const char* uri); // no-op in ThunarBookmarks mode

private:
	void install_monitor();
	void uninstall_monitor();
	std::vector<std::string> read_thunar_bookmarks() const;

private:
	Settings* m_settings;
	Mode m_mode;
	GFileMonitor* m_monitor;
	std::vector<PlacesItem*> m_items;
};

}

#endif // WHISKERMENU_FAVOURITES_SECTION_H
