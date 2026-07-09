/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "category-lifetime.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

void WhiskerMenu::detach_category_widgets(GtkSizeGroup* width_group,
		std::vector<GtkWidget*>& widgets)
{
	for (GtkWidget* widget : widgets)
	{
		if (width_group)
		{
			gtk_size_group_remove_widget(width_group, widget);
		}

		GtkWidget* parent = gtk_widget_get_parent(widget);
		if (parent)
		{
			gtk_container_remove(GTK_CONTAINER(parent), widget);
		}
	}

	widgets.clear();
}

//-----------------------------------------------------------------------------

GtkWidget* WhiskerMenu::active_toggle_child_or_default(GtkContainer* container,
		GtkWidget* fallback)
{
	GtkWidget* widget = fallback;

	if (!container)
	{
		return widget;
	}

	GList* children = gtk_container_get_children(container);
	for (GList* li = children; li; li = li->next)
	{
		if (!GTK_IS_TOGGLE_BUTTON(li->data))
		{
			continue;
		}

		GtkToggleButton* button = GTK_TOGGLE_BUTTON(li->data);
		if (gtk_toggle_button_get_active(button))
		{
			widget = GTK_WIDGET(button);
			break;
		}
	}
	g_list_free(children);

	return widget;
}
