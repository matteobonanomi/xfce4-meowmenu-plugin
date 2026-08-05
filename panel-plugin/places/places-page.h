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
#include <cstdint>
#include <vector>

#include <gtk/gtk.h>

namespace WhiskerMenu
{

static const char WHISKERMENU_PLACES_FAVOURITE_DND_TARGET[] =
		"application/x-meowmenu-places-favourite";
static const guint WHISKERMENU_PLACES_FAVOURITE_DND_INFO = 3;

class FavouritesSection;
class HistorySection;
class HomeSection;
class LauncherView;
class PlacesItem;
class PlacesSection;
class Settings;
class Window;

enum class PlacesFocusLeaseState : unsigned
{
	Idle,
	AwaitingFirst,
	Relinquished,
	Settled,
};

/* PlacesFocusLease:
 *
 * Tracks the short-lived right to focus the first result of one recursive
 * Home query. It is deliberately independent of GTK and the worker so stale
 * callbacks can be rejected before touching the visible model.
 */
class PlacesFocusLease
{
public:
	/* begin:
	 * Starts a new query identity and returns its generation token.
	 */
	std::uint64_t begin(const std::string& query,
			const PlacesSection* section, bool mode_active)
	{
		++m_generation;
		m_query = query;
		m_section = section;
		m_mode_active = mode_active;
		m_first_result_seen = false;
		m_state = mode_active && !query.empty() && section
				? PlacesFocusLeaseState::AwaitingFirst
				: PlacesFocusLeaseState::Idle;
		return m_generation;
	}

	/* invalidate:
	 * Revokes every callback associated with the current query identity.
	 */
	void invalidate()
	{
		++m_generation;
		m_query.clear();
		m_section = nullptr;
		m_mode_active = false;
		m_first_result_seen = false;
		m_state = PlacesFocusLeaseState::Idle;
	}

	/* matches:
	 * Confirms that a worker callback still belongs to the active query.
	 */
	bool matches(std::uint64_t generation, const std::string& query,
			const PlacesSection* section, bool mode_active) const
	{
		return generation == m_generation && query == m_query
				&& section == m_section && mode_active == m_mode_active
				&& mode_active;
	}

	/* claim_first:
	 * Consumes the one automatic focus claim for a current first result.
	 */
	bool claim_first(std::uint64_t generation, const std::string& query,
			const PlacesSection* section, bool mode_active)
	{
		if (!matches(generation, query, section, mode_active)
				|| m_state != PlacesFocusLeaseState::AwaitingFirst
				|| m_first_result_seen)
			return false;
		m_first_result_seen = true;
		m_state = PlacesFocusLeaseState::Settled;
		return true;
	}

	/* relinquish:
	 * Makes subsequent current results display-only after deliberate movement.
	 */
	void relinquish()
	{
		if (m_state == PlacesFocusLeaseState::AwaitingFirst)
			m_state = PlacesFocusLeaseState::Relinquished;
	}

	/* settle_empty:
	 * Closes an empty current search without creating a focus target.
	 */
	void settle_empty(std::uint64_t generation, const std::string& query,
			const PlacesSection* section, bool mode_active)
	{
		if (matches(generation, query, section, mode_active)
				&& m_state == PlacesFocusLeaseState::AwaitingFirst)
			m_state = PlacesFocusLeaseState::Settled;
	}

	std::uint64_t generation() const { return m_generation; }
	PlacesFocusLeaseState state() const { return m_state; }

private:
	std::uint64_t m_generation = 0;
	std::string m_query;
	const PlacesSection* m_section = nullptr;
	PlacesFocusLeaseState m_state = PlacesFocusLeaseState::Idle;
	bool m_mode_active = false;
	bool m_first_result_seen = false;
};

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
	void select_first();
	bool focus_first_result();
	void note_deliberate_navigation();
	void invalidate_focus_lease();

private:
	void create_view();
	void on_row_activated(GtkTreePath* path);
	void on_button_press(GdkEventButton* event);
	gboolean on_button_release(GdkEventButton* event);
	void on_drag_begin(GdkDragContext* context);
	void on_drag_data_get(GtkSelectionData* data, guint info);
	void on_drag_end();
	void clear_drag_state(bool defer_folder_cleanup = false);
	void show_context_menu(PlacesItem* item, GdkEvent* event);
	void rebuild_model();

	// Home recursive-search dispatch (the documented behavior, the documented behavior–f).
	void start_home_search();
	void cancel_home_search();
	void clear_home_search_items();
	void on_home_search_result(PlacesItem* item, std::uint64_t generation);
	void on_home_search_done(std::uint64_t generation);
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
	bool m_item_dragged;
	PlacesItem* m_pressed_drag_item;
	guint m_pressed_drag_info;
	std::string m_folder_drag_artifact_uri;

	std::string m_filter; // case-folded text or empty for no-filter

	// Home recursive-search state.
	guint m_debounce_id;
	bool m_home_search_active;
	std::vector<PlacesItem*> m_home_search_items;
	PlacesFocusLease m_focus_lease;
};

}

#endif // WHISKERMENU_PLACES_PAGE_H
