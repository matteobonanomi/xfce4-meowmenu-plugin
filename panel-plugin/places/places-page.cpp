/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "places-page.h"

#include "core/desktop-drag.h"
#include "favourites-section.h"
#include "history-section.h"
#include "home-section.h"
#include "launcher/page.h"
#include "ui/grid-cell-metrics.h"
#include "ui/grid-presentation.h"
#include "ui/image-menu-item.h"
#include "ui/launcher-icon-view.h"
#include "ui/launcher-tree-view.h"
#include "ui/launcher-view.h"
#include "places-item.h"
#include "places-section.h"
#include "settings.h"
#include "ui/slot.h"
#include "core/window.h"

#include <cstring>

#include <libxfce4ui/libxfce4ui.h>

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

namespace
{
constexpr guint FOLDER_DRAG_ARTIFACT_CLEANUP_DELAY_MS = 5 * 60 * 1000;
}

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
	m_viewport_width(0),
	m_item_dragged(false),
	m_pressed_drag_item(nullptr),
	m_pressed_drag_info(0),
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
	gtk_scrolled_window_set_propagate_natural_width(
			GTK_SCROLLED_WINDOW(m_widget), FALSE);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(m_widget),
			MEOWMENU_LAUNCHER_SHADOW_TYPE);
	gtk_container_add(GTK_CONTAINER(m_widget), m_view->get_widget());
	g_object_ref_sink(m_widget);

	gtk_style_context_add_class(gtk_widget_get_style_context(m_widget), "launchers-pane");
	connect(m_widget, "size-allocate",
			[this](GtkWidget*, GtkAllocation*)
			{
				sync_viewport_width();
			});

	m_empty_message = gtk_label_new(_("No items to show."));
	gtk_widget_set_halign(m_empty_message, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(m_empty_message, GTK_ALIGN_CENTER);
	g_object_ref_sink(m_empty_message);
}

//-----------------------------------------------------------------------------

PlacesPage::~PlacesPage()
{
	clear_drag_state();
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

	connect(m_view->get_widget(), "button-release-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			return on_button_release(reinterpret_cast<GdkEventButton*>(event));
		});

	connect(m_view->get_widget(), "drag-data-get",
		[this](GtkWidget*, GdkDragContext*, GtkSelectionData* data, guint info, guint)
		{
			on_drag_data_get(data, info);
		});

	connect(m_view->get_widget(), "drag-begin",
		[this](GtkWidget*, GdkDragContext* context)
		{
			on_drag_begin(context);
		},
		Connect::After);

	connect(m_view->get_widget(), "drag-end",
		[this](GtkWidget*, GdkDragContext*)
		{
			on_drag_end();
		});

	if (desktop_drag_external_uri_enabled(m_settings->layout_mode))
	{
		const GtkTargetEntry targets[] = {
			{ g_strdup("text/uri-list"), GTK_TARGET_OTHER_APP,
				WHISKERMENU_DESKTOP_DRAG_URI_LIST_INFO },
			{ g_strdup(WHISKERMENU_PLACES_FAVOURITE_DND_TARGET),
				GTK_TARGET_SAME_APP, WHISKERMENU_PLACES_FAVOURITE_DND_INFO }
		};
		m_view->set_drag_source(GDK_BUTTON1_MASK, targets, 2, GDK_ACTION_COPY);
		g_free(targets[0].target);
		g_free(targets[1].target);
	}
	else
	{
		const GtkTargetEntry targets[] = {
			{ g_strdup(WHISKERMENU_PLACES_FAVOURITE_DND_TARGET),
				GTK_TARGET_SAME_APP, WHISKERMENU_PLACES_FAVOURITE_DND_INFO }
		};
		m_view->set_drag_source(GDK_BUTTON1_MASK, targets, 1, GDK_ACTION_COPY);
		g_free(targets[0].target);
	}

	m_view->set_model(GTK_TREE_MODEL(m_model));
	m_view->set_full_redraw_safeguard(full_redraw_safeguard_required(
			m_settings->menu_opacity,
			m_view->is_grid_view()
					? LauncherViewKind::IconGrid
					: LauncherViewKind::Tree,
			m_settings->transparent_grid));
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
	sync_viewport_width();
}

//-----------------------------------------------------------------------------

/* sync_viewport_width:
 *
 * Supplies the Places grid with the same effective Results allocation used by
 * application pages. Toplevel overshoot is removed so explicit grid columns
 * cannot feed their current requisition back into the next allocation.
 */
