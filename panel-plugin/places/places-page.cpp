/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "places-page.h"

#include "favourites-section.h"
#include "history-section.h"
#include "home-section.h"
#include "ui/image-menu-item.h"
#include "ui/launcher-icon-view.h"
#include "ui/launcher-tree-view.h"
#include "ui/launcher-view.h"
#include "places-item.h"
#include "places-section.h"
#include "settings.h"
#include "ui/slot.h"
#include "core/window.h"

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

PlacesPage::PlacesPage(Settings* settings, Window* window) :
	m_settings(settings),
	m_window(window),
	m_home(new HomeSection()),
	m_history(new HistorySection()),
	m_favourites(new FavouritesSection(settings)),
	m_active_section(nullptr),
	m_view(nullptr),
	m_widget(nullptr),
	m_empty_message(nullptr),
	m_model(nullptr),
	m_debounce_id(0),
	m_home_search_active(false)
{
	m_model = gtk_list_store_new(
			LauncherView::N_COLUMNS,
			G_TYPE_ICON,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);

	create_view();

	m_widget = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_widget),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(m_widget),
			GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(m_widget), m_view->get_widget());
	g_object_ref_sink(m_widget);

	gtk_style_context_add_class(gtk_widget_get_style_context(m_widget), "launchers-pane");

	m_empty_message = gtk_label_new(_("No items to show."));
	gtk_widget_set_halign(m_empty_message, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(m_empty_message, GTK_ALIGN_CENTER);
	g_object_ref_sink(m_empty_message);
}

//-----------------------------------------------------------------------------

PlacesPage::~PlacesPage()
{
	cancel_home_search();
	clear_home_search_items();
	delete m_view;
	if (m_widget)
	{
		gtk_widget_destroy(m_widget);
		g_object_unref(m_widget);
	}
	if (m_empty_message)
	{
		g_object_unref(m_empty_message);
	}
	if (m_model)
	{
		g_object_unref(m_model);
	}

	delete m_home;
	delete m_history;
	delete m_favourites;
}

//-----------------------------------------------------------------------------

void PlacesPage::create_view()
{
	if (m_settings->view_mode == Settings::ViewAsIcons)
	{
		m_view = new LauncherIconView(m_settings);
		connect(m_view->get_widget(), "item-activated",
			[this](GtkIconView*, GtkTreePath* path) { on_row_activated(path); });
	}
	else
	{
		m_view = new LauncherTreeView(m_settings);
		connect(m_view->get_widget(), "row-activated",
			[this](GtkTreeView*, GtkTreePath* path, GtkTreeViewColumn*) { on_row_activated(path); });
	}

	connect(m_view->get_widget(), "button-press-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			on_button_press(reinterpret_cast<GdkEventButton*>(event));
			return GDK_EVENT_PROPAGATE;
		});

	m_view->set_model(GTK_TREE_MODEL(m_model));
}

//-----------------------------------------------------------------------------

/* reload_view:
 *
 * Recreates the underlying LauncherView when /view-mode changes, mirroring
 * Page::update_view() so Places mode honours the same Results View setting.
 */
void PlacesPage::reload_view()
{
	if (!m_view || !m_widget)
	{
		return;
	}
	const bool is_icons = (m_settings->view_mode == Settings::ViewAsIcons);
	const bool already_icons = (dynamic_cast<LauncherIconView*>(m_view) != nullptr);
	if (is_icons == already_icons)
	{
		return;
	}

	gtk_container_remove(GTK_CONTAINER(m_widget), m_view->get_widget());
	delete m_view;
	m_view = nullptr;
	create_view();
	gtk_container_add(GTK_CONTAINER(m_widget), m_view->get_widget());
	gtk_widget_show_all(m_widget);
}

//-----------------------------------------------------------------------------

/* select_first:
 * Select and scroll to the first item in the view, mirroring Page::select_first
 * so callers that handle both Apps and Places mode can treat them uniformly.
 */
void PlacesPage::select_first()
{
	GtkTreeModel* model = m_view->get_model();
	GtkTreeIter iter;
	if (model && gtk_tree_model_get_iter_first(model, &iter))
	{
		GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
		m_view->set_cursor(path);
		m_view->select_path(path);
		m_view->scroll_to_path(path);
		gtk_tree_path_free(path);
	}
}

//-----------------------------------------------------------------------------

void PlacesPage::set_active_section(PlacesSection* section)
{
	cancel_home_search();
	clear_home_search_items();
	m_active_section = section;
	rebuild_model();
}

void PlacesPage::refresh_active()
{
	cancel_home_search();
	clear_home_search_items();
	rebuild_model();
}

//-----------------------------------------------------------------------------

/* set_filter:
 *
 * Section-aware dispatcher. For non-Home sections or queries below the
 * 2-character recursive-search threshold (FR-035, FR-035a), keeps the
 * existing visible-items substring filter. For Home with a query of at
 * least 2 characters, cancels any in-flight recursive walk, debounces
 * 150 ms, then kicks off a HomeSearchWorker via HomeSection::start_search.
 */
