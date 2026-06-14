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

#include "common.h"

namespace WhiskerMenu
{

GtkWidget* make_aligned_frame(const gchar* text, GtkWidget* content)
{
	gchar* markup = g_markup_printf_escaped("<b>%s</b>", text);
	GtkWidget* label = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);

	GtkWidget* frame = gtk_frame_new(nullptr);
	gtk_frame_set_label_widget(GTK_FRAME(frame), label);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	gtk_widget_set_margin_start(content, 12);
	gtk_widget_set_margin_top(content, 6);
	gtk_container_add(GTK_CONTAINER(frame), content);

	return frame;
}

GtkWidget* make_info_frame(const gchar* title, GtkWidget* content, const gchar* info_text)
{
	// Header box: [<b>title</b>] [?]
	GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

	gchar* markup = g_markup_printf_escaped("<b>%s</b>", title);
	GtkWidget* title_label = gtk_label_new(nullptr);
	gtk_label_set_markup(GTK_LABEL(title_label), markup);
	g_free(markup);
	gtk_box_pack_start(GTK_BOX(header_box), title_label, false, false, 0);

	GtkWidget* info_btn = gtk_button_new_with_label("?");
	gtk_button_set_relief(GTK_BUTTON(info_btn), GTK_RELIEF_NONE);
	gtk_widget_set_valign(info_btn, GTK_ALIGN_CENTER);

	GtkWidget* popover = gtk_popover_new(info_btn);
	GtkWidget* pop_label = gtk_label_new(info_text);
	gtk_label_set_line_wrap(GTK_LABEL(pop_label), true);
	gtk_label_set_max_width_chars(GTK_LABEL(pop_label), 45);
	gtk_widget_set_margin_start(pop_label, 8);
	gtk_widget_set_margin_end(pop_label, 8);
	gtk_widget_set_margin_top(pop_label, 8);
	gtk_widget_set_margin_bottom(pop_label, 8);
	gtk_widget_show(pop_label);
	gtk_container_add(GTK_CONTAINER(popover), pop_label);
	g_signal_connect_swapped(info_btn, "clicked", G_CALLBACK(gtk_popover_popup), popover);

	gtk_box_pack_start(GTK_BOX(header_box), info_btn, false, false, 0);
	gtk_widget_show_all(header_box);

	GtkWidget* frame = gtk_frame_new(nullptr);
	gtk_frame_set_label_widget(GTK_FRAME(frame), header_box);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	gtk_widget_set_margin_start(content, 12);
	gtk_widget_set_margin_top(content, 6);
	gtk_container_add(GTK_CONTAINER(frame), content);

	return frame;
}

GtkWidget* make_help_button(const gchar* accessible_name)
{
	// Same "?" affordance as make_info_frame()'s header button. The description
	// is exposed as a tooltip (set by the caller), which GTK reveals on hover and
	// on keyboard focus; the ATK name keeps the control announceable when the
	// tooltip is empty.
	GtkWidget* btn = gtk_button_new_with_label("?");
	gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
	gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
	// START keeps the "?" at its natural compact size when add_form_row() gives
	// the control-only cell hexpand, instead of letting it stretch across the
	// half — the same compact-render guard make_form_switch() applies to grid-
	// attached switches.
	gtk_widget_set_halign(btn, GTK_ALIGN_START);

	AtkObject* a11y = gtk_widget_get_accessible(btn);
	if (a11y != nullptr)
		atk_object_set_name(a11y, accessible_name);

	return btn;
}

GtkWidget* make_two_column_section()
{
	// column_homogeneous forces the two columns to identical width regardless of
	// their children's natural sizes, so the split is a strict 50/50 that cannot
	// drift the way two hexpanding box children can when one child's minimum
	// exceeds half (R1, FR-001). One shared grid (rather than two independent
	// sub-grids) keeps paired C1/C2 cells on the same row even when one side is
	// empty, since both columns share the grid's row heights.
	GtkWidget* grid = gtk_grid_new();
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), true);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	return grid;
}

void add_form_row(GtkWidget* grid, int column, int row, GtkWidget* label,
	GtkWidget* control, bool wide, GtkSizeGroup* label_group)
{
	if (label == nullptr)
	{
		// Control-only cell (e.g. a check button carrying its own text). hexpand
		// so it claims its full homogeneous half; its own content stays left.
		gtk_widget_set_hexpand(control, true);
		gtk_grid_attach(GTK_GRID(grid), control, column, row, 1, 1);
		return;
	}

	// Inline [label | control] cell. The cell box hexpands to fill its half; the
	// label sits left, the control either fills the remaining width (wide) or
	// stays at natural width with empty trailing space (narrow).
	GtkWidget* cell = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_hexpand(cell, true);

	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(cell), label, false, false, 0);
	if (label_group != nullptr)
		gtk_size_group_add_widget(label_group, label);

	if (wide)
	{
		gtk_widget_set_hexpand(control, true);
		gtk_box_pack_start(GTK_BOX(cell), control, true, true, 0);
	}
	else
	{
		gtk_widget_set_halign(control, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(cell), control, false, false, 0);
	}

	gtk_grid_attach(GTK_GRID(grid), cell, column, row, 1, 1);
}

GtkWidget* make_form_switch()
{
	GtkWidget* sw = gtk_switch_new();
	// START/CENTER keeps the switch at its natural compact size when attached
	// to a grid column sized by a wider sibling control, instead of letting it
	// stretch to fill the column on themes that draw the full trough.
	gtk_widget_set_halign(sw, GTK_ALIGN_START);
	gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
	return sw;
}

GtkWidget* wrap_in_scrolled(GtkWidget* content)
{
	GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), true);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(scroll), content);
	return scroll;
}

}
