/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "window-pages.h"
#include "window.h"

#include "core/category-lifetime.h"
#include "launcher/applications-page.h"
#include "launcher/category-button.h"
#include "launcher/favorites-page.h"
#include "launcher/recent-page.h"
#include "places/home-section.h"
#include "places/places-page.h"
#include "search/search-page.h"
#include "settings.h"
#include "ui/launcher-view.h"

#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

Page* WhiskerMenu::Window::get_active_page()
{
	Page* page = nullptr;
	if (g_strcmp0(gtk_stack_get_visible_child_name(m_panels_stack), "search") == 0)
	{
		page = m_search_results;
	}
	else if (m_favorites->get_button()->get_active())
	{
		page = m_favorites;
	}
	else if (m_recent->get_button()->get_active())
	{
		page = m_recent;
	}
	else
	{
		page = m_applications;
	}
	return page;
}

//-----------------------------------------------------------------------------

GtkWidget* WhiskerMenu::Window::get_active_category_button()
{
	return active_toggle_child_or_default(GTK_CONTAINER(m_category_buttons),
			m_default_button->get_widget());
}

//-----------------------------------------------------------------------------

// NOTE: the three category toggles below switch the visible panel and then hand
// keyboard focus to the search entry so a pointer selection lets the user type
// immediately. A keyboard-driven activation sets m_keyboard_category_nav, in
// which case the handoff is skipped and focus stays on the active category
// button so arrow navigation can continue (FR-002/005/006; C1/C2). The guard,
// and only the guard, distinguishes the keyboard origin from the pointer origin.

