/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "page.h"

#include "core/window-frame.h"
#include "config/xfce-helpers.h"
#include "core/desktop-drag.h"
#include "launcher/category-button.h"
#include "favorites-page.h"
#include "ui/grid-cell-metrics.h"
#include "ui/image-menu-item.h"
#include "ui/grid-presentation.h"
#include "launcher.h"
#include "launcher-safety.h"
#include "ui/launcher-icon-view.h"
#include "ui/launcher-tree-view.h"
#include "recent-page.h"
#include "settings.h"
#include "ui/slot.h"
#include "core/window.h"
#include "core/window-keyboard.h"

#include <cstring>

#include <libxfce4ui/libxfce4ui.h>

#include <glib/gstdio.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

static Launcher* get_launcher_at_path(LauncherView* view, GtkTreePath* path,
		Element** out_element = nullptr)
{
	if (out_element)
	{
		*out_element = nullptr;
	}
	if (!view || !path)
	{
		return nullptr;
	}

	GtkTreeModel* model = view->get_model();
	if (!model)
	{
		return nullptr;
	}

	GtkTreeIter iter;
	if (!launcher_model_get_iter(model, path, &iter))
	{
		return nullptr;
	}

	Element* element = nullptr;
	gtk_tree_model_get(model, &iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	if (out_element)
	{
		*out_element = element;
	}
	return dynamic_cast<Launcher*>(element);
}

//-----------------------------------------------------------------------------

Page::Page(Settings* settings, Window* window, const gchar* icon, const gchar* text) :
	m_settings(settings),
	m_window(window),
	m_button(nullptr),
	m_selected_launcher(nullptr),
	m_drag_enabled(true),
	m_launcher_dragged(false),
	m_favourite_drag_payload_delivered(false),
	m_reorderable(false),
	m_viewport_width(0)
{
	// Create button
	if (icon && text)
	{
		GIcon* gicon = g_themed_icon_new(icon);
		m_button = new CategoryButton(m_settings, gicon, text);
		g_object_unref(gicon);
	}

	// Create view
	create_view();

	// Add scrolling to view
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
	connect(m_widget, "map",
			[this](GtkWidget*)
			{
				sync_viewport_width();
				GtkWidget* toplevel = gtk_widget_get_toplevel(m_widget);
				if (!meow::meowmenu_queue_complete_window_frame(toplevel))
					m_view->request_content_redraw();
			});
}

//-----------------------------------------------------------------------------

Page::~Page()
{
	delete m_button;
	delete m_view;
	gtk_widget_destroy(m_widget);
	g_object_unref(m_widget);
}

//-----------------------------------------------------------------------------

void Page::reset_selection()
{
	m_view->collapse_all();

	// Set keyboard focus on first item and scroll to top
	select_first();

	// Clear selection
	m_view->clear_selection();
}

//-----------------------------------------------------------------------------

void Page::select_first()
{
	// Select and set keyboard focus on first item
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

	// Scroll to top
	GtkAdjustment* adjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(m_view->get_widget()));
	gtk_adjustment_set_value(adjustment, gtk_adjustment_get_lower(adjustment));
}

/* focus_first_result:
 *
 * Establishes the first current result as the complete keyboard anchor. The
 * cursor, single selection, reveal, and view focus are applied together so a
 * refreshed model cannot leave a stale focus indicator behind.
 *
 * Returns: true when a selectable first row exists.
 */
bool Page::focus_first_result()
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

/* keyboard_move:
 * @direction: physical direction requested by the window dispatcher.
 *
 * Asks the concrete result view for an adjacent displayed item and applies
 * its cursor, selection, reveal, and focus transition as one operation. A
 * missing neighbour is a boundary no-op; model order is never wrapped.
 */
bool Page::keyboard_move(Keyboard::PhysicalDirection direction)
{
	if (!m_view)
		return false;
	GtkTreePath* origin = m_view->get_selected_path();
	if (!origin)
		origin = m_view->get_cursor();
	if (!origin)
		return false;

	GtkTreePath* target = m_view->get_directional_path(origin, direction);
	gtk_tree_path_free(origin);
	if (!target)
		return false;
	const bool moved = m_view->apply_keyboard_target(target);
	gtk_tree_path_free(target);
	return moved;
}

//-----------------------------------------------------------------------------

