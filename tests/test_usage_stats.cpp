/*
 * Production-linked normal-path coverage for the usage statistics cache.
 */

#include "config/usage-stats.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <glib/gstdio.h>

using namespace WhiskerMenu;

int main()
{
	GError* error = nullptr;
	gchar* scratch = g_dir_make_tmp("meow-usage-stats-XXXXXX", &error);
	assert(scratch && !error);
	assert(g_setenv("XDG_CACHE_HOME", scratch, TRUE));

	gchar* cache_dir = g_build_filename(scratch, "xfce4", "meowmenu", nullptr);
	assert(g_mkdir_with_parents(cache_dir, 0700) == 0);
	gchar* cache_path = g_build_filename(cache_dir, "stats", nullptr);
	const gint64 now = g_get_real_time() / G_USEC_PER_SEC;
	gchar* seed = g_strdup_printf("org.example.Seeded.desktop\t%lld\t4\n",
			static_cast<long long>(now));
	assert(g_file_set_contents(cache_path, seed, -1, &error));
	assert(!error);
	g_free(seed);

	UsageStats stats;
	assert(stats.get_frecency("org.example.Seeded.desktop", 0.5) > 0.0);
	assert(stats.get_frecency("org.example.Unknown.desktop", 0.5) == 0.0);

	// Both updates occur before the idle runs; the ordinary coalesced write must
	// persist the combined count rather than losing the second launch.
	stats.record_launch("org.example.New.desktop");
	stats.record_launch("org.example.New.desktop");
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);

	gchar* contents = nullptr;
	gsize length = 0;
	assert(g_file_get_contents(cache_path, &contents, &length, &error));
	assert(!error);
	assert(std::strstr(contents, "org.example.Seeded.desktop\t") != nullptr);
	assert(std::strstr(contents, "org.example.New.desktop\t") != nullptr);
	assert(std::strstr(contents, "\t2\n") != nullptr);
	g_free(contents);

	assert(g_remove(cache_path) == 0);
	assert(g_rmdir(cache_dir) == 0);
	gchar* xfce_dir = g_build_filename(scratch, "xfce4", nullptr);
	assert(g_rmdir(xfce_dir) == 0);
	assert(g_rmdir(scratch) == 0);
	g_free(xfce_dir);
	g_free(cache_path);
	g_free(cache_dir);
	g_free(scratch);
	std::printf("test_usage_stats: ok\n");
	return 0;
}
