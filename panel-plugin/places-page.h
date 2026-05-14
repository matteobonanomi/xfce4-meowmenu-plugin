/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_PLACES_PAGE_H
#define WHISKERMENU_PLACES_PAGE_H

#include <string>
#include <vector>

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class FavouritesSection;
class HistorySection;
class HomeSection;
class LauncherView;
class PlacesItem;
class PlacesSection;
class Settings;
class Window;

class PlacesPage
{
public:
	PlacesPage(Settings* settings, Window* window);
	~PlacesPage();

	PlacesPage(const PlacesPage&) = delete;
	PlacesPage& operator=(const PlacesPage&) = delete;

	GtkWidget* get_widget() const { return m_widget; }
	GtkWidget* get_message() const { return m_empty_message; }
	LauncherView* get_view() const { return m_view; }

	HomeSection* get_home_section() const             { return m_home; }
	HistorySection* get_history_section() const       { return m_history; }
	FavouritesSection* get_favourites_section() const { return m_favourites; }
	PlacesSection* get_active_section() const         { return m_active_section; }

	void set_active_section(PlacesSection* section);
	void set_filter(const gchar* filter);
	void refresh_active();
	void reload_view();

private:
	void create_view();
	void on_row_activated(GtkTreePath* path);
	void on_button_press(GdkEventButton* event);
	void show_context_menu(PlacesItem* item, GdkEvent* event);
	void rebuild_model();

	// Home recursive-search dispatch (FR-035, FR-035a–f).
	void start_home_search();
	void cancel_home_search();
	void clear_home_search_items();
	void on_home_search_result(PlacesItem* item);
	void on_home_search_done();
	static gboolean on_debounce_fired(gpointer data);

private:
	Settings* m_settings;
	Window* m_window;

	HomeSection* m_home;
	HistorySection* m_history;
	FavouritesSection* m_favourites;

	PlacesSection* m_active_section;

	LauncherView* m_view;
	GtkWidget* m_widget;        // GtkScrolledWindow wrapping the view
	GtkWidget* m_empty_message; // GtkLabel shown when model has zero rows
	GtkListStore* m_model;

	std::string m_filter; // case-folded text or empty for no-filter

	// Home recursive-search state.
	guint m_debounce_id;
	bool m_home_search_active;
	std::vector<PlacesItem*> m_home_search_items;
};

}

#endif // WHISKERMENU_PLACES_PAGE_H
