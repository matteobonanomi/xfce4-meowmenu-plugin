/*
 * Copyright (C) 2013-2025 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_LAUNCHER_VIEW_H
#define WHISKERMENU_LAUNCHER_VIEW_H

#include "slot.h"
#include "core/window-keyboard.h"

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class LauncherView
{
public:
	virtual ~LauncherView() = default;

	LauncherView(const LauncherView&) = delete;
	LauncherView(LauncherView&&) = delete;
	LauncherView& operator=(const LauncherView&) = delete;
	LauncherView& operator=(LauncherView&&) = delete;

	virtual GtkWidget* get_widget() const=0;

	virtual GtkTreePath* get_cursor() const=0;
	virtual GtkTreePath* get_path_at_pos(int x, int y) const=0;
	virtual GtkTreePath* get_selected_path() const=0;
	virtual bool is_path_selected(GtkTreePath* path) const=0;
	virtual void activate_path(GtkTreePath* path)=0;
	virtual void scroll_to_path(GtkTreePath* path)=0;
	virtual void select_path(GtkTreePath* path)=0;
	virtual void set_cursor(GtkTreePath* path)=0;
	virtual bool is_first_visual_row(GtkTreePath* path) const=0;
	virtual GtkTreePath* get_directional_path(GtkTreePath*,
			Keyboard::PhysicalDirection) const
	{
		return nullptr;
	}
	virtual bool get_path_rectangle(GtkTreePath*,
			Keyboard::NavigationRect*) const
	{
		return false;
	}
	virtual bool apply_keyboard_target(GtkTreePath*)
	{
		return false;
	}

	virtual void set_fixed_height_mode(bool fixed_height)=0;
	virtual void set_selection_mode(GtkSelectionMode mode)=0;

	virtual void hide_tooltips()=0;
	virtual void show_tooltips()=0;

	virtual void clear_selection()=0;
	virtual void collapse_all()=0;

	GtkTreeModel* get_model() const
	{
		return m_model;
	}

	virtual void set_model(GtkTreeModel* model)=0;
	virtual void unset_model()=0;

	virtual void set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)=0;
	virtual void set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)=0;
	virtual void unset_drag_source()=0;
	virtual void unset_drag_dest()=0;

	/* set_drag_icon:
	 * @path: source model row currently being dragged.
	 * @context: GTK drag context for the active drag operation.
	 * @icon_size: requested preview icon size in pixels.
	 *
	 * Renders the row icon at a caller-chosen size and installs it as the GTK
	 * drag preview. Falls back to GTK's GIcon preview if the theme lookup fails.
	 */
	void set_drag_icon(GtkTreePath* path, GdkDragContext* context, int icon_size) const
	{
		if (!path || !context || !m_model || icon_size <= 0)
		{
			return;
		}

		GtkTreeIter iter;
		if (!gtk_tree_model_get_iter(m_model, &iter, path))
		{
			return;
		}

		GIcon* icon = nullptr;
		gtk_tree_model_get(m_model, &iter, COLUMN_ICON, &icon, -1);
		if (!icon)
		{
			return;
		}

		GtkIconTheme* theme = gtk_icon_theme_get_for_screen(
				gtk_widget_get_screen(get_widget()));
		GtkIconInfo* info = gtk_icon_theme_lookup_by_gicon(
				theme, icon, icon_size, GTK_ICON_LOOKUP_FORCE_SIZE);
		GdkPixbuf* pixbuf = info ? gtk_icon_info_load_icon(info, nullptr) : nullptr;
		if (pixbuf)
		{
			gtk_drag_set_icon_pixbuf(context, pixbuf, icon_size / 2, icon_size / 2);
			g_object_unref(pixbuf);
		}
		else
		{
			gtk_drag_set_icon_gicon(context, icon, icon_size / 2, icon_size / 2);
		}
		if (info)
		{
			g_object_unref(info);
		}
		g_object_unref(icon);
	}

	virtual void reload_icon_size()=0;
	virtual void set_viewport_width(int)
	{
	}
	virtual int get_minimum_viewport_width() const { return 0; }

	virtual int get_item_height() const { return 32; }
	virtual int get_icon_size() const { return 0; }
	virtual bool is_grid_view() const { return false; }

	/* set_full_redraw_safeguard:
	 * @enabled: whether this result surface requires full-view redraws.
	 *
	 * Pushed by the owner from the resolved opacity and view-style policy. The
	 * base remains settings-agnostic.
	 */
	void set_full_redraw_safeguard(bool enabled)
	{
		m_full_redraw_safeguard = enabled;
	}

	enum Columns
	{
		COLUMN_ICON = 0,
		COLUMN_TEXT,
		COLUMN_TOOLTIP,
		COLUMN_LAUNCHER,
		N_COLUMNS
	};

