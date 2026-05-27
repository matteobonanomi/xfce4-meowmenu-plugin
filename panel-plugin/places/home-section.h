/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_HOME_SECTION_H
#define WHISKERMENU_HOME_SECTION_H

#include "home-search-worker.h"
#include "places-section.h"

namespace WhiskerMenu
{

class HomeSection : public PlacesSection
{
public:
	HomeSection();
	~HomeSection() override;

	std::vector<PlacesItem*> get_items(int max) override;
	void clear_items() override;
	const gchar* get_icon_name() const override { return "user-home"; }
	const gchar* get_display_name() const override;

	/* start_search:
	 * @casefolded_query: caller-supplied casefolded query, non-empty.
	 * @cap: hard upper bound on results.
	 * @on_result: invoked on the main thread per match; takes ownership.
	 * @on_done: invoked once when the walk exits.
	 *
	 * Spawns a HomeSearchWorker. Caller MUST call cancel_search() before
	 * invoking again; this method does not auto-cancel.
	 */
	void start_search(const gchar* casefolded_query, int cap,
			HomeSearchWorker::ResultCallback on_result,
			HomeSearchWorker::DoneCallback on_done);

	/* cancel_search:
	 *
	 * Cancels and frees the active worker, if any. Idempotent.
	 */
	void cancel_search();

	bool is_searching() const { return m_worker != nullptr; }

private:
	std::vector<PlacesItem*> m_items;
	HomeSearchWorker* m_worker;
};

}

#endif // WHISKERMENU_HOME_SECTION_H
