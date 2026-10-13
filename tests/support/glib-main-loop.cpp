#include "glib-main-loop.h"

#include <cstdio>

namespace meowmenu_test
{

namespace
{

gboolean stop_loop(gpointer data)
{
	g_main_loop_quit(static_cast<GMainLoop*>(data));
	return G_SOURCE_REMOVE;
}

}

bool run_until(const std::function<bool()>& condition, guint timeout_ms,
		const char* description, GMainContext* context)
{
	if (!context)
		context = g_main_context_default();
	const gint64 deadline = g_get_monotonic_time() +
			static_cast<gint64>(timeout_ms) * 1000;
	while (!condition())
	{
		while (g_main_context_pending(context))
			g_main_context_iteration(context, FALSE);
		if (g_get_monotonic_time() >= deadline)
		{
			g_test_message("timed out after %u ms waiting for %s", timeout_ms,
					description ? description : "condition");
			g_test_fail();
			return false;
		}
		g_usleep(1000);
	}
	return true;
}

void drain_for(guint duration_ms, GMainContext* context)
{
	if (!context)
		context = g_main_context_default();
	GMainLoop* loop = g_main_loop_new(context, FALSE);
	GSource* timeout = g_timeout_source_new(duration_ms);
	g_source_set_callback(timeout, stop_loop, loop, nullptr);
	g_source_attach(timeout, context);
	g_source_unref(timeout);
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
}

SourceOwner::SourceOwner()
	: source_id_(0)
{
}

SourceOwner::~SourceOwner()
{
	clear();
}

void SourceOwner::attach(guint source_id)
{
	clear();
	source_id_ = source_id;
}

void SourceOwner::fired()
{
	/* A fired source no longer belongs to the owner and must not be removed. */
	source_id_ = 0;
}

void SourceOwner::clear()
{
	if (source_id_ != 0)
	{
		g_source_remove(source_id_);
		source_id_ = 0;
	}
}

EnvironmentScope::EnvironmentScope(const std::vector<std::string>& names)
{
	for (const std::string& name : names)
		values_.push_back({name, g_strdup(g_getenv(name.c_str()))});
}

EnvironmentScope::~EnvironmentScope()
{
	for (const SavedValue& value : values_)
	{
		if (value.value)
			g_setenv(value.name.c_str(), value.value, TRUE);
		else
			g_unsetenv(value.name.c_str());
		g_free(value.value);
	}
}

}