void PlacesPage::set_filter(const gchar* filter)
{
	gchar* folded = PlacesItem::places_filter_casefold(filter);
	m_filter = folded ? folded : "";
	g_free(folded);

	const bool home_active = (m_active_section == m_home);
	const bool long_enough = g_utf8_strlen(m_filter.c_str(), -1) >= 2;

	if (home_active && long_enough)
	{
		// Cancel any pending debounce or in-flight worker; we'll
		// schedule a fresh run after the debounce window.
		cancel_home_search();
		// Clear visible model immediately so stale results don't linger
		// while the new walk is being prepared.
		gtk_list_store_clear(m_model);
		clear_home_search_items();
		gtk_widget_set_visible(m_empty_message, true);

		m_home_search_active = true;
		m_debounce_id = g_timeout_add(150,
				&PlacesPage::on_debounce_fired, this);
		return;
	}

	// Non-Home or short filter: fall back to existing visible-items filter.
	cancel_home_search();
	clear_home_search_items();
	rebuild_model();
}

//-----------------------------------------------------------------------------

gboolean PlacesPage::on_debounce_fired(gpointer data)
{
	auto* self = static_cast<PlacesPage*>(data);
	self->m_debounce_id = 0;
	self->start_home_search();
	return G_SOURCE_REMOVE;
}

void PlacesPage::start_home_search()
{
	if (!m_home || m_filter.empty())
	{
		return;
	}
	const int cap = m_settings ? (int) m_settings->places_max_items : 20;
	if (cap <= 0)
	{
		on_home_search_done();
		return;
	}

	m_home->start_search(m_filter.c_str(), cap,
			[this](PlacesItem* item) { on_home_search_result(item); },
			[this]() { on_home_search_done(); });
}

void PlacesPage::cancel_home_search()
{
	if (m_debounce_id != 0)
	{
		g_source_remove(m_debounce_id);
		m_debounce_id = 0;
	}
	if (m_home)
	{
		m_home->cancel_search();
	}
	m_home_search_active = false;
}

void PlacesPage::clear_home_search_items()
{
	for (auto* item : m_home_search_items)
	{
		delete item;
	}
	m_home_search_items.clear();
}

void PlacesPage::on_home_search_result(PlacesItem* item)
{
	if (!item)
	{
		return;
	}
	// Take ownership and append shallow-first into the model. Use the same
	// availability-driven markup/tooltip feed as rebuild_model() so missing
	// home-search results render muted consistently.
	m_home_search_items.push_back(item);
	gtk_list_store_insert_with_values(
			m_model, nullptr, G_MAXINT,
			LauncherView::COLUMN_ICON, item->get_icon(),
			LauncherView::COLUMN_TEXT, item->get_display_markup(),
			LauncherView::COLUMN_TOOLTIP, item->get_tooltip(),
			LauncherView::COLUMN_LAUNCHER, static_cast<Element*>(item),
			-1);
	gtk_widget_set_visible(m_empty_message, false);
}

void PlacesPage::on_home_search_done()
{
	// Finalize the worker; safe to call from inside the worker's own
	// done callback because HomeSection holds the worker via a
	// raw pointer and HomeSearchWorker keeps its callbacks alive via
	// the Sink shared_ptr until this idle returns.
	if (m_home)
	{
		m_home->cancel_search();
	}
	m_home_search_active = false;

	// If the walk produced no matches at all, surface the empty-state.
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_model), &iter))
	{
		gtk_widget_set_visible(m_empty_message, true);
	}
}

//-----------------------------------------------------------------------------

void PlacesPage::rebuild_model()
{
	gtk_list_store_clear(m_model);

	if (!m_active_section)
	{
		gtk_widget_set_visible(m_empty_message, true);
		return;
	}

	const int max = m_settings ? (int) m_settings->places_max_items : 20;
	const auto items = m_active_section->get_items(max);
	const char* filter = m_filter.empty() ? nullptr : m_filter.c_str();

	int matched = 0;
	for (auto* item : items)
	{
		if (filter && !item->search(filter))
		{
			continue;
		}
		// Feed the availability-driven markup so missing items render muted;
		// get_tooltip() already carries the "missing" cue for those items.
		gtk_list_store_insert_with_values(
				m_model, nullptr, G_MAXINT,
				LauncherView::COLUMN_ICON, item->get_icon(),
				LauncherView::COLUMN_TEXT, item->get_display_markup(),
				LauncherView::COLUMN_TOOLTIP, item->get_tooltip(),
				LauncherView::COLUMN_LAUNCHER, static_cast<Element*>(item),
				-1);
		++matched;
	}

	gtk_widget_set_visible(m_empty_message, matched == 0);
}

//-----------------------------------------------------------------------------

void PlacesPage::on_row_activated(GtkTreePath* path)
{
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(m_model), &iter, path))
	{
		return;
	}
	Element* element = nullptr;
	gtk_tree_model_get(GTK_TREE_MODEL(m_model), &iter,
			LauncherView::COLUMN_LAUNCHER, &element, -1);
	auto* item = dynamic_cast<PlacesItem*>(element);
	if (!item)
	{
		return;
	}
	// Missing target: primary activation is inert — do not close the menu,
	// do not launch, and show no error. Recovery is offered through the
	// context menu ("Open Containing Folder").
	if (!item->exists())
	{
		return;
	}
	GdkScreen* screen = gtk_widget_get_screen(m_view->get_widget());
	m_window->hide();
	item->open(screen);
}

