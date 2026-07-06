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

#include "core/user-session-relayout.h"

using namespace WhiskerMenu;

bool
WhiskerMenu::meow_box_contains_child(GtkBox* box, GtkWidget* child)
{
	if (!GTK_IS_BOX(box) || !GTK_IS_WIDGET(child))
		return false;

	GList* children = gtk_container_get_children(GTK_CONTAINER(box));
	const bool found = g_list_find(children, child) != nullptr;
	g_list_free(children);
	return found;
}

bool
WhiskerMenu::meow_box_repack_child(GtkBox* box, GtkWidget* child,
                                   bool pack_end, bool expand, bool fill,
                                   guint padding)
{
	if (!GTK_IS_BOX(box) || !GTK_IS_WIDGET(child))
		return false;

	g_object_ref(child);
	if (GtkWidget* parent = gtk_widget_get_parent(child))
	{
		if (GTK_IS_CONTAINER(parent))
			gtk_container_remove(GTK_CONTAINER(parent), child);
	}

	if (pack_end)
		gtk_box_pack_end(box, child, expand, fill, padding);
	else
		gtk_box_pack_start(box, child, expand, fill, padding);

	g_object_unref(child);
	return true;
}

bool
WhiskerMenu::meow_box_reorder_child_if_present(GtkBox* box, GtkWidget* child,
                                               gint position)
{
	if (!meow_box_contains_child(box, child))
		return false;

	gtk_box_reorder_child(box, child, position);
	return true;
}

void
WhiskerMenu::meow_widget_set_visible_if_valid(GtkWidget* widget, bool visible)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_visible(widget, visible);
}

void
WhiskerMenu::meow_widget_set_can_focus_if_valid(GtkWidget* widget,
                                                bool can_focus)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_can_focus(widget, can_focus);
}

void
WhiskerMenu::meow_widget_set_hexpand_if_valid(GtkWidget* widget, bool expand)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_hexpand(widget, expand);
}

void
WhiskerMenu::meow_widget_set_halign_if_valid(GtkWidget* widget, GtkAlign align)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_halign(widget, align);
}
