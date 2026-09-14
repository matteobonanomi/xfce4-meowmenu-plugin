#include "private-xfconf-fixture.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <sys/wait.h>

namespace
{

const char* k_child_argument = "--private-xfconf-child";

const char* const k_xfconfd_candidates[] = {
	"/usr/lib/x86_64-linux-gnu/xfce4/xfconf/xfconfd",
	"/usr/lib/xfce4/xfconf/xfconfd",
	"/usr/libexec/xfconf/xfconfd",
	"/usr/libexec/xfce4/xfconf/xfconfd",
	"/usr/lib64/xfce4/xfconf/xfconfd",
	nullptr
};

const char* find_xfconfd()
{
	for (const char* const* path = k_xfconfd_candidates; *path; ++path)
	{
		if (g_file_test(*path, G_FILE_TEST_IS_EXECUTABLE))
			return *path;
	}
	return nullptr;
}

/* remove_tree:
 * @path: exact scratch directory created by this process.
 *
 * Removes the private daemon state after both the daemon and bus have stopped.
 */
void remove_tree(const char* path)
{
	GDir* directory = g_dir_open(path, 0, nullptr);
	if (directory)
	{
		while (const gchar* name = g_dir_read_name(directory))
		{
			gchar* child = g_build_filename(path, name, nullptr);
			if (g_file_test(child, G_FILE_TEST_IS_DIR))
				remove_tree(child);
			else
				g_remove(child);
			g_free(child);
		}
		g_dir_close(directory);
	}
	g_rmdir(path);
}

bool wait_for_registration()
{
	GDBusConnection* connection = g_bus_get_sync(
			G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (!connection)
		return false;
	for (int attempt = 0; attempt < 50; ++attempt)
	{
		GVariant* result = g_dbus_connection_call_sync(connection,
				"org.freedesktop.DBus", "/org/freedesktop/DBus",
				"org.freedesktop.DBus", "NameHasOwner",
				g_variant_new("(s)", "org.xfce.Xfconf"),
				G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE,
				-1, nullptr, nullptr);
		gboolean owned = FALSE;
		if (result)
		{
			g_variant_get(result, "(b)", &owned);
			g_variant_unref(result);
		}
		if (owned)
		{
			g_object_unref(connection);
			return true;
		}
		g_usleep(100 * 1000);
	}
	g_object_unref(connection);
	return false;
}

/* stop_xfconfd:
 * @pid: child daemon spawned with G_SPAWN_DO_NOT_REAP_CHILD.
 *
 * Waits for termination before scratch cleanup so the daemon cannot recreate
 * its XML file after the directory has been removed.
 */
void stop_xfconfd(GPid pid)
{
	kill(pid, SIGTERM);
	gint status = 0;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
	{
	}
	g_spawn_close_pid(pid);
}

}

int run_with_private_xfconf(int argc, char** argv,
		int (*child_main)(int argc, char** argv))
{
	if (argc == 2 && std::strcmp(argv[1], k_child_argument) == 0)
		return child_main(argc, argv);

	gchar* display = g_strdup(g_getenv("DISPLAY"));
	gchar* xauthority = g_strdup(g_getenv("XAUTHORITY"));
	const char* xfconfd = find_xfconfd();
	gchar* dbus_daemon = g_find_program_in_path("dbus-daemon");
	if (!xfconfd || !dbus_daemon)
	{
		std::printf("# SKIP: private D-Bus/xfconfd fixture unavailable\n");
		g_free(dbus_daemon);
		g_free(display);
		g_free(xauthority);
		return 77;
	}
	g_free(dbus_daemon);
	gchar* scratch = g_dir_make_tmp("meow-private-xfconf-XXXXXX", nullptr);
	if (!scratch)
	{
		std::printf("# SKIP: private Xfconf scratch directory unavailable\n");
		g_free(display);
		g_free(xauthority);
		return 77;
	}
	g_setenv("XDG_CONFIG_HOME", scratch, TRUE);
	g_setenv("XDG_CACHE_HOME", scratch, TRUE);
	g_setenv("XDG_RUNTIME_DIR", scratch, TRUE);

	GTestDBus* bus = g_test_dbus_new(G_TEST_DBUS_NONE);
	g_test_dbus_up(bus);
	if (display)
		g_setenv("DISPLAY", display, TRUE);
	if (xauthority)
		g_setenv("XAUTHORITY", xauthority, TRUE);
	g_free(display);
	g_free(xauthority);
	GPid xfconfd_pid = 0;
	GError* error = nullptr;
	gchar* daemon_argv[] = { const_cast<gchar*>(xfconfd), nullptr };
	const bool spawned = g_spawn_async(nullptr, daemon_argv, nullptr,
			static_cast<GSpawnFlags>(G_SPAWN_LEAVE_DESCRIPTORS_OPEN
					| G_SPAWN_DO_NOT_REAP_CHILD
					| G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
			nullptr, nullptr, &xfconfd_pid, &error);
	if (!spawned || !wait_for_registration())
	{
		std::printf("# SKIP: private xfconfd failed to start%s%s\n",
				error ? ": " : "", error ? error->message : "");
		g_clear_error(&error);
		if (spawned)
			stop_xfconfd(xfconfd_pid);
		g_test_dbus_down(bus);
		g_object_unref(bus);
		remove_tree(scratch);
		g_free(scratch);
		return 77;
	}

	gchar* child_argv[] = { argv[0], const_cast<gchar*>(k_child_argument), nullptr };
	gint child_status = 0;
	const bool child_spawned = g_spawn_sync(nullptr, child_argv, nullptr,
			G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, nullptr,
			&child_status, &error);

	stop_xfconfd(xfconfd_pid);
	g_test_dbus_down(bus);
	g_object_unref(bus);
	remove_tree(scratch);
	g_free(scratch);

	if (!child_spawned)
	{
		if (error)
			std::fprintf(stderr, "private fixture child failed: %s\n", error->message);
		g_clear_error(&error);
		return 1;
	}
	if (WIFEXITED(child_status))
		return WEXITSTATUS(child_status);
	std::fprintf(stderr, "private fixture child ended abnormally\n");
	return 1;
}
