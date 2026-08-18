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
WhiskerMenu::meow_container_contains_child(GtkContainer* container,
                                           GtkWidget* child)
{
	if (!GTK_IS_CONTAINER(container) || !GTK_IS_WIDGET(child))
		return false;

	GList* children = gtk_container_get_children(container);
	const bool found = g_list_find(children, child) != nullptr;
	g_list_free(children);
	return found;
}

bool
WhiskerMenu::meow_box_contains_child(GtkBox* box, GtkWidget* child)
{
	return GTK_IS_BOX(box)
			&& meow_container_contains_child(GTK_CONTAINER(box), child);
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
WhiskerMenu::meow_grid_attach_child(GtkGrid* grid, GtkWidget* child,
                                    gint left, gint top, gint width,
                                    gint height)
{
	if (!GTK_IS_GRID(grid) || !GTK_IS_WIDGET(child) || width <= 0 || height <= 0)
		return false;

	g_object_ref(child);
	if (GtkWidget* parent = gtk_widget_get_parent(child))
	{
		if (GTK_IS_CONTAINER(parent))
			gtk_container_remove(GTK_CONTAINER(parent), child);
	}
	gtk_grid_attach(grid, child, left, top, width, height);
	g_object_unref(child);
	return true;
}

bool
WhiskerMenu::meow_size_group_set_widget(GtkSizeGroup* group,
                                        GtkWidget* widget, bool member)
{
	if (!GTK_IS_SIZE_GROUP(group) || !GTK_IS_WIDGET(widget))
		return false;

	const bool current = g_slist_find(gtk_size_group_get_widgets(group), widget)
			!= nullptr;
	if (member && !current)
		gtk_size_group_add_widget(group, widget);
	else if (!member && current)
		gtk_size_group_remove_widget(group, widget);

	return member == (g_slist_find(gtk_size_group_get_widgets(group), widget)
			!= nullptr);
}

/* meow_configure_vertical_sidebar_width:
 * @sidebar: vertical navigation scroller that owns the width source.
 * @profile: optional visible header aligned with the navigation column.
 * @active: whether windowed vertical-column sizing is active.
 * @profile_visible: whether @profile contributes its natural width.
 *
 * Measures with stale requests cleared, then fixes both column owners to the
 * larger natural width. Explicitly disabling expansion keeps later surplus
 * window allocation in the Results column rather than feeding it back through
 * a cross-parent GtkSizeGroup.
 *
 * Returns: the applied width, or -1 when inactive or given invalid widgets.
 */
int
WhiskerMenu::meow_configure_vertical_sidebar_width(GtkWidget* sidebar,
                                                   GtkWidget* profile,
                                                   bool active,
                                                   bool profile_visible)
{
	if (!GTK_IS_WIDGET(sidebar) || !GTK_IS_WIDGET(profile))
		return -1;

	int sidebar_height = -1;
	int profile_height = -1;
	gtk_widget_get_size_request(sidebar, nullptr, &sidebar_height);
	gtk_widget_get_size_request(profile, nullptr, &profile_height);
	gtk_widget_set_size_request(sidebar, -1, sidebar_height);
	gtk_widget_set_size_request(profile, -1, profile_height);
	gtk_widget_set_hexpand(sidebar, FALSE);
	gtk_widget_set_hexpand(profile, FALSE);
	if (!active)
		return -1;

	int sidebar_natural = 0;
	int profile_natural = 0;
	gtk_widget_get_preferred_width(sidebar, nullptr, &sidebar_natural);
	if (profile_visible)
		gtk_widget_get_preferred_width(profile, nullptr, &profile_natural);
	const int width = MAX(sidebar_natural, profile_natural);
	gtk_widget_set_size_request(sidebar, width, sidebar_height);
	if (profile_visible)
		gtk_widget_set_size_request(profile, width, profile_height);
	return width;
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
WhiskerMenu::meow_widget_set_vexpand_if_valid(GtkWidget* widget, bool expand)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_vexpand(widget, expand);
}

void
WhiskerMenu::meow_widget_set_halign_if_valid(GtkWidget* widget, GtkAlign align)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_halign(widget, align);
}

void
WhiskerMenu::meow_widget_set_valign_if_valid(GtkWidget* widget, GtkAlign align)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_valign(widget, align);
}
