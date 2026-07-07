/*
 * Copyright (C) 2026 MeowMenu contributors
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

#ifndef WHISKERMENU_USER_SESSION_RELAYOUT_H
#define WHISKERMENU_USER_SESSION_RELAYOUT_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

/* meow_box_contains_child:
 * @box: a GtkBox that may own @child.
 * @child: the widget whose membership is being tested.
 *
 * Checks actual child membership before callers attempt reorder operations.
 * Parent equality is usually enough, but enumerating the GtkBox child list
 * keeps the guard aligned with the GTK assertion that reorder relies on.
 *
 * Returns: true when @child is currently a child of @box.
 */
bool meow_box_contains_child(GtkBox* box, GtkWidget* child);

/* meow_box_repack_child:
 * @box: target GtkBox that should own @child.
 * @child: widget to move or reinsert.
 * @pack_end: true to pack at the trailing side, false to pack at the leading side.
 * @expand: GtkBox child expand flag.
 * @fill: GtkBox child fill flag.
 * @padding: GtkBox child padding in pixels.
 *
 * Moves @child from its current parent to @box, or repacks it within @box to
 * change leading/trailing ownership. A temporary reference is held across
 * remove/add so the child cannot be destroyed while it is between parents.
 *
 * Returns: true when the child was packed into @box.
 */
bool meow_box_repack_child(GtkBox* box, GtkWidget* child, bool pack_end,
                           bool expand, bool fill, guint padding);

/* meow_box_reorder_child_if_present:
 * @box: target GtkBox.
 * @child: child to reorder.
 * @position: target child position; -1 means last, matching GTK.
 *
 * Reorders only after confirming @child is in @box. Missing children are a
 * no-op because the caller may be handling a live reparenting transition.
 *
 * Returns: true when GTK was asked to reorder the child.
 */
bool meow_box_reorder_child_if_present(GtkBox* box, GtkWidget* child,
                                       gint position);

/* meow_widget_set_visible_if_valid:
 * @widget: widget to show or hide.
 * @visible: target visibility.
 *
 * Applies gtk_widget_set_visible() only to live GtkWidget instances so repeated
 * row relayouts cannot trip GTK visibility assertions on absent children.
 */
void meow_widget_set_visible_if_valid(GtkWidget* widget, bool visible);

/* meow_widget_set_can_focus_if_valid:
 * @widget: widget whose focusability should change.
 * @can_focus: target focusability.
 *
 * Applies gtk_widget_set_can_focus() only to live GtkWidget instances.
 */
void meow_widget_set_can_focus_if_valid(GtkWidget* widget, bool can_focus);

/* meow_widget_set_hexpand_if_valid:
 * @widget: widget whose horizontal expansion should change.
 * @expand: target expansion flag.
 *
 * Applies gtk_widget_set_hexpand() only to live GtkWidget instances.
 */
void meow_widget_set_hexpand_if_valid(GtkWidget* widget, bool expand);

/* meow_widget_set_halign_if_valid:
 * @widget: widget whose horizontal alignment should change.
 * @align: target alignment.
 *
 * Applies gtk_widget_set_halign() only to live GtkWidget instances.
 */
void meow_widget_set_halign_if_valid(GtkWidget* widget, GtkAlign align);

}

#endif // WHISKERMENU_USER_SESSION_RELAYOUT_H