void Page::update_view()
{
	if ( ((m_settings->view_mode == Settings::ViewAsIcons) && dynamic_cast<LauncherIconView*>(m_view))
			|| ((m_settings->view_mode != Settings::ViewAsIcons) && dynamic_cast<LauncherTreeView*>(m_view)) )
	{
		return;
	}

	g_assert(m_view);
	LauncherView* view = m_view;
	create_view();
	m_view->set_model(view->get_model());
	delete view;

	gtk_container_add(GTK_CONTAINER(m_widget), m_view->get_widget());
	gtk_widget_show_all(m_widget);
	sync_viewport_width();

	view_created();
}

/* sync_viewport_width:
 *
 * Supplies the launcher view with the scroller's allocation after removing any
 * natural-size overshoot GTK has already added beyond the requested toplevel
 * width. Full Screen has no windowed cap, while interactive resize remains
 * authoritative because it updates the toplevel size request.
 */
void Page::sync_viewport_width()
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
 * Pushes the predicted Results width into icon grids before the toplevel size
 * request changes. This breaks the requisition/allocation cycle that otherwise
 * hides results during a drag and delays column changes until button release.
 */
void Page::prepare_viewport_resize(int current_toplevel_width,
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

void Page::set_interactive_resize(bool active)
{
	if (m_view)
		m_view->set_interactive_resize(active);
}

//-----------------------------------------------------------------------------

int Page::get_minimum_viewport_width() const
{
	return m_view ? m_view->get_minimum_viewport_width() : 0;
}

//-----------------------------------------------------------------------------

void Page::set_reorderable(bool reorderable)
{
	m_reorderable = reorderable;
	if (m_reorderable)
	{
		if (desktop_drag_external_uri_enabled(m_settings->layout_mode))
		{
			const GtkTargetEntry row_targets[] = {
				{ g_strdup("GTK_TREE_MODEL_ROW"), GTK_TARGET_SAME_WIDGET, 0 },
				{ g_strdup("text/uri-list"), GTK_TARGET_OTHER_APP,
					WHISKERMENU_DESKTOP_DRAG_URI_LIST_INFO },
				{ g_strdup(WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET),
					GTK_TARGET_SAME_APP, WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO }
			};

			m_view->set_drag_source(GDK_BUTTON1_MASK,
					row_targets, 3,
					GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY));

			m_view->set_drag_dest(row_targets, 1,
					GDK_ACTION_MOVE);

			g_free(row_targets[0].target);
			g_free(row_targets[1].target);
			g_free(row_targets[2].target);
		}
		else
		{
			const GtkTargetEntry row_targets[] = {
				{ g_strdup("GTK_TREE_MODEL_ROW"), GTK_TARGET_SAME_WIDGET, 0 },
				{ g_strdup(WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET),
					GTK_TARGET_SAME_APP, WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO }
			};

			m_view->set_drag_source(GDK_BUTTON1_MASK,
					row_targets, 2,
					GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY));

			m_view->set_drag_dest(row_targets, 1,
					GDK_ACTION_MOVE);

			g_free(row_targets[0].target);
			g_free(row_targets[1].target);
		}
	}
	else
	{
		if (desktop_drag_external_uri_enabled(m_settings->layout_mode))
		{
			const GtkTargetEntry row_targets[] = {
				{ g_strdup("text/uri-list"), GTK_TARGET_OTHER_APP,
					WHISKERMENU_DESKTOP_DRAG_URI_LIST_INFO },
				{ g_strdup(WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET),
					GTK_TARGET_SAME_APP, WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO }
			};

			m_view->set_drag_source(GDK_BUTTON1_MASK,
					row_targets, 2,
					GDK_ACTION_COPY);

			m_view->unset_drag_dest();

			g_free(row_targets[0].target);
			g_free(row_targets[1].target);
		}
		else
		{
			const GtkTargetEntry row_targets[] = {
				{ g_strdup(WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET),
					GTK_TARGET_SAME_APP, WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO }
			};

			m_view->set_drag_source(GDK_BUTTON1_MASK,
					row_targets, 1,
					GDK_ACTION_COPY);

			m_view->unset_drag_dest();

			g_free(row_targets[0].target);
		}
	}
}

//-----------------------------------------------------------------------------