protected:
	LauncherView() = default;

	void enable_hover_selection(GtkWidget* view)
	{
		gtk_widget_add_events(GTK_WIDGET(view), GDK_SCROLL_MASK);

		connect(view, "leave-notify-event",
			[this](GtkWidget*, GdkEvent*) -> gboolean
			{
				// NOTE: clears the hover-owned highlight so no row stays
				// highlighted once the pointer leaves the list (the documented behavior). The
				// selection is the only painted highlight (pointer prelight is
				// neutralised in the plugin CSS), so unselecting here leaves
				// zero highlights from hover.
				clear_selection();
				queue_full_redraw_safeguard();
				return GDK_EVENT_PROPAGATE;
			});

		connect(view, "motion-notify-event",
			[this](GtkWidget*, GdkEvent* event) -> gboolean
			{
				GdkEventMotion* motion_event = reinterpret_cast<GdkEventMotion*>(event);
				select_path_at_pos(motion_event->x, motion_event->y);
				return GDK_EVENT_PROPAGATE;
			});

		connect(view, "scroll-event",
			[this](GtkWidget*, GdkEvent* event) -> gboolean
			{
				GdkEventScroll* scroll_event = reinterpret_cast<GdkEventScroll*>(event);
				select_path_at_pos(scroll_event->x, scroll_event->y);
				return GDK_EVENT_PROPAGATE;
			});

		// Scroll-reveal safeguard. The discrete scroll-event above only fires for
		// the mouse wheel; a keyboard Page Up/Down, a scrollbar drag, or kinetic
		// scrolling moves the viewport without it. The view is a GtkScrollable, so
		// when it is placed in its scrolled window the scrolled window installs its
		// own vertical adjustment (notify::vadjustment fires once). Connect that
		// adjustment's value-changed to the guarded redraw so a pure scroll
		// that reveals rows — with no selection change — still recomposites the
		// whole surface (the documented behavior's "newly revealed rows", the 041 symptom).
		// Opaque list/tree surfaces leave the guard disabled.
		//
		// LIFECYCLE: this is the ONE safeguard connection made on an object the
		// view does not own. The vertical adjustment belongs to the scrolled
		// window, which is part of the results panel and is reused across menu
		// rebuilds, so it outlives this LauncherView. Every other safeguard
		// connection is on the view widget and is torn down with it; this one must
		// be unbound explicitly, or a later value-changed would invoke
		// queue_full_redraw_safeguard() → get_widget() on a freed view
		// (use-after-free, the documented behavior). We therefore track the current adjustment and
		// our handler id, drop a stale handler whenever GTK swaps the adjustment
		// (so at most one is ever live), and disconnect on the view widget's own
		// "destroy" (below) so the handler can never fire after the view is gone.
		connect(view, "notify::vadjustment",
			[this](GtkWidget* w, GParamSpec*)
			{
				GtkAdjustment* adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(w));
				disconnect_scroll_safeguard();
				if (adj)
				{
					m_scroll_adjustment = adj;
					m_scroll_handler_id = connect(adj, "value-changed",
						[this](GtkAdjustment*)
						{
							queue_full_redraw_safeguard();
						});
				}
			});

		// Bind the external-adjustment connection above to the view widget's
		// lifetime. "destroy" is emitted on the view (during its concrete
		// destructor's gtk_widget_destroy, while this LauncherView is still alive),
		// so its Slot is torn down normally and the disconnect runs at exactly the
		// right moment — before the view is freed, covering the the documented behavior teardown.
		connect(view, "destroy",
			[this](GtkWidget*)
			{
				disconnect_scroll_safeguard();
			});
	}

	/* queue_full_redraw_safeguard:
	 *
	 * Queues a full-widget redraw only for a surface whose presentation policy
	 * requires it. GTK coalesces overlapping requests, so navigation and scroll
	 * triggers still cost at most one redraw per frame.
	 */
	void queue_full_redraw_safeguard()
	{
		if (m_full_redraw_safeguard)
			gtk_widget_queue_draw(get_widget());
	}

	GtkTreeModel* m_model = nullptr;