void WhiskerMenu::Window::favorites_toggled()
{
	m_favorites->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "favorites");
	if (!m_keyboard_category_nav)
		gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::recent_toggled()
{
	m_recent->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "recent");
	if (!m_keyboard_category_nav)
		gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::category_toggled()
{
	m_applications->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "applications");
	if (!m_keyboard_category_nav)
		gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::reset_default_button()
{
	const int default_base = meow_default_category_order_base(m_strip_lead_spacer
			&& gtk_widget_get_visible(m_strip_lead_spacer));

	switch (m_settings->default_category)
	{
	case Settings::CategoryRecent:
		m_default_button = m_recent->get_button();
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(),
				default_base);
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(),
				default_base + 1);
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(),
				default_base + 2);
		break;

	case Settings::CategoryAll:
		m_default_button = m_applications->get_button();
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(),
				default_base);
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(),
				default_base + 1);
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(),
				default_base + 2);
		break;

	default:
		m_default_button = m_favorites->get_button();
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(),
				default_base);
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(),
				default_base + 1);
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(),
				default_base + 2);
		break;
	}

	// NOTE: the default-button reorders above can push leading controls past the
	// built-ins. Keep the strip spacer first when visible so it can continue to
	// pin the built-ins to the trailing edge across close/reopen.
	if (m_strip_lead_spacer && gtk_widget_get_visible(m_strip_lead_spacer))
	{
		gtk_box_reorder_child(m_category_buttons, m_strip_lead_spacer,
				meow_strip_spacer_order(true));
	}
	if (m_mode_selector_box
			&& gtk_widget_get_parent(GTK_WIDGET(m_mode_selector_box))
					== GTK_WIDGET(m_category_buttons))
	{
		gtk_box_reorder_child(m_category_buttons, GTK_WIDGET(m_mode_selector_box), 0);
	}
	if (m_mode_selector_separator
			&& gtk_widget_get_parent(m_mode_selector_separator)
					== GTK_WIDGET(m_category_buttons))
	{
		gtk_box_reorder_child(m_category_buttons, m_mode_selector_separator, 1);
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_favorites()
{
	// Switch to favorites panel
	m_favorites->get_button()->set_active(true);

	// Clear search entry
	gtk_entry_set_text(m_search_entry, "");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_default_page()
{
	// Switch to favorites panel
	m_default_button->set_active(true);

	// Clear search entry
	gtk_entry_set_text(m_search_entry, "");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::search()
{
	// Fetch search string
	const gchar* text = gtk_entry_get_text(m_search_entry);
	if (xfce_str_is_empty(text))
	{
		text = nullptr;
	}

	if (m_places_active)
	{
		// Places mode: stay on the places panel; filter the active section.
		gtk_stack_set_visible_child_name(m_panels_stack, "places");
		m_places->set_filter(text);
		// FR-014: query empty → return focus to the search entry so the
		// user is back in Browsing-style entry focus.
		if (!text)
		{
			gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		}
		return;
	}

	if (text)
	{
		// Switch the applications area to search results; the sidebar stays visible.
		gtk_stack_set_visible_child_name(m_panels_stack, "search");
	}
	else
	{
		// Restore the panel that matches the currently active category button.
		if (m_favorites->get_button()->get_active())
			gtk_stack_set_visible_child_name(m_panels_stack, "favorites");
		else if (m_recent->get_button()->get_active())
			gtk_stack_set_visible_child_name(m_panels_stack, "recent");
		else
			gtk_stack_set_visible_child_name(m_panels_stack, "applications");
	}

	// Apply filter
	m_search_results->set_filter(text);

	if (text)
	{
		// FR-011: when the query produced at least one result, move
		// keyboard focus to the first result so the user can press
		// Enter to launch it or use arrows to navigate. Subsequent
		// printable keystrokes are still routed back into the entry
		// via the on_key_press_event_after catch-all (FR-012).
		GtkTreeModel* model = m_search_results->get_view()->get_model();
		GtkTreeIter iter;
		if (m_search_results->has_calculator_result())
		{
			gtk_widget_grab_focus(
					m_search_results->get_preferred_focus_widget());
		}
		else if (model && gtk_tree_model_get_iter_first(model, &iter))
		{
			gtk_widget_grab_focus(m_search_results->get_view()->get_widget());
			m_search_results->select_first();
		}
	}
	else
	{
		// FR-014: query became empty (Backspace-to-empty or Esc-clear);
		// return focus to the entry so the user is back in Browsing.
		gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
	}
}

//-----------------------------------------------------------------------------

/* switch_mode:
 * @to_places: true to enter Places mode; false to return to Apps.
 *
 * Toggles the Apps/Places selector visuals, hides/shows the appropriate
 * sidebar buttons, and updates the search-entry placeholder. Re-entrancy is
 * guarded by m_mode_switch_in_progress so the two toggle-button "toggled"
 * signals do not loop.
 */
void WhiskerMenu::Window::switch_mode(bool to_places)
{
	if (m_mode_switch_in_progress)
	{
		return;
	}
	m_mode_switch_in_progress = true;

	m_places_active = to_places;
	gtk_toggle_button_set_active(m_mode_btn_apps,   !to_places);
	gtk_toggle_button_set_active(m_mode_btn_places,  to_places);

	const bool history_visible = m_settings->places_history_enabled;
	const bool fav_visible     = m_settings->places_favourites_enabled;

	gtk_widget_set_visible(m_favorites->get_button()->get_widget(),       !to_places);
	gtk_widget_set_visible(m_recent->get_button()->get_widget(),
			!to_places && m_settings->recent_items_max);
	gtk_widget_set_visible(m_applications->get_button()->get_widget(),    !to_places);

	gtk_widget_set_visible(m_places_home_btn->get_widget(),     to_places);
	gtk_widget_set_visible(m_places_history_btn->get_widget(),  to_places && history_visible);
	gtk_widget_set_visible(m_places_fav_btn->get_widget(),      to_places && fav_visible);

	// Hide app categories (Accessories, Development, ...) in Places mode.
	for (GtkWidget* w : m_app_category_widgets)
	{
		gtk_widget_set_visible(w, !to_places);
	}

	gtk_entry_set_text(m_search_entry, "");
	gtk_entry_set_placeholder_text(m_search_entry,
			to_places ? _("Search places\xe2\x80\xa6")
			          : _("Search applications\xe2\x80\xa6"));

	if (to_places)
	{
		m_places_home_btn->set_active(true);
		m_places->set_active_section(m_places->get_home_section());
		gtk_stack_set_visible_child_name(m_panels_stack, "places");
	}
	else
	{
		// NOTE: Apps and Places buttons are separate radio groups. When Places
		// was entered, the default Apps button was never deactivated, so
		// set_active(true) in show_default_page() is a GTK no-op and the
		// "toggled" handler never fires. Switch the stack explicitly first.
		const char* page = "favorites";
		switch (m_settings->default_category)
		{
		case Settings::CategoryRecent: page = "recent";       break;
		case Settings::CategoryAll:    page = "applications"; break;
		default: break;
		}
		gtk_stack_set_visible_child_name(m_panels_stack, page);
		show_default_page();
	}

	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
	m_mode_switch_in_progress = false;
	update_favourite_drop_targets();
}

//-----------------------------------------------------------------------------
