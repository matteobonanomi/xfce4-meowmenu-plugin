/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "application-load-generation.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

bool WhiskerMenu::application_load_generation_can_commit(guint64 current_generation,
		guint64 job_generation, bool cancelled, bool owner_alive)
{
	return owner_alive && !cancelled && (current_generation == job_generation);
}

