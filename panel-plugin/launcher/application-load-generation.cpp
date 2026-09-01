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

ApplicationLoadGeneration::ApplicationLoadGeneration(guint64 generation) :
	m_generation(generation),
	m_status(ApplicationLoadStatus::Queued),
	m_cancelled(false),
	m_invalidated(false)
{
}

//-----------------------------------------------------------------------------

bool ApplicationLoadGeneration::start()
{
	if (m_status != ApplicationLoadStatus::Queued)
	{
		return false;
	}
	m_status = ApplicationLoadStatus::Loading;
	return true;
}

//-----------------------------------------------------------------------------

void ApplicationLoadGeneration::invalidate()
{
	if (m_status == ApplicationLoadStatus::Queued
			|| m_status == ApplicationLoadStatus::Loading
			|| m_status == ApplicationLoadStatus::CandidateReady)
	{
		m_invalidated = true;
	}
}

//-----------------------------------------------------------------------------

void ApplicationLoadGeneration::cancel()
{
	if (m_status == ApplicationLoadStatus::Committed
			|| m_status == ApplicationLoadStatus::Discarded)
	{
		return;
	}
	m_cancelled = true;
	m_status = ApplicationLoadStatus::Discarded;
}

//-----------------------------------------------------------------------------

bool ApplicationLoadGeneration::candidate_ready(bool coherent)
{
	if (m_status != ApplicationLoadStatus::Loading)
	{
		return false;
	}
	if (!coherent || m_cancelled || m_invalidated)
	{
		m_status = ApplicationLoadStatus::Discarded;
		return false;
	}
	m_status = ApplicationLoadStatus::CandidateReady;
	return true;
}

//-----------------------------------------------------------------------------

bool ApplicationLoadGeneration::commit(guint64 current_generation,
		bool owner_alive)
{
	if (m_status != ApplicationLoadStatus::CandidateReady
			|| !owner_alive || m_cancelled || m_invalidated
			|| current_generation != m_generation)
	{
		discard();
		return false;
	}
	m_status = ApplicationLoadStatus::Committed;
	return true;
}

//-----------------------------------------------------------------------------

void ApplicationLoadGeneration::discard()
{
	if (m_status != ApplicationLoadStatus::Committed)
	{
		m_status = ApplicationLoadStatus::Discarded;
	}
}
