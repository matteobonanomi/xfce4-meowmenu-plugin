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

	virtual void reload_icon_size()=0;

	/* set_background_translucent:
	 * @translucent: whether the menu background is currently see-through
	 *               (alpha < 1, i.e. menu opacity below 100).
	 *
	 * Pushed by the view owner from the resolved /menu-opacity value; the base
	 * never reads Xfconf, so it stays settings-agnostic. While true, navigation
	 * recomposites the whole view (see queue_translucent_safeguard_redraw); while
	 * false the safeguard is a no-op, so the fully-opaque path is unchanged.
	 */
	void set_background_translucent(bool translucent)
	{
		m_background_translucent = translucent;
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
				// highlighted once the pointer leaves the list (FR-004). The
				// selection is the only painted highlight (pointer prelight is
				// neutralised in the plugin CSS), so unselecting here leaves
				// zero highlights from hover.
				clear_selection();
				queue_translucent_safeguard_redraw();
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
		// adjustment's value-changed to the translucent redraw so a pure scroll
		// that reveals rows — with no selection change — still recomposites the
		// whole surface (FR-007's "newly revealed rows", the 041 symptom). Gated by
		// the translucent flag, so a no-op at opacity 100.
		connect(view, "notify::vadjustment",
			[this](GtkWidget* w, GParamSpec*)
			{
				GtkAdjustment* adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(w));
				if (adj)
					connect(adj, "value-changed",
						[this](GtkAdjustment*)
						{
							queue_translucent_safeguard_redraw();
						});
			});
	}

	/* queue_translucent_safeguard_redraw:
	 *
	 * Queues a full-widget redraw ONLY while the background is translucent. Hover,
	 * wheel, and keyboard navigation all resolve to a single selection change and
	 * pointer prelight is CSS-neutralised, so this one hook — invoked from the
	 * shared selection chokepoint here and from the scroll/selection signals each
	 * concrete view wires up — covers every navigation modality. The redraw forces
	 * the whole translucent surface to recomposite so no stale highlight pixels
	 * survive on visited or newly revealed rows. At opacity 100 the flag is false
	 * and this is a no-op, so the fully-opaque path is byte-for-byte unchanged.
	 * gtk_widget_queue_draw coalesces, so overlapping triggers cost at most one
	 * redraw per frame.
	 */
	void queue_translucent_safeguard_redraw()
	{
		if (m_background_translucent)
			gtk_widget_queue_draw(get_widget());
	}

	GtkTreeModel* m_model = nullptr;

private:
	// Whether the menu background is translucent (alpha < 1). Pushed by the view
	// owner via set_background_translucent(); gates the safeguard redraw so the
	// fully-opaque path pays nothing. Default false until the owner pushes a value.
	bool m_background_translucent = false;

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
	 * jumping back to a stale keyboard position (FR-003, FR-005). Called only
	 * from motion/scroll, so a motionless pointer never re-selects (FR-005).
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
		// translucent, so a sweep or wheel that changes (or clears) the hovered
		// row leaves no trailing highlight; a no-op when opaque.
		queue_translucent_safeguard_redraw();
	}
};

}

#endif // WHISKERMENU_LAUNCHER_VIEW_H
