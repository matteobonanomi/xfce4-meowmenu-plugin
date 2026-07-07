/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_CATEGORY_LIFETIME_H
#define WHISKERMENU_CATEGORY_LIFETIME_H

#include <vector>

#include <gtk/gtk.h>

namespace WhiskerMenu
{

/* detach_category_widgets:
 * @width_group: optional size group that may include the dynamic widgets.
 * @widgets: borrowed dynamic category widgets currently packed in the window.
 *
 * Removes every borrowed dynamic category widget from transient window-owned
 * GTK containers and clears @widgets. The function does not destroy widgets;
 * their CategoryButton owners keep the lifetime responsibility.
 */
void detach_category_widgets(GtkSizeGroup* width_group,
		std::vector<GtkWidget*>& widgets);

}

#endif // WHISKERMENU_CATEGORY_LIFETIME_H
