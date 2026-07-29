/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "home-search-worker.h"

#include "places-item.h"

#include <algorithm>
#include <cstring>
#include <deque>

#include <glib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

namespace
{

// the documented behavior §2: hard-coded blocklist of directory names that are never
// descended. The dot-prefix rule (the documented behavior §1) already covers .cache,
// .local, .git etc.; these entries catch the visible heavyweights.
static const char* const HOME_SEARCH_BLOCKLIST[] = {
	"node_modules",
	"__pycache__",
	"target",
	"build",
	"dist",
	nullptr
};

// the documented behavior wall-clock budgets.
static constexpr gint64 SOFT_BUDGET_US = 150 * 1000;
static constexpr gint64 HARD_BUDGET_US = 300 * 1000;

static bool is_blocklisted(const gchar* name)
{
	for (const char* const* p = HOME_SEARCH_BLOCKLIST; *p; ++p)
	{
		if (g_strcmp0(name, *p) == 0)
		{
			return true;
		}
	}
	return false;
}

} // anonymous namespace

//-----------------------------------------------------------------------------

HomeSearchWorker::HomeSearchWorker(const std::string& query, int cap,
		ResultCallback on_result, DoneCallback on_done) :
	m_thread(nullptr),
	m_cancellable(g_cancellable_new()),
	m_query(query),
	m_cap(cap > 0 ? cap : 0),
	m_start_us(0),
	m_dispatched(0),
	m_sink(std::make_shared<Sink>()),
	m_joined(false)
{
	m_sink->on_result = std::move(on_result);
	m_sink->on_done = std::move(on_done);
}

//-----------------------------------------------------------------------------

HomeSearchWorker* HomeSearchWorker::start(const gchar* casefolded_query,
		int cap, ResultCallback on_result, DoneCallback on_done)
{
	if (!casefolded_query || !*casefolded_query)
	{
		return nullptr;
	}

	auto* w = new HomeSearchWorker(casefolded_query, cap,
			std::move(on_result), std::move(on_done));
	w->m_thread = g_thread_new("meowmenu-home-search",
			&HomeSearchWorker::thread_main_trampoline, w);
	return w;
}

//-----------------------------------------------------------------------------

HomeSearchWorker::~HomeSearchWorker()
{
	cancel();
	if (m_cancellable)
	{
		g_object_unref(m_cancellable);
		m_cancellable = nullptr;
	}
}

//-----------------------------------------------------------------------------

void HomeSearchWorker::cancel()
{
	// NOTE: cancelled is observed both by GIO (via g_cancellable_cancel)
	// and by pending main-thread idles (via the sink flag). Owner-side
	// callbacks are silenced atomically.
	m_sink->cancelled.store(true, std::memory_order_release);
	if (m_cancellable)
	{
		g_cancellable_cancel(m_cancellable);
	}
	if (m_thread && !m_joined)
	{
		g_thread_join(m_thread);
		m_thread = nullptr;
		m_joined = true;
	}
}

//-----------------------------------------------------------------------------

gpointer HomeSearchWorker::thread_main_trampoline(gpointer data)
{
	static_cast<HomeSearchWorker*>(data)->thread_main();
	return nullptr;
}

void HomeSearchWorker::thread_main()
{
	m_start_us = g_get_monotonic_time();

	const gchar* home = g_get_home_dir();
	if (home && *home)
	{
		walk(home);
	}

	// Dispatch on_done last so it always trails the result idles in
	// FIFO order.
	auto* ctx = new DoneCtx{ m_sink };
	g_idle_add(&HomeSearchWorker::on_done_idle, ctx);
}

//-----------------------------------------------------------------------------

/* walk:
 *
 * BFS over @root_path. Skips hidden directories, the fixed blocklist, and
 * symlinks-to-dirs. Dispatches matches to the main thread via g_idle_add.
 */
