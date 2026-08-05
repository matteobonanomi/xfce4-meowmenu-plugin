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
#include "places/favourites-section.h"
#include "places/history-section.h"
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
// button so arrow navigation can continue (the documented behavior; C1/C2). The guard,
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
	const bool vertical_switch_controls = m_mode_selector_upper_separator
			&& m_mode_selector_box
			&& m_mode_selector_separator
			&& gtk_widget_get_parent(m_mode_selector_upper_separator)
					== GTK_WIDGET(m_category_buttons)
			&& gtk_widget_get_parent(GTK_WIDGET(m_mode_selector_box))
					== GTK_WIDGET(m_category_buttons)
			&& gtk_widget_get_parent(m_mode_selector_separator)
					== GTK_WIDGET(m_category_buttons);
	const int default_base = meow_default_category_order_base(m_strip_lead_spacer
			&& gtk_widget_get_visible(m_strip_lead_spacer), vertical_switch_controls);

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
		gtk_box_reorder_child(m_category_buttons, GTK_WIDGET(m_mode_selector_box), 1);
	}
	if (m_mode_selector_upper_separator
			&& gtk_widget_get_parent(m_mode_selector_upper_separator)
					== GTK_WIDGET(m_category_buttons))
	{
		gtk_box_reorder_child(m_category_buttons, m_mode_selector_upper_separator, 0);
	}
	if (m_mode_selector_separator
			&& gtk_widget_get_parent(m_mode_selector_separator)
					== GTK_WIDGET(m_category_buttons))
	{
		gtk_box_reorder_child(m_category_buttons, m_mode_selector_separator, 2);
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_favorites()
{
	if (m_places_active)
	{
		return;
	}

	// Switch to favorites panel
	m_favorites->get_button()->set_active(true);

	// Clear search entry
	gtk_entry_set_text(m_search_entry, "");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_default_page()
{
	if (m_places_active)
	{
		m_places_home_btn->set_active(true);
		m_places->set_active_section(m_places->get_home_section());
		gtk_stack_set_visible_child_name(m_panels_stack, "places");
		gtk_entry_set_text(m_search_entry, "");
		gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		return;
	}

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
		if (text)
			m_places->focus_first_result();
		// the documented behavior: query empty → return focus to the search entry so the
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
		m_search_results->focus_first_visual_result();
	}
	else
	{
		// the documented behavior: query became empty (Backspace-to-empty or Esc-clear);
		// return focus to the entry so the user is back in Browsing.
		gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
	}
}

//-----------------------------------------------------------------------------

/* current_menu_content:
 *
 * Maps the active Places section into the pure presentation vocabulary.
 * Applications content is deliberately opaque because live reevaluation keeps
 * its current category or search surface.
 *
 * Returns: the current Places section, or RetainCurrent for Applications.
 */
MenuContentTarget WhiskerMenu::Window::current_menu_content() const
{
	if (!m_places_active)
	{
		return MenuContentTarget::RetainCurrent;
	}
	if (m_places->get_active_section() == m_places->get_history_section())
	{
		return MenuContentTarget::PlacesHistory;
	}
	if (m_places->get_active_section() == m_places->get_favourites_section())
	{
		return MenuContentTarget::PlacesFavourites;
	}
	return MenuContentTarget::PlacesHome;
}

//-----------------------------------------------------------------------------

/* apply_menu_mode:
 * @requested_mode: desired top-level mode; unavailable Places resolves to Apps.
 * @transition: Enter selects the mode default, Reevaluate retains valid content.
 *
 * Applies selector state, search context, control visibility, and content as
 * one guarded transaction. This is the only Window path that owns the complete
 * Applications/Places presentation matrix.
 */
void WhiskerMenu::Window::apply_menu_mode(MenuMode requested_mode,
		MenuModeTransition transition)
{
	if (m_mode_switch_in_progress)
	{
		return;
	}
	m_mode_switch_in_progress = true;

	MenuModeInputs inputs;
	inputs.requested_mode = requested_mode;
	inputs.transition = transition;
	inputs.current_content = current_menu_content();
	inputs.places_enabled = m_settings->places_enabled;
	inputs.recent_applications_enabled = m_settings->recent_items_max;
	inputs.places_history_enabled = m_settings->places_history_enabled;
	inputs.places_favourites_enabled = m_settings->places_favourites_enabled;

	// A live setting can invalidate Places itself. Treat that forced mode change
	// as entry so the result cannot retain Places content behind Apps controls.
	const MenuMode prior_mode = m_places_active
			? MenuMode::Places : MenuMode::Applications;
	MenuModeResolution resolution = resolve_menu_mode(inputs);
	if (resolution.mode != prior_mode
			&& transition == MenuModeTransition::Reevaluate)
	{
		inputs.transition = MenuModeTransition::Enter;
		resolution = resolve_menu_mode(inputs);
	}
	if (resolution.mode != prior_mode)
		m_places->invalidate_focus_lease();

	const bool places = resolution.mode == MenuMode::Places;
	m_places_active = places;
	gtk_toggle_button_set_active(m_mode_btn_apps, !places);
	gtk_toggle_button_set_active(m_mode_btn_places, places);

	gtk_widget_set_visible(m_favorites->get_button()->get_widget(),
			resolution.applications_favourites_visible);
	gtk_widget_set_visible(m_recent->get_button()->get_widget(),
			resolution.applications_recent_visible);
	gtk_widget_set_visible(m_applications->get_button()->get_widget(),
			resolution.applications_all_visible);
	gtk_widget_set_visible(m_places_home_btn->get_widget(),
			resolution.places_home_visible);
	gtk_widget_set_visible(m_places_history_btn->get_widget(),
			resolution.places_history_visible);
	gtk_widget_set_visible(m_places_fav_btn->get_widget(),
			resolution.places_favourites_visible);
	for (GtkWidget* widget : m_app_category_widgets)
	{
		gtk_widget_set_visible(widget,
				resolution.application_categories_visible);
	}

	gtk_entry_set_placeholder_text(m_search_entry, places
			? _("Search places\xe2\x80\xa6")
			: _("Search applications\xe2\x80\xa6"));

	if (inputs.transition == MenuModeTransition::Enter)
	{
		gtk_entry_set_text(m_search_entry, "");
	}

	switch (resolution.content)
	{
	case MenuContentTarget::ApplicationsDefault:
	{
		const char* page = "favorites";
		switch (m_settings->default_category)
		{
		case Settings::CategoryRecent: page = "recent";       break;
		case Settings::CategoryAll:    page = "applications"; break;
		default: break;
		}
		gtk_stack_set_visible_child_name(m_panels_stack, page);
		m_default_button->set_active(true);
		break;
	}

	case MenuContentTarget::PlacesHistory:
		m_places_history_btn->set_active(true);
		m_places->set_active_section(m_places->get_history_section());
		gtk_stack_set_visible_child_name(m_panels_stack, "places");
		break;

	case MenuContentTarget::PlacesFavourites:
		m_places_fav_btn->set_active(true);
		m_places->set_active_section(m_places->get_favourites_section());
		gtk_stack_set_visible_child_name(m_panels_stack, "places");
		break;

	case MenuContentTarget::PlacesHome:
		m_places_home_btn->set_active(true);
		m_places->set_active_section(m_places->get_home_section());
		gtk_stack_set_visible_child_name(m_panels_stack, "places");
		break;

	case MenuContentTarget::RetainCurrent:
		break;
	}

	if (inputs.transition == MenuModeTransition::Enter)
	{
		gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		if (places)
		{
			if (!m_places->focus_first_result())
				gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		}
		else if (!m_search_results->focus_first_visual_result())
		{
			gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		}
	}
	m_mode_switch_in_progress = false;
	update_favourite_drop_targets();
}

//-----------------------------------------------------------------------------

/* switch_mode:
 * @to_places: true to enter Places mode; false to return to Applications.
 *
 * Public mode changes are mode-entry transactions, so Places always enters on
 * Home and Applications enters on its configured valid default category.
 */
void WhiskerMenu::Window::switch_mode(bool to_places)
{
	apply_menu_mode(to_places ? MenuMode::Places : MenuMode::Applications,
			MenuModeTransition::Enter);
}

//-----------------------------------------------------------------------------
