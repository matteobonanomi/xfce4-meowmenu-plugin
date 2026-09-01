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

#ifndef WHISKERMENU_PAGE_H
#define WHISKERMENU_PAGE_H

#include <gtk/gtk.h>

#include "core/window-keyboard.h"

namespace WhiskerMenu
{

static const char WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET[] =
		"application/x-meowmenu-application-favourite";
static const guint WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO = 2;
constexpr GtkShadowType MEOWMENU_LAUNCHER_SHADOW_TYPE = GTK_SHADOW_NONE;

class CategoryButton;
class DesktopAction;
class Launcher;
class LauncherView;
class Settings;
class Window;

class Page
{
public:
	Page(Settings* settings, Window* window, const gchar* icon, const gchar* text);
	virtual ~Page();

	Page(const Page&) = delete;
	Page(Page&&) = delete;
	Page& operator=(const Page&) = delete;
	Page& operator=(Page&&) = delete;

	GtkWidget* get_widget() const
	{
		return m_widget;
	}

	CategoryButton* get_button() const
	{
		return m_button;
	}

	LauncherView* get_view() const
	{
		return m_view;
	}

	void reset_selection();
	void select_first();
	bool focus_first_result();
	bool keyboard_move(Keyboard::PhysicalDirection direction);
	void update_view();
	void present();

	/* prepare_viewport_resize:
	 * @current_toplevel_width: launcher width before the resize step.
	 * @requested_toplevel_width: launcher width requested by the step.
	 *
	 * Prepares an icon grid for a live horizontal resize before GTK allocates
	 * the new window. List views and height-only steps are left unchanged.
	 */
	void prepare_viewport_resize(int current_toplevel_width,
			int requested_toplevel_width);

	int get_viewport_width() const { return m_viewport_width; }
	int get_minimum_viewport_width() const;

	/* activate_first:
	 *
	 * Launches the launcher in the first row of the current view, if
	 * the model has at least one row. Subject to the same activation
	 * debounce as a mouse/key activation, so holding Enter on the
	 * search entry cannot launch the same application twice. Used by
	 * the search entry's "activate" handler to satisfy the documented behavior.
	 *
	 * Returns: true iff a row existed and was activated.
	 */
	bool activate_first();

protected:
	Window* get_window() const
	{
		return m_window;
	}

	virtual void view_created()
	{
	}

	void set_reorderable(bool reorderable);

	Launcher* get_selected_launcher() const
	{
		return m_selected_launcher;
	}

protected:
	Settings* const m_settings;

private:
	void create_view();
	void sync_viewport_width();
	virtual bool remember_launcher(Launcher* launcher);
	void launcher_activated(GtkTreePath* path);
	void launcher_action_activated(GtkMenuItem* menuitem, DesktopAction* action);
	gboolean view_button_press_event(GdkEvent* event);
	gboolean view_button_release_event(GdkEvent* event);
	void view_drag_begin(GdkDragContext* context);
	void view_drag_data_get(GtkSelectionData* data, guint info);
	void view_drag_end();
	gboolean view_popup_menu_event();
	void add_selected_to_desktop();
	void add_selected_to_panel();
	void edit_selected();
	void create_context_menu(GtkTreePath* path, GdkEvent* event);
	virtual void extend_context_menu(GtkWidget* menu);

private:
	Window* m_window;
	CategoryButton* m_button;
	GtkWidget* m_widget;
	LauncherView* m_view;
	Launcher* m_selected_launcher;
	bool m_drag_enabled;
	bool m_launcher_dragged;
	bool m_favourite_drag_payload_delivered;
	bool m_reorderable;
	int m_viewport_width;
	guint m_present_tick_id;
};

}

#endif // WHISKERMENU_PAGE_H
