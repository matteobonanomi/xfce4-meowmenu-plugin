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
