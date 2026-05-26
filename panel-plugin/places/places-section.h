/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_PLACES_SECTION_H
#define WHISKERMENU_PLACES_SECTION_H

#include <vector>

#include <glib.h>

namespace WhiskerMenu
{

class PlacesItem;

/* PlacesSection:
 *
 * Pure data provider for one Places sub-section (Home / History / Favourites).
 * Not a GTK widget. Concrete sections own the PlacesItem* objects they hand out
 * via get_items() and free them in clear_items().
 */
class PlacesSection
{
public:
	virtual ~PlacesSection() = default;

	PlacesSection(const PlacesSection&) = delete;
	PlacesSection& operator=(const PlacesSection&) = delete;

	/* get_items:
	 * @max: maximum number of items to return; ignored when section has a
	 *       natural cap (Home, fixed to the 9 XDG dirs).
	 * Returns: caller borrows the pointers; PlacesSection retains ownership
	 *          and frees them on the next clear_items() or destructor call.
	 */
	virtual std::vector<PlacesItem*> get_items(int max) = 0;

	virtual void clear_items() = 0;

	virtual const gchar* get_icon_name() const = 0;
	virtual const gchar* get_display_name() const = 0;

protected:
	PlacesSection() = default;
};

}

#endif // WHISKERMENU_PLACES_SECTION_H