void PlacesPage::sync_viewport_width()
{
	if (!m_view || !GTK_IS_SCROLLED_WINDOW(m_widget))
		return;
	GtkWidget* toplevel = gtk_widget_get_toplevel(m_widget);
	int requested_width = -1;
	if (GTK_IS_WIDGET(toplevel)
			&& g_strcmp0(m_settings->layout_mode, "fullscreen") != 0)
	{
		gtk_widget_get_size_request(toplevel, &requested_width, nullptr);
	}
	m_viewport_width = meow_grid_effective_viewport_width(
			gtk_widget_get_allocated_width(m_widget),
			GTK_IS_WIDGET(toplevel)
					? gtk_widget_get_allocated_width(toplevel) : 0,
			requested_width);
	m_view->set_viewport_width(m_viewport_width);
}

//-----------------------------------------------------------------------------

/* prepare_viewport_resize:
 * @current_toplevel_width: launcher width before the resize step.
 * @requested_toplevel_width: launcher width requested by the step.
 *
 * Pushes the predicted Results width into a Places icon grid before GTK
 * allocates the resized toplevel. List views and height-only steps are no-ops.
 */
void PlacesPage::prepare_viewport_resize(int current_toplevel_width,
		int requested_toplevel_width)
{
	if (!m_view || m_viewport_width < 1
			|| m_view->get_minimum_viewport_width() < 1)
	{
		return;
	}

	const int viewport_width = meow_grid_resized_viewport_width(
			m_viewport_width, current_toplevel_width,
			requested_toplevel_width);
	if (viewport_width == m_viewport_width)
		return;
	m_viewport_width = viewport_width;
	m_view->set_viewport_width(m_viewport_width);
}

//-----------------------------------------------------------------------------

void PlacesPage::set_interactive_resize(bool active)
{
	if (m_view)
		m_view->set_interactive_resize(active);
}

//-----------------------------------------------------------------------------

