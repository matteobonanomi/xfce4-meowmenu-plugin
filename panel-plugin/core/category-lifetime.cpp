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

