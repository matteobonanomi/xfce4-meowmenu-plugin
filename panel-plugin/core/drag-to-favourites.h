/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_DRAG_TO_FAVOURITES_H
#define WHISKERMENU_DRAG_TO_FAVOURITES_H

namespace WhiskerMenu
{

enum class FavouriteDragPayload
{
	Application,
	Places
};

enum class FavouriteDropTarget
{
	ApplicationFavorites,
	PlacesFavourites
};

bool favourite_drop_accepts(FavouriteDragPayload payload,
		FavouriteDropTarget target);
bool application_favourites_drop_available(bool sidebar_enabled,
		bool places_active, bool favorites_visible);
bool places_favourites_drop_available(bool sidebar_enabled, bool places_enabled,
		bool places_active, bool favourites_visible, bool meowmenu_only);
int favourite_drag_preview_size();

}

#endif // WHISKERMENU_DRAG_TO_FAVOURITES_H
