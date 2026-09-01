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

enum class ApplicationLoadStatus
{
	Queued,
	Loading,
	CandidateReady,
	Committed,
	Discarded
};

class ApplicationLoadGeneration
{
public:
	explicit ApplicationLoadGeneration(guint64 generation);

	guint64 id() const { return m_generation; }
	ApplicationLoadStatus status() const { return m_status; }
	bool follow_up_required() const { return m_invalidated; }

	/* Moves a newly queued generation into worker-owned loading. */
	bool start();

	/* Prevents publication and records that a replacement load is needed. */
	void invalidate();

	/* Terminates outstanding work without allowing later publication. */
	void cancel();

	/* Accepts only a coherent, current candidate from active loading. */
	bool candidate_ready(bool coherent);

	/* Publishes only for the live owner and current generation identifier. */
	bool commit(guint64 current_generation, bool owner_alive);

	/* Retires any generation that has not already committed. */
	void discard();

private:
	guint64 m_generation;
	ApplicationLoadStatus m_status;
	bool m_cancelled;
	bool m_invalidated;
};

}

#endif // WHISKERMENU_APPLICATION_LOAD_GENERATION_H