void Page::create_view()
{
	if (m_settings->view_mode == Settings::ViewAsIcons)
	{
		m_view = new LauncherIconView(m_settings);
		connect(m_view->get_widget(), "item-activated",
			[this](GtkIconView*, GtkTreePath* path)
			{
				launcher_activated(path);
			});
	}
	else
	{
		m_view = new LauncherTreeView(m_settings);
		connect(m_view->get_widget(), "row-activated",
			[this](GtkTreeView*, GtkTreePath* path, GtkTreeViewColumn*)
			{
				launcher_activated(path);
			});
	}

	connect(m_view->get_widget(), "button-press-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			return view_button_press_event(event);
		});

	connect(m_view->get_widget(), "button-release-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			return view_button_release_event(event);
		});

	connect(m_view->get_widget(), "drag-data-get",
		[this](GtkWidget*, GdkDragContext*, GtkSelectionData* data, guint info, guint)
		{
			view_drag_data_get(data, info);
		});

	connect(m_view->get_widget(), "drag-begin",
		[this](GtkWidget*, GdkDragContext* context)
		{
			view_drag_begin(context);
		},
		Connect::After);

	connect(m_view->get_widget(), "drag-end",
		[this](GtkWidget*, GdkDragContext*)
		{
			view_drag_end();
		});

	connect(m_view->get_widget(), "popup-menu",
		[this](GtkWidget*) -> gboolean
		{
			return view_popup_menu_event();
		});

	set_reorderable(m_reorderable);

	// Transparent grid is redraw-sensitive even when the surrounding menu shell
	// is fully opaque. The owner resolves that policy while the shared view stays
	// independent of Xfconf.
	m_view->set_full_redraw_safeguard(full_redraw_safeguard_required(
			m_settings->menu_opacity,
			m_view->is_grid_view()
					? LauncherViewKind::IconGrid
					: LauncherViewKind::Tree,
			m_settings->transparent_grid));
}

//-----------------------------------------------------------------------------

bool Page::remember_launcher(Launcher*)
{
	return true;
}

//-----------------------------------------------------------------------------

bool Page::activate_first()
{
	GtkTreeModel* model = m_view ? m_view->get_model() : nullptr;
	if (!model)
	{
		return false;
	}
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(model, &iter))
	{
		return false;
	}
	GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
	launcher_activated(path);
	gtk_tree_path_free(path);
	return true;
}

//-----------------------------------------------------------------------------

void Page::launcher_activated(GtkTreePath* path)
{
	// NOTE: 250 ms debounce against held-Enter key-repeat bursts (the documented behavior,
	// the documented behavior). The state is process-global across pages because a single
	// user keypress can only target one launcher at a time and a stuck
	// key must not multi-launch.
	static Keyboard::ActivationDebounce s_debounce;
	if (!s_debounce.accept(g_get_monotonic_time()))
	{
		return;
	}

	// Find element
	Element* element = nullptr;
	get_launcher_at_path(m_view, path, &element);
	if (!element)
	{
		return;
	}

	// Add to recent and record frecency stats
	if (Launcher* launcher = dynamic_cast<Launcher*>(element))
	{
		if (remember_launcher(launcher))
		{
			m_window->get_recent()->add(launcher);
		}
		m_settings->usage_stats.record_launch(launcher->get_desktop_id());
	}

	// Hide window
	m_window->hide();

	// Execute app
	element->run(gtk_widget_get_screen(m_widget));
}

//-----------------------------------------------------------------------------

void Page::launcher_action_activated(GtkMenuItem* menuitem, DesktopAction* action)
{
	g_assert(m_selected_launcher);

	// Add to recent
	if (remember_launcher(m_selected_launcher))
	{
		m_window->get_recent()->add(m_selected_launcher);
	}

	// Hide window
	m_window->hide();

	// Execute app
	m_selected_launcher->run(gtk_widget_get_screen(GTK_WIDGET(menuitem)), action);
}

//-----------------------------------------------------------------------------

