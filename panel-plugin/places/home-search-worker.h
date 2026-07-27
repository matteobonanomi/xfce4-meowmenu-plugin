/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_HOME_SEARCH_WORKER_H
#define WHISKERMENU_HOME_SEARCH_WORKER_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <gio/gio.h>

namespace WhiskerMenu
{

class PlacesItem;

/* HomeSearchWorker:
 *
 * Per-search worker that performs a bounded BFS walk of $HOME from a
 * dedicated GThread and dispatches matches back to the GTK main thread
 * via g_idle_add. Owns its GCancellable and the casefolded query.
 *
 * Lifetime: created via start(); owner can drop the pointer at any time
 * after calling cancel(). Internal sink keeps callbacks alive until
 * pending idles drain.
 *
 * Thread-safety: the worker thread touches only GLib/GIO and PlacesItem
 * construction (no GTK). All user callbacks fire on the main thread.
 */
class HomeSearchWorker
{
public:
	using ResultCallback = std::function<void(PlacesItem*)>;
	using DoneCallback   = std::function<void()>;

	/* start:
	 * @casefolded_query: g_utf8_casefold'd query string; must be non-empty.
	 * @cap: hard upper bound on dispatched matches (the documented behavior).
	 * @on_result: invoked on the main thread for each match; takes ownership.
	 * @on_done: invoked once on the main thread when the walk exits.
	 *
	 * Returns: a newly allocated worker; caller owns and may cancel.
	 */
	static HomeSearchWorker* start(const gchar* casefolded_query,
			int cap,
			ResultCallback on_result,
			DoneCallback on_done);

	~HomeSearchWorker();

	/* cancel:
	 *
	 * Signals cancellation, joins the worker thread, and silences any
	 * pending result/done idles. Idempotent; safe to call after natural
	 * completion. After return, no user callback will fire.
	 */
	void cancel();


	HomeSearchWorker(const HomeSearchWorker&) = delete;
	HomeSearchWorker& operator=(const HomeSearchWorker&) = delete;

private:
	struct Sink
	{
		std::atomic<bool> cancelled;
		ResultCallback on_result;
		DoneCallback on_done;

		Sink() : cancelled(false) {}
	};

	struct ResultCtx
	{
		std::shared_ptr<Sink> sink;
		PlacesItem* item;
	};

	struct DoneCtx
	{
		std::shared_ptr<Sink> sink;
	};

	HomeSearchWorker(const std::string& query, int cap,
			ResultCallback on_result, DoneCallback on_done);

	static gpointer thread_main_trampoline(gpointer data);
	void thread_main();
	void walk(const gchar* root_path);

	static gboolean on_result_idle(gpointer data);
	static gboolean on_done_idle(gpointer data);

	GThread* m_thread;
	GCancellable* m_cancellable;
	std::string m_query;
	int m_cap;
	gint64 m_start_us;
	std::atomic<int> m_dispatched;
	std::shared_ptr<Sink> m_sink;
	bool m_joined;
};

} // namespace WhiskerMenu

#endif // WHISKERMENU_HOME_SEARCH_WORKER_H
