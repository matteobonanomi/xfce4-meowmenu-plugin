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

/* make_help_button:
 * @accessible_name: translatable name announced by assistive technology. The
 *   revealed description itself is set separately by the caller via
 *   gtk_widget_set_tooltip_text().
 *
 * Returns: a relief-none, vertically-centered "?" GtkButton matching the help
 * affordance used by make_info_frame(). It requests halign = GTK_ALIGN_START so
 * that when attached into a control-only grid cell (which carries hexpand) it
 * renders at its natural compact size instead of stretching across the column.
 * GTK shows its tooltip on both pointer hover and keyboard focus, so the caller
 * need only keep the tooltip text in sync with the active selection. Caller owns
 * the floating ref.
 */
GtkWidget* make_help_button(const gchar* accessible_name);

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

/* COLUMN_C1 / COLUMN_C2:
 * The two equal-width columns of a make_two_column_section() container, used as
 * the @column argument to add_form_row(). C1 is the left half, C2 the right.
 */
enum { COLUMN_C1 = 0, COLUMN_C2 = 1 };

/* make_two_column_section:
 *
 * Builds a section's equal-width two-column container: a single GtkGrid with
 * column_homogeneous = TRUE, so its two columns are always exactly 50/50 and the
 * midpoint coincides across sections of equal content width (the documented behavior). Cells are
 * attached with add_form_row() at a (column, row) coordinate. Because both
 * columns share the grid's rows, a populated C1 cell holds the row height even
 * when the paired C2 cell is empty — so paired controls (e.g. Menu width / Menu
 * height) stay on the same line, and a lone C1 control never crosses the
 * midpoint into an empty C2.
 *
 * Returns: the container grid; caller owns the floating ref.
 */
GtkWidget* make_two_column_section();

/* add_form_row:
 * @grid: a container from make_two_column_section().
 * @column: COLUMN_C1 or COLUMN_C2 — which half to place the cell in.
 * @row: zero-based row index; the same row in the other column is its pair.
 * @label: mnemonic label for @control, or NULL for a control-only cell (e.g. a
 *   check button carrying its own text).
 * @control: the control widget; must not be NULL.
 * @wide: TRUE gives @control hexpand so it fills the rest of its half; FALSE
 *   left-aligns it (halign = START) at natural width with empty trailing space.
 * @label_group: optional GtkSizeGroup joining @label to its peers in the same
 *   column so the label colons line up. Pass NULL to skip. NOTE: aligns label
 *   widths only; the 50/50 split is owned by the homogeneous grid, not this.
 *
 * For a labelled cell, builds an inline [label | control] row (label
 * left-aligned) that hexpands to fill its homogeneous half. For a control-only
 * cell (@label NULL) the control is attached directly into the half.
 */
void add_form_row(GtkWidget* grid, int column, int row, GtkWidget* label,
	GtkWidget* control, bool wide, GtkSizeGroup* label_group);

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
