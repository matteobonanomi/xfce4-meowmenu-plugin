/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "drag-to-favourites.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

bool WhiskerMenu::favourite_drop_accepts(FavouriteDragPayload payload,
		FavouriteDropTarget target)
{
	return ((payload == FavouriteDragPayload::Application)
				&& (target == FavouriteDropTarget::ApplicationFavorites))
			|| ((payload == FavouriteDragPayload::Places)
				&& (target == FavouriteDropTarget::PlacesFavourites));
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::application_favourites_drop_available(bool sidebar_enabled,
		bool places_active, bool favorites_visible)
{
	return sidebar_enabled && !places_active && favorites_visible;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::places_favourites_drop_available(bool sidebar_enabled,
		bool places_enabled, bool places_active, bool favourites_visible,
		bool meowmenu_only)
{
	return sidebar_enabled
			&& places_enabled
			&& places_active
			&& favourites_visible
			&& meowmenu_only;
}

//-----------------------------------------------------------------------------

int WhiskerMenu::favourite_drag_preview_size()
{
	return 32;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::launcher_drag_should_hide_menu_after_end(
		bool favourite_payload_delivered)
{
	return !favourite_payload_delivered;
}

//-----------------------------------------------------------------------------