//-----------------------------------------------------------------------------

void PlacesPage::on_button_press(GdkEventButton* event)
{
	if (!event || !gdk_event_triggers_context_menu(reinterpret_cast<GdkEvent*>(event)))
	{
		return;
	}
	GtkTreePath* path = m_view->get_path_at_pos(event->x, event->y);
	if (!path)
	{
		return;
	}
	GtkTreeIter iter;
	Element* element = nullptr;
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(m_model), &iter, path))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(m_model), &iter,
				LauncherView::COLUMN_LAUNCHER, &element, -1);
	}
	gtk_tree_path_free(path);

	auto* item = dynamic_cast<PlacesItem*>(element);
	if (item)
	{
		show_context_menu(item, reinterpret_cast<GdkEvent*>(event));
	}
}

//-----------------------------------------------------------------------------

/* show_context_menu:
 *
 * Builds the per-item context menu fresh on every right-click (FR-031). Action
 * visibility depends on item type and active favourite-sync mode. Add/Remove
 * Favourites are omitted in ThunarBookmarks mode (read-only).
 */
void PlacesPage::show_context_menu(PlacesItem* item, GdkEvent* event)
{
	GtkWidget* menu = gtk_menu_new();
	connect(menu, "selection-done",
		[](GtkMenuShell* shell) { gtk_widget_destroy(GTK_WIDGET(shell)); });

	auto append = [menu](GtkWidget* w)
	{
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), w);
	};

	GtkWidget* mi = whiskermenu_image_menu_item_new("document-open", _("_Open"));
	connect(mi, "activate",
		[this, item](GtkMenuItem*)
		{
			GdkScreen* s = gtk_widget_get_screen(m_view->get_widget());
			m_window->hide();
			item->open(s);
		});
	// "Open" stays present but insensitive for a missing target, matching the
	// inert primary activation; "Open Containing Folder" below remains enabled
	// so a renamed file can still be located.
	gtk_widget_set_sensitive(mi, item->exists());
	append(mi);

	const bool meow_only = m_favourites
			&& m_favourites->get_mode() == FavouritesSection::MeowMenuOnly;
	if (meow_only)
	{
		if (item->is_favourite())
		{
			// Left enabled for missing favourites (not gated on exists()) so a
			// stale favourite can always be removed even though "Open" above is
			// greyed — this is the greyed-out-favourite behaviour from 005.
			mi = whiskermenu_image_menu_item_new("list-remove", _("Remove from Favourites"));
			connect(mi, "activate",
				[this, item](GtkMenuItem*)
				{
					m_favourites->remove_favourite(item->get_uri());
					refresh_active();
				});
			append(mi);
		}
		else
		{
			mi = whiskermenu_image_menu_item_new("bookmark-new", _("Add to Favourites"));
			connect(mi, "activate",
				[this, item](GtkMenuItem*)
				{
					m_favourites->add_favourite(item->get_uri());
					refresh_active();
				});
			append(mi);
		}
	}

	// Offered for both recent and favourite items, and intentionally left
	// enabled even when the target is missing so a renamed file can be located
	// from its parent folder. PlacesItem::open_containing() fails silently (no
	// dialog) when the parent is also gone.
	mi = whiskermenu_image_menu_item_new("folder-open", _("Open Containing Folder"));
	connect(mi, "activate",
		[this, item](GtkMenuItem*)
		{
			GdkScreen* s = gtk_widget_get_screen(m_view->get_widget());
			m_window->hide();
			item->open_containing(s);
		});
	append(mi);

	mi = whiskermenu_image_menu_item_new("edit-copy", _("Copy Path"));
	connect(mi, "activate",
		[item](GtkMenuItem*) { item->copy_path(); });
	append(mi);

	if (item->is_directory())
	{
		mi = whiskermenu_image_menu_item_new("utilities-terminal", _("Open in Terminal"));
		connect(mi, "activate",
			[this, item](GtkMenuItem*)
			{
				GdkScreen* s = gtk_widget_get_screen(m_view->get_widget());
				m_window->hide();
				item->open_in_terminal(s);
			});
		append(mi);
	}
	else
	{
		mi = whiskermenu_image_menu_item_new("system-run", _("Open With…"));
		connect(mi, "activate",
			[this, item](GtkMenuItem*) { item->open_with(m_widget); });
		append(mi);

		mi = whiskermenu_image_menu_item_new("list-add", _("Add Desktop Link"));
		connect(mi, "activate",
			[this, item](GtkMenuItem*) { item->add_desktop_link(m_widget); });
		append(mi);
	}

	gtk_widget_show_all(menu);
	m_window->set_child_has_focus();
	gtk_menu_attach_to_widget(GTK_MENU(menu), m_view->get_widget(), nullptr);
	gtk_menu_popup_at_pointer(GTK_MENU(menu), event);
}

//-----------------------------------------------------------------------------