gboolean Page::view_button_press_event(GdkEvent* event)
{
	GdkEventButton* button_event = reinterpret_cast<GdkEventButton*>(event);

	m_launcher_dragged = false;

	GtkTreePath* path = m_view->get_path_at_pos(button_event->x, button_event->y);
	if (!path)
	{
		return GDK_EVENT_PROPAGATE;
	}

	if (gdk_event_triggers_context_menu(event))
	{
		create_context_menu(path, event);
		return GDK_EVENT_STOP;
	}
	else if (button_event->button != GDK_BUTTON_PRIMARY)
	{
		gtk_tree_path_free(path);
		return GDK_EVENT_PROPAGATE;
	}

	Element* element = nullptr;
	get_launcher_at_path(m_view, path, &element);
	gtk_tree_path_free(path);
	if (!(m_selected_launcher = dynamic_cast<Launcher*>(element)))
	{
		m_drag_enabled = false;
		m_view->unset_drag_source();
		m_view->unset_drag_dest();
	}
	else if (!m_drag_enabled)
	{
		m_drag_enabled = true;
		set_reorderable(m_reorderable);
	}

	m_window->set_child_has_focus();

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

gboolean Page::view_button_release_event(GdkEvent* event)
{
	GdkEventButton* button_event = reinterpret_cast<GdkEventButton*>(event);
	if (button_event->button != 1)
	{
		return GDK_EVENT_PROPAGATE;
	}

	if (m_launcher_dragged)
	{
		m_launcher_dragged = false;
		m_favourite_drag_payload_delivered = false;
	}

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

void Page::view_drag_begin(GdkDragContext* context)
{
	m_favourite_drag_payload_delivered = false;

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

void Page::view_drag_data_get(GtkSelectionData* data, guint info)
{
	if (!m_selected_launcher)
	{
		return;
	}

	bool payload_delivered = false;
	if (info == WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO)
	{
		const gchar* desktop_id = m_selected_launcher->get_desktop_id();
		if (!xfce_str_is_empty(desktop_id))
		{
			gtk_selection_data_set(data,
					gdk_atom_intern(WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET, FALSE),
					8,
					reinterpret_cast<const guchar*>(desktop_id),
					strlen(desktop_id));
			m_favourite_drag_payload_delivered = true;
			payload_delivered = true;
		}
	}
	else if (info == WHISKERMENU_DESKTOP_DRAG_URI_LIST_INFO)
	{
		gchar* uris[2] = { m_selected_launcher->get_uri(), nullptr };
		if (desktop_drag_application_uri_available(
				m_settings->layout_mode, uris[0]))
		{
			gtk_selection_data_set_uris(data, uris);
			payload_delivered = true;
		}
		g_free(uris[0]);
	}
	else
	{
		return;
	}

	if (payload_delivered)
	{
		m_launcher_dragged = true;
	}
}

//-----------------------------------------------------------------------------

void Page::view_drag_end()
{
	if (m_launcher_dragged)
	{
		m_launcher_dragged = false;
		m_favourite_drag_payload_delivered = false;
	}
}

//-----------------------------------------------------------------------------

gboolean Page::view_popup_menu_event()
{
	GtkTreePath* path = m_view->get_cursor();
	if (!path)
	{
		return GDK_EVENT_PROPAGATE;
	}

	create_context_menu(path, nullptr);

	return GDK_EVENT_STOP;
}

//-----------------------------------------------------------------------------

void Page::create_context_menu(GtkTreePath* path, GdkEvent* event)
{
	// Get selected launcher
	if (!(m_selected_launcher = get_launcher_at_path(m_view, path)))
	{
		gtk_tree_path_free(path);
		return;
	}

	// Create context menu
	GtkWidget* menu = gtk_menu_new();
	connect(menu, "selection-done",
		[this](GtkMenuShell* menushell)
		{
			m_selected_launcher = nullptr;
			gtk_widget_destroy(GTK_WIDGET(menushell));
		});

	// Add menu items
	GtkWidget* menuitem = gtk_menu_item_new_with_label(m_selected_launcher->get_display_name());
	gtk_widget_set_sensitive(menuitem, false);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	const auto actions = m_selected_launcher->get_actions();
	if (!actions.empty())
	{
		for (auto action : actions)
		{
			menuitem = whiskermenu_image_menu_item_new(action->get_icon(), action->get_name());
			connect(menuitem, "activate",
				[this, action](GtkMenuItem* action_menuitem)
				{
					launcher_action_activated(action_menuitem, action);
				});
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		}

		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	if (!m_window->get_favorites()->contains(m_selected_launcher))
	{
		menuitem = whiskermenu_image_menu_item_new("bookmark-new", _("Add to Favorites"));
		connect(menuitem, "activate",
			[this](GtkMenuItem*)
			{
				g_assert(m_selected_launcher);
				m_window->get_favorites()->add(m_selected_launcher);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	else
	{
		menuitem = whiskermenu_image_menu_item_new("list-remove", _("Remove from Favorites"));
		connect(menuitem, "activate",
			[this](GtkMenuItem*)
			{
				g_assert(m_selected_launcher);
				m_window->get_favorites()->remove(m_selected_launcher);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	menuitem = whiskermenu_image_menu_item_new("list-add", _("Add to Desktop"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			add_selected_to_desktop();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("list-add", _("Add to Panel"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			add_selected_to_panel();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	if (!m_selected_launcher->has_auto_start())
	{
		menuitem = whiskermenu_image_menu_item_new("list-add", _("Add to Autostart"));
		connect(menuitem, "activate",
			[this](GtkMenuItem*)
			{
				g_assert(m_selected_launcher);
				m_selected_launcher->set_auto_start(true);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	else
	{
		menuitem = whiskermenu_image_menu_item_new("list-remove", _("Remove from Autostart"));
		connect(menuitem, "activate",
			[this](GtkMenuItem*)
			{
				g_assert(m_selected_launcher);
				m_selected_launcher->set_auto_start(false);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("gtk-edit", _("Edit Application..."));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			edit_selected();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("edit-delete", _("Hide Application"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			g_assert(m_selected_launcher);
			m_window->hide();
			m_selected_launcher->hide();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	extend_context_menu(menu);

	gtk_widget_show_all(menu);

	// Show context menu
	m_window->set_child_has_focus();
	gtk_menu_attach_to_widget(GTK_MENU(menu), m_view->get_widget(), nullptr);
	gtk_menu_popup_at_pointer(GTK_MENU(menu), event);

	// Keep selection
	m_view->select_path(path);
	gtk_tree_path_free(path);
}

//-----------------------------------------------------------------------------

void Page::extend_context_menu(GtkWidget*)
{
}

//-----------------------------------------------------------------------------

void Page::add_selected_to_desktop()
{
	// Fetch desktop folder
	const gchar* desktop_path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
	GFile* desktop_folder = g_file_new_for_path(desktop_path);

	// Fetch launcher source
	g_assert(m_selected_launcher);
	GFile* source_file = m_selected_launcher->get_file();

	// Fetch launcher destination
	gchar* basename = g_file_get_basename(source_file);
	GFile* destination_file = g_file_get_child(desktop_folder, basename);
	g_free(basename);

	// Copy launcher to desktop folder
	GError* error = nullptr;
	if (g_file_copy(source_file, destination_file, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &error))
	{
		// Make launcher executable
		gchar* path = g_file_get_path(destination_file);
		g_chmod(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		g_free(path);

#if LIBXFCE4UTIL_CHECK_VERSION(4,17,0)
		// Make launcher trusted
		xfce_g_file_set_trusted(destination_file, true, nullptr, nullptr);
#endif
	}
	else
	{
		xfce_dialog_show_error(nullptr, error, _("Unable to add launcher to desktop."));
		g_error_free(error);
	}

	g_object_unref(destination_file);
	g_object_unref(source_file);
	g_object_unref(desktop_folder);
}

//-----------------------------------------------------------------------------

void Page::add_selected_to_panel()
{
	// Connect to Xfce panel through D-Bus
	GError* error = nullptr;
	GDBusProxy* proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_NONE,
			nullptr,
			"org.xfce.Panel",
			"/org/xfce/Panel",
			"org.xfce.Panel",
			nullptr,
			&error);
	if (proxy)
	{
		// Fetch launcher desktop ID
		g_assert(m_selected_launcher);
		const gchar* parameters[] = { m_selected_launcher->get_desktop_id(), nullptr };

		// Tell panel to add item
		GVariant* result = g_dbus_proxy_call_sync(proxy,
				"AddNewItem",
				g_variant_new("(s^as)", "launcher", parameters),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				nullptr,
				&error);

		if (!result)
		{
			xfce_dialog_show_error(nullptr, error, _("Unable to add launcher to panel."));
			g_error_free(error);
		}
		else
		{
			g_variant_unref(result);
		}

		// Disconnect from D-Bus
		g_object_unref(proxy);
	}
	else
	{
		xfce_dialog_show_error(nullptr, error, _("Unable to add launcher to panel."));
		g_error_free(error);
	}
}

//-----------------------------------------------------------------------------

void Page::edit_selected()
{
	g_assert(m_selected_launcher);

	m_window->hide();

	gchar* uri = m_selected_launcher->get_uri();
	if (!uri)
	{
		return;
	}
	const gchar* editor = xfce_desktop_item_editor(
			current_xfce_dependency_regime());
	gchar** argv = launcher_editor_argv(editor, uri);
	g_free(uri);
	if (!argv)
	{
		return;
	}

	GError* error = nullptr;
	if (!g_spawn_async(nullptr, argv, nullptr, G_SPAWN_SEARCH_PATH,
			nullptr, nullptr, nullptr, &error))
	{
		xfce_dialog_show_error(nullptr, error, _("Unable to edit launcher."));
		g_error_free(error);
	}
	g_strfreev(argv);
}

//-----------------------------------------------------------------------------
