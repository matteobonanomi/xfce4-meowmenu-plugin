/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_COMMAND_TIMEOUT_H
#define WHISKERMENU_COMMAND_TIMEOUT_H

namespace WhiskerMenu
{

/* command_timeout_source_is_active:
 * @time_left: countdown state after the confirmation dialog exits.
 *
 * Returns: true while the GLib timeout source still needs explicit removal.
 * Expiry decrements the counter below zero and removes the source itself.
 */
inline bool command_timeout_source_is_active(int time_left)
{
	return time_left >= 0;
}

}

#endif // WHISKERMENU_COMMAND_TIMEOUT_H
