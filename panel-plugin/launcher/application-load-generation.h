/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_APPLICATION_LOAD_GENERATION_H
#define WHISKERMENU_APPLICATION_LOAD_GENERATION_H

#include <glib.h>

namespace WhiskerMenu
{

/* application_load_generation_can_commit:
 * @current_generation: generation currently owned by ApplicationsPage.
 * @job_generation: generation carried by the async load completion.
 * @cancelled: true when teardown or a newer clear invalidated this job.
 * @owner_alive: true when the completion still has a live owning page/window.
 *
 * Returns: true iff an async application load completion may publish its
 * replacement category set to the UI.
 */
bool application_load_generation_can_commit(guint64 current_generation,
		guint64 job_generation, bool cancelled, bool owner_alive);

}

#endif // WHISKERMENU_APPLICATION_LOAD_GENERATION_H