void HomeSearchWorker::walk(const gchar* root_path)
{
	std::deque<GFile*> queue;
	queue.push_back(g_file_new_for_path(root_path));

	bool soft_budget_exhausted = false;

	while (!queue.empty())
	{
		if (g_cancellable_is_cancelled(m_cancellable))
		{
			break;
		}

		const gint64 elapsed = g_get_monotonic_time() - m_start_us;
		if (elapsed >= HARD_BUDGET_US)
		{
			break;
		}
		if (!soft_budget_exhausted && elapsed >= SOFT_BUDGET_US)
		{
			// Stop descending into new directories; drain the current
			// queue contents (already-discovered siblings) only.
			soft_budget_exhausted = true;
		}

		GFile* dir = queue.front();
		queue.pop_front();

		GError* error = nullptr;
		GFileEnumerator* en = g_file_enumerate_children(dir,
				G_FILE_ATTRIBUTE_STANDARD_NAME ","
				G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
				G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				m_cancellable, &error);

		if (!en)
		{
			if (error) g_error_free(error);
			g_object_unref(dir);
			continue;
		}

		// Per-layer buffer for deterministic ordering: collect this
		// directory's matches, sort casefolded-name → path, then dispatch.
		struct Match { std::string sort_key; GFile* file; };
		std::deque<Match> matches;

		while (true)
		{
			if (g_cancellable_is_cancelled(m_cancellable))
			{
				break;
			}
			GFileInfo* info = g_file_enumerator_next_file(en, m_cancellable, nullptr);
			if (!info)
			{
				break;
			}

			const gchar* name = g_file_info_get_name(info);
			const GFileType type = g_file_info_get_file_type(info);
			const gboolean is_symlink = g_file_info_get_is_symlink(info);

			if (!name || name[0] == '.')
			{
				g_object_unref(info);
				continue;
			}
			if (is_blocklisted(name))
			{
				g_object_unref(info);
				continue;
			}
			if (type == G_FILE_TYPE_DIRECTORY && is_symlink)
			{
				g_object_unref(info);
				continue;
			}

			GFile* child = g_file_get_child(dir, name);

			// Match test (the documented behavior).
			gchar* folded = g_utf8_casefold(name, -1);
			const bool match = folded
					&& strstr(folded, m_query.c_str()) != nullptr;
			if (match)
			{
				matches.push_back({ folded ? folded : "", child });
				g_object_ref(child); // hold reference for the match list
			}
			g_free(folded);

			// Queue subdirectory for descent unless soft budget expired.
			if (type == G_FILE_TYPE_DIRECTORY && !soft_budget_exhausted)
			{
				queue.push_back(G_FILE(g_object_ref(child)));
			}

			g_object_unref(child);
			g_object_unref(info);
		}

		g_file_enumerator_close(en, nullptr, nullptr);
		g_object_unref(en);
		g_object_unref(dir);

		// Sort matches in this layer for deterministic order.
		std::sort(matches.begin(), matches.end(),
				[](const Match& a, const Match& b)
				{
					return a.sort_key < b.sort_key;
				});

		for (auto& m : matches)
		{
			if (g_cancellable_is_cancelled(m_cancellable))
			{
				g_object_unref(m.file);
				continue;
			}

			PlacesItem* item = new PlacesItem(m.file);
			g_object_unref(m.file);

			const int n = m_dispatched.fetch_add(1, std::memory_order_acq_rel) + 1;
			if (m_cap > 0 && n > m_cap)
			{
				// Already at cap: don't dispatch this one.
				delete item;
				g_cancellable_cancel(m_cancellable);
				continue;
			}

			auto* rctx = new ResultCtx{ m_sink, item };
			g_idle_add(&HomeSearchWorker::on_result_idle, rctx);

			if (m_cap > 0 && n >= m_cap)
			{
				g_cancellable_cancel(m_cancellable);
			}
		}
	}

	// Drain any remaining queued directories without descending.
	while (!queue.empty())
	{
		g_object_unref(queue.front());
		queue.pop_front();
	}
}

//-----------------------------------------------------------------------------

gboolean HomeSearchWorker::on_result_idle(gpointer data)
{
	auto* ctx = static_cast<ResultCtx*>(data);
	auto sink = ctx->sink;
	PlacesItem* item = ctx->item;
	delete ctx;

	if (sink->cancelled.load(std::memory_order_acquire))
	{
		delete item;
	}
	else if (sink->on_result)
	{
		sink->on_result(item);
	}
	else
	{
		delete item;
	}
	return G_SOURCE_REMOVE;
}

//-----------------------------------------------------------------------------

gboolean HomeSearchWorker::on_done_idle(gpointer data)
{
	auto* ctx = static_cast<DoneCtx*>(data);
	auto sink = ctx->sink;
	delete ctx;

	if (!sink->cancelled.load(std::memory_order_acquire) && sink->on_done)
	{
		sink->on_done();
	}
	return G_SOURCE_REMOVE;
}

//-----------------------------------------------------------------------------

