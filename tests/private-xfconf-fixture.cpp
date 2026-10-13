#include "private-xfconf-fixture.h"

#include "support/xfconf-fixture.h"
#include "support/glib-main-loop.h"
#include "support/assertions.h"

#include <cstdio>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <xfconf/xfconf.h>

namespace
{

const char* const k_child_argument = "--private-xfconf-child";

}

/* run_with_private_xfconf:
 * @argc: argument count from the test entry point.
 * @argv: argument vector from the test entry point.
 * @child_main: test function to run inside the private Xfconf process.
 *
 * Runs a test in a private Xfconf process and isolates its runtime data from
 * the developer's profile so local preset overrides cannot affect the result.
 *
 * Returns: the child result, or 77 when the private fixture is unavailable.
 */
int run_with_private_xfconf(int argc, char** argv,
		int (*child_main)(int argc, char** argv))
{
	if (argc == 2 && g_strcmp0(argv[1], k_child_argument) == 0)
	{
		// Keep runtime data out of the developer's profile. Built-in preset files
		// in XDG_DATA_HOME are user overrides, so stale local files must not alter
		// lifecycle assertions or make the result depend on the checkout host.
		gchar* test_data_home = g_dir_make_tmp("meowmenu-test-data-XXXXXX", nullptr);
		if (test_data_home)
			g_setenv("XDG_DATA_HOME", test_data_home, TRUE);

		const gboolean initialized = xfconf_init(nullptr);
		meowmenu_test::install_fatal_diagnostics();
		const int result = child_main(argc, argv);
		meowmenu_test::drain_for(250);
		if (initialized)
			xfconf_shutdown();
		if (test_data_home)
		{
			g_rmdir(test_data_home);
			g_free(test_data_home);
		}
		return result;
	}

	meowmenu_test::XfconfFixture fixture;
	if (!fixture.start())
	{
		if (g_strcmp0(g_getenv("MEOWMENU_REQUIRE_XFCONF"), "1") == 0)
		{
			std::fprintf(stderr, "required private Xfconf fixture unavailable\n");
			return 1;
		}
		std::printf("# SKIP: private D-Bus/xfconfd fixture unavailable\n");
		return 77;
	}

	gchar* child_argv[] = {
		argv[0], const_cast<gchar*>(k_child_argument), nullptr,
	};
	gint child_status = 0;
	GError* error = nullptr;
	const bool spawned = g_spawn_sync(nullptr, child_argv, nullptr,
			G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, nullptr,
			&child_status, &error);
	fixture.stop();
	if (!spawned)
	{
		std::fprintf(stderr, "private Xfconf child failed: %s\n",
				error ? error->message : "unknown error");
		g_clear_error(&error);
		return 1;
	}
	if (WIFEXITED(child_status))
		return WEXITSTATUS(child_status);
	std::fprintf(stderr, "private Xfconf child ended abnormally\n");
	return 1;
}

void settle_private_xfconf()
{
	meowmenu_test::drain_for(10);
	GDBusConnection* connection = g_bus_get_sync(
			G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (connection)
	{
		g_dbus_connection_flush_sync(connection, nullptr, nullptr);
		g_object_unref(connection);
	}
	meowmenu_test::drain_for(10);
}

void seed_private_xfconf_profile(const char* channel_name,
		const char* property_base, int schema_version)
{
	XfconfChannel* channel = xfconf_channel_new_with_property_base(
			channel_name, property_base);
	g_return_if_fail(channel != nullptr);
	const gboolean initialized = xfconf_channel_set_bool(
			channel, "/initialized", TRUE);
	const gboolean versioned = xfconf_channel_set_int(
			channel, "/schema-version", schema_version);
	g_object_unref(channel);
	g_return_if_fail(initialized && versioned);
}