private:
	/* disconnect_scroll_safeguard:
	 *
	 * Disconnects the value-changed handler tracked on the scrolled window's
	 * vertical adjustment (if any) and clears the tracking members. Idempotent:
	 * safe to call when nothing is tracked. Invoked when GTK swaps the adjustment
	 * (so only one handler is ever live) and from the view widget's "destroy" (so
	 * the handler never outlives the view it would refresh — the documented behavior).
	 */
	void disconnect_scroll_safeguard()
	{
		if (m_scroll_adjustment && m_scroll_handler_id != 0)
		{
			g_signal_handler_disconnect(m_scroll_adjustment, m_scroll_handler_id);
		}
		m_scroll_adjustment = nullptr;
		m_scroll_handler_id = 0;
	}

	// Pushed by the owner after resolving opacity and view styling. Default false
	// so ordinary opaque list/tree views pay nothing.
	bool m_full_redraw_safeguard = false;

	// The scrolled window's vertical adjustment we hold a value-changed handler
	// on, plus that handler's id. The adjustment is not owned by this view and
	// outlives it, so the pair lets us disconnect the handler on adjustment swap
	// and on view destroy (the documented behavior). Both stay null/0 while nothing is tracked.
	GtkAdjustment* m_scroll_adjustment = nullptr;
	gulong m_scroll_handler_id = 0;

	/* select_path_at_pos:
	 * @x: pointer x in widget coordinates.
	 * @y: pointer y in widget coordinates.
	 *
	 * Makes the row under a *moving* pointer the single selection (the sole
	 * painted highlight). When the pointer is over no row the selection is
	 * cleared. When it is already over the selected row this early-returns,
	 * which is safe because the hover path always calls set_cursor() AND
	 * select_path() together: cursor and selection therefore stay coincident,
	 * so a subsequent arrow press moves relative to the hovered row rather than
	 * jumping back to a stale keyboard position (the documented behavior, the documented behavior). Called only
	 * from motion/scroll, so a motionless pointer never re-selects (the documented behavior).
	 */
	void select_path_at_pos(int x, int y)
	{
		GtkTreePath* path = get_path_at_pos(x, y);
		if (!path)
		{
			clear_selection();
		}
		else if (!is_path_selected(path))
		{
			// Keep cursor == selection: set_cursor moves the GTK cursor
			// without selecting, select_path then makes that same row the
			// single selection.
			set_cursor(path);
			select_path(path);
		}
		gtk_tree_path_free(path);
		// Recomposite the whole surface on every pointer move/wheel while
		// guarded, so a sweep or wheel that changes (or clears) the hovered row
		// leaves no trailing highlight; a no-op on ordinary opaque surfaces.
		queue_full_redraw_safeguard();
	}
};

}

#endif // WHISKERMENU_LAUNCHER_VIEW_H