int PlacesPage::get_minimum_viewport_width() const
{
	return m_view ? m_view->get_minimum_viewport_width() : 0;
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

/* focus_first_result:
 *
 * Applies the first current Places row as a complete keyboard anchor. The
 * method is intentionally a no-op for an empty asynchronous model, allowing
 * Search to remain the fallback until a current result is delivered.
 *
 * Returns: true when a row was selected, revealed, and focused.
 */
bool PlacesPage::focus_first_result()
{
	GtkTreeModel* model = m_view ? m_view->get_model() : nullptr;
	GtkTreeIter iter;
	if (!model || !gtk_tree_model_get_iter_first(model, &iter))
		return false;

	GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
	m_view->set_cursor(path);
	m_view->select_path(path);
	m_view->scroll_to_path(path);
	gtk_widget_grab_focus(m_view->get_widget());
	gtk_tree_path_free(path);
	return true;
}

void PlacesPage::note_deliberate_navigation()
{
	m_focus_lease.relinquish();
}

void PlacesPage::invalidate_focus_lease()
{
	cancel_home_search();
	m_focus_lease.invalidate();
}

//-----------------------------------------------------------------------------

void PlacesPage::set_active_section(PlacesSection* section)
{
	invalidate_focus_lease();
	clear_home_search_items();
	m_active_section = section;
	rebuild_model();
}

void PlacesPage::refresh_active()
{
	invalidate_focus_lease();
	clear_home_search_items();
	rebuild_model();
}

//-----------------------------------------------------------------------------

/* set_filter:
 *
 * Section-aware dispatcher. For non-Home sections or queries below the
 * 2-character recursive-search threshold (the documented behavior, the documented behavior), keeps the
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
		const std::uint64_t generation = m_focus_lease.begin(
				m_filter, m_active_section,
				m_window && m_window->is_places_active());
		// Clear visible model immediately so stale results don't linger
		// while the new walk is being prepared.
		gtk_list_store_clear(m_model);
		clear_home_search_items();
		gtk_widget_set_visible(m_empty_message, true);

		m_home_search_active = true;
		m_debounce_id = g_timeout_add(150,
				&PlacesPage::on_debounce_fired, this);
		(void)generation;
		return;
	}

	// Non-Home or short filter: fall back to existing visible-items filter.
	cancel_home_search();
	m_focus_lease.invalidate();
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
		on_home_search_done(m_focus_lease.generation());
		return;
	}

	const std::uint64_t generation = m_focus_lease.generation();
	m_home->start_search(m_filter.c_str(), cap,
			[this, generation](PlacesItem* item)
			{
				on_home_search_result(item, generation);
			},
			[this, generation]() { on_home_search_done(generation); });
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
	m_focus_lease.invalidate();
}

void PlacesPage::clear_home_search_items()
{
	for (auto* item : m_home_search_items)
	{
		delete item;
	}
	m_home_search_items.clear();
}

void PlacesPage::on_home_search_result(PlacesItem* item,
		std::uint64_t generation)
{
	if (!item)
	{
		return;
	}
	if (!m_focus_lease.matches(generation, m_filter, m_active_section,
			m_window && m_window->is_places_active()))
	{
		delete item;
		return;
	}
	// Take ownership and append shallow-first into the model. Use the same
	// availability-driven markup/tooltip feed as rebuild_model() so missing
	// home-search results render muted consistently.
	m_home_search_items.push_back(item);
	GtkTreeIter iter;
	gtk_list_store_insert_with_values(
			m_model, &iter, G_MAXINT,
			LauncherView::COLUMN_ICON, item->get_icon(),
			LauncherView::COLUMN_TEXT, item->get_display_markup(),
			LauncherView::COLUMN_TOOLTIP, item->get_tooltip(),
			LauncherView::COLUMN_LAUNCHER, static_cast<Element*>(item),
			-1);
	gtk_widget_set_visible(m_empty_message, false);
	if (m_focus_lease.claim_first(generation, m_filter, m_active_section,
			m_window && m_window->is_places_active()))
	{
		GtkTreePath* path = gtk_tree_model_get_path(
				GTK_TREE_MODEL(m_model), &iter);
		m_view->set_cursor(path);
		m_view->select_path(path);
		m_view->scroll_to_path(path);
		gtk_widget_grab_focus(m_view->get_widget());
		gtk_tree_path_free(path);
	}
}

void PlacesPage::on_home_search_done(std::uint64_t generation)
{
	if (!m_focus_lease.matches(generation, m_filter, m_active_section,
			m_window && m_window->is_places_active()))
		return;
	// Finalize the worker; safe to call from inside the worker's own
	// done callback because HomeSection holds the worker via a
	// raw pointer and HomeSearchWorker keeps its callbacks alive via
	// the Sink shared_ptr until this idle returns.
	if (m_home)
	{
		m_home->cancel_search();
	}
	m_home_search_active = false;
	m_focus_lease.settle_empty(generation, m_filter, m_active_section,
			m_window && m_window->is_places_active());

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
	if (m_item_dragged)
	{
		return;
	}

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
	// Lifetime invariant: dispatch the open BEFORE closing the menu. open() is a
	// synchronous dispatch — once it returns, the launch has been handed off —
	// whereas m_window->hide() clears the search entry, which re-runs the filter
	// and can delete this very item via clear_home_search_items(). Opening first
	// guarantees the item stays valid for the whole open (the documented behavior).
	item->open(screen, m_widget);
	m_window->hide();
}

//-----------------------------------------------------------------------------

void PlacesPage::on_button_press(GdkEventButton* event)
{
	if (!event)
	{
		return;
	}
	m_focus_lease.relinquish();

	GtkTreePath* path = m_view->get_path_at_pos(event->x, event->y);
	if (!path)
	{
		return;
	}

	if (!gdk_event_triggers_context_menu(reinterpret_cast<GdkEvent*>(event))
			&& event->button == GDK_BUTTON_PRIMARY)
	{
		clear_drag_state();
		GtkTreeIter iter;
		Element* element = nullptr;
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(m_model), &iter, path))
		{
			gtk_tree_model_get(GTK_TREE_MODEL(m_model), &iter,
					LauncherView::COLUMN_LAUNCHER, &element, -1);
		}
		m_pressed_drag_item = dynamic_cast<PlacesItem*>(element);
		m_view->set_cursor(path);
		m_view->select_path(path);
		gtk_tree_path_free(path);
		return;
	}

	if (!gdk_event_triggers_context_menu(reinterpret_cast<GdkEvent*>(event)))
	{
		gtk_tree_path_free(path);
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

gboolean PlacesPage::on_button_release(GdkEventButton* event)
{
	if (!event || event->button != GDK_BUTTON_PRIMARY)
	{
		return GDK_EVENT_PROPAGATE;
	}

	if (m_item_dragged)
	{
		clear_drag_state(true);
		return GDK_EVENT_STOP;
	}

	clear_drag_state();
	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

void PlacesPage::on_drag_begin(GdkDragContext* context)
{
	GtkTreePath* path = m_view->get_selected_path();
	if (!path)
	{
		path = m_view->get_cursor();
	}
	m_view->set_drag_icon(path, context, desktop_drag_preview_size());
	if (path)
	{
		gtk_tree_path_free(path);
	}
}

//-----------------------------------------------------------------------------

/* on_drag_data_get:
 * @data: GTK selection data to populate.
 * @info: target info requested by the destination.
 *
 * Exports the pressed Places item URI for same-app favourite drops or external
 * desktop drops. Missing items deliberately export no payload so stale rows
 * cannot be re-added or sent to the desktop.
 */
void PlacesPage::on_drag_data_get(GtkSelectionData* data, guint info)
{
	auto* item = m_pressed_drag_item;
	if (!item)
	{
		return;
	}
	m_pressed_drag_info = info;

	if (info == WHISKERMENU_PLACES_FAVOURITE_DND_INFO)
	{
		if (!item->exists() || xfce_str_is_empty(item->get_uri()))
		{
			return;
		}
		gtk_selection_data_set(data,
				gdk_atom_intern(WHISKERMENU_PLACES_FAVOURITE_DND_TARGET, FALSE),
				8,
				reinterpret_cast<const guchar*>(item->get_uri()),
				strlen(item->get_uri()));
		m_item_dragged = true;
	}
	else if (info == WHISKERMENU_DESKTOP_DRAG_URI_LIST_INFO)
	{
		if (!desktop_drag_places_uri_available(
				m_settings->layout_mode, item->exists(), item->get_uri()))
		{
			return;
		}
		const char* drag_uri = item->get_uri();
		gchar* folder_launcher_uri = nullptr;
		if (item->is_directory())
		{
			if (m_folder_drag_artifact_uri.empty())
			{
				folder_launcher_uri = desktop_drag_create_folder_launcher_uri(
						item->get_file(), item->get_text());
				if (!folder_launcher_uri)
				{
					return;
				}
				m_folder_drag_artifact_uri = folder_launcher_uri;
			}
			if (m_folder_drag_artifact_uri.empty())
			{
				return;
			}
			drag_uri = m_folder_drag_artifact_uri.c_str();
		}

		gchar* uris[2] = { g_strdup(drag_uri), nullptr };
		gtk_selection_data_set_uris(data, uris);
		g_free(uris[0]);
		g_free(folder_launcher_uri);
		m_item_dragged = true;
	}
}

//-----------------------------------------------------------------------------

void PlacesPage::on_drag_end()
{
	clear_drag_state(true);
}

//-----------------------------------------------------------------------------

/* clear_drag_state:
 *
 * Resets transient Places drag state. Folder launcher cleanup is optionally
 * deferred because external destinations may still need to copy the artifact
 * after GTK emits drag-end. It deliberately does not hide the menu; focus loss
 * remains the only close authority after drag completion or cancellation.
 */
void PlacesPage::clear_drag_state(bool defer_folder_cleanup)
{
	m_item_dragged = false;
	m_pressed_drag_item = nullptr;
	m_pressed_drag_info = 0;
	if (defer_folder_cleanup)
	{
		desktop_drag_schedule_folder_launcher_cleanup(
				m_folder_drag_artifact_uri.c_str(),
				FOLDER_DRAG_ARTIFACT_CLEANUP_DELAY_MS);
	}
	else
	{
		desktop_drag_cleanup_folder_launcher_uri(
				m_folder_drag_artifact_uri.c_str());
	}
	m_folder_drag_artifact_uri.clear();
}

//-----------------------------------------------------------------------------

/* show_context_menu:
 *
 * Builds the per-item context menu fresh on every right-click (the documented behavior). Action
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
			// Dispatch before hide — see on_row_activated(): hide() can free the
			// item mid-open, so the open must complete first (the documented behavior).
			item->open(s, m_widget);
			m_window->hide();
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
			// Dispatch before hide — same lifetime invariant as on_row_activated().
			item->open_containing(s, m_widget);
			m_window->hide();
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
				// Dispatch before hide — same lifetime invariant as above.
				item->open_in_terminal(s, m_widget);
				m_window->hide();
			});
		append(mi);

		mi = whiskermenu_image_menu_item_new("list-add", _("Add Desktop Link"));
		connect(mi, "activate",
			[this, item](GtkMenuItem*) { item->add_desktop_link(m_widget); });
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
