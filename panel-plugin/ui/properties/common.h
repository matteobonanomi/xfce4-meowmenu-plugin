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

#ifndef WHISKERMENU_UI_PROPERTIES_COMMON_H
#define WHISKERMENU_UI_PROPERTIES_COMMON_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

/* make_aligned_frame:
 * @text: bold frame title; markup is escaped.
 * @content: child widget; ownership passes to the returned frame.
 *
 * Returns: a GtkFrame with a bold title label and a 12px start /
 * 6px top margin applied to @content. Caller owns the floating widget.
 */
GtkWidget* make_aligned_frame(const gchar* text, GtkWidget* content);

/* make_info_frame:
 * @title: bold frame title; markup is escaped.
 * @content: child widget; ownership passes to the returned frame.
 * @info_text: text shown by the "?" popover next to the title.
 *
 * Returns: a GtkFrame whose title row carries a "?" button revealing a
 * line-wrapped explanatory popover. Caller owns the floating widget.
 */
GtkWidget* make_info_frame(const gchar* title, GtkWidget* content, const gchar* info_text);

/* wrap_in_scrolled:
 * @content: widget hierarchy to wrap; must not be NULL.
 *
 * Wraps a Properties-tab content widget in a GtkScrolledWindow with
 * vertical-only scrolling: horizontal policy is NEVER, vertical is
 * AUTOMATIC, and natural height is propagated so short tabs do not
 * waste vertical space.
 *
 * Returns: a new floating GtkWidget owned by the caller's container.
 */
GtkWidget* wrap_in_scrolled(GtkWidget* content);

/* make_form_switch:
 *
 * Returns: a floating, inactive GtkSwitch requesting
 * halign = GTK_ALIGN_START and valign = GTK_ALIGN_CENTER, so that when it
 * is grid-attached into a column shared with a wider control (combo box,
 * spin button) it renders at its natural compact width instead of stretching
 * to fill the column. The caller owns the floating ref, sets the active state
 * via gtk_switch_set_active(), and wires every signal. The factory touches no
 * Xfconf key and no Settings field — toggle behaviour and dependent-control
 * greying stay entirely the caller's responsibility.
 */
GtkWidget* make_form_switch();

}

#endif // WHISKERMENU_UI_PROPERTIES_COMMON_H
