#include "xfconf-fixture.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/wait.h>

#include <glib/gstdio.h>

namespace
{

const char* find_xfconfd()
{
	static const char* const candidates[] = {
		"/usr/lib/x86_64-linux-gnu/xfce4/xfconf/xfconfd",
		"/usr/lib/xfce4/xfconf/xfconfd",
		"/usr/libexec/xfconf/xfconfd",
		"/usr/libexec/xfce4/xfconf/xfconfd",
		"/usr/lib64/xfce4/xfconf/xfconfd",
		nullptr,
	};
	for (const char* const* path = candidates; *path; ++path)
		if (g_file_test(*path, G_FILE_TEST_IS_EXECUTABLE))
			return *path;
	return nullptr;
}

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

}

namespace meowmenu_test
{

XfconfFixture::XfconfFixture()
	: bus_(nullptr), xfconfd_pid_(0), active_(false)
{
	static guint serial = 0;
	property_base_ = "/plugins/plugin-test-" +
			std::to_string(++serial);
}

XfconfFixture::~XfconfFixture()
{
	stop();
}

bool XfconfFixture::start()
{
	if (active_)
		return true;
	const char* xfconfd = find_xfconfd();
	gchar* dbus_daemon = g_find_program_in_path("dbus-daemon");
	if (!xfconfd || !dbus_daemon)
	{
		g_free(dbus_daemon);
		g_test_message("# SKIP: private D-Bus/xfconfd fixture unavailable");
		return false;
	}
	g_free(dbus_daemon);
	gchar* scratch = g_dir_make_tmp("meowmenu-private-xfconf-XXXXXX", nullptr);
	if (!scratch)
	{
		g_test_message("# SKIP: private Xfconf scratch directory unavailable");
		return false;
	}
	scratch_ = scratch;
	g_free(scratch);
	gchar* display = g_strdup(g_getenv("DISPLAY"));
	gchar* xauthority = g_strdup(g_getenv("XAUTHORITY"));
	environment_.reset(new EnvironmentScope({"XDG_CONFIG_HOME",
		"XDG_CACHE_HOME", "XDG_RUNTIME_DIR", "DISPLAY", "XAUTHORITY",
		"DBUS_SESSION_BUS_ADDRESS"}));
	g_setenv("XDG_CONFIG_HOME", scratch_.c_str(), TRUE);
	g_setenv("XDG_CACHE_HOME", scratch_.c_str(), TRUE);
	g_setenv("XDG_RUNTIME_DIR", scratch_.c_str(), TRUE);
	bus_ = g_test_dbus_new(G_TEST_DBUS_NONE);
	g_test_dbus_up(bus_);
	if (display)
		g_setenv("DISPLAY", display, TRUE);
	if (xauthority)
		g_setenv("XAUTHORITY", xauthority, TRUE);
	g_free(display);
	g_free(xauthority);
	gchar* daemon_argv[] = {const_cast<gchar*>(xfconfd), nullptr};
	GError* error = nullptr;
	const bool spawned = g_spawn_async(nullptr, daemon_argv, nullptr,
			static_cast<GSpawnFlags>(G_SPAWN_DO_NOT_REAP_CHILD |
					G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
			nullptr, nullptr, &xfconfd_pid_, &error);
	if (!spawned || !wait_for_service())
	{
		g_test_message("# SKIP: private xfconfd failed to start%s%s",
			error ? ": " : "", error ? error->message : "");
		g_clear_error(&error);
		stop();
		return false;
	}
	g_clear_error(&error);
	active_ = true;
	return true;
}

bool XfconfFixture::wait_for_service()
{
	GError* error = nullptr;
	GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
	if (!connection)
	{
		g_clear_error(&error);
		return false;
	}
	for (guint attempt = 0; attempt < 50; ++attempt)
	{
		GVariant* result = g_dbus_connection_call_sync(connection,
				"org.freedesktop.DBus", "/org/freedesktop/DBus",
				"org.freedesktop.DBus", "NameHasOwner",
				g_variant_new("(s)", "org.xfce.Xfconf"),
				G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 100,
				nullptr, nullptr);
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
		g_usleep(10000);
	}
	g_object_unref(connection);
	return false;
}

void XfconfFixture::stop_child()
{
	if (!xfconfd_pid_)
		return;
	kill(xfconfd_pid_, SIGTERM);
	int status = 0;
	const gint64 deadline = g_get_monotonic_time() + 500000;
	while (g_get_monotonic_time() < deadline)
	{
		const pid_t result = waitpid(xfconfd_pid_, &status, WNOHANG);
		if (result == xfconfd_pid_)
		{
			xfconfd_pid_ = 0;
			g_spawn_close_pid(result);
			return;
		}
		if (result < 0 && errno != EINTR)
			break;
		g_usleep(10000);
	}
	kill(xfconfd_pid_, SIGKILL);
	while (waitpid(xfconfd_pid_, &status, 0) < 0 && errno == EINTR)
	{
	}
	g_spawn_close_pid(xfconfd_pid_);
	xfconfd_pid_ = 0;
}

void XfconfFixture::stop()
{
	stop_child();
	active_ = false;
	/* Drain proxy finalizers before tearing down the bus they reference. */
	for (guint pass = 0; pass < 100; ++pass)
	{
		while (g_main_context_pending(nullptr))
			g_main_context_iteration(nullptr, FALSE);
		g_usleep(1000);
	}
	if (bus_)
	{
		if (g_test_dbus_get_bus_address(bus_))
			g_test_dbus_down(bus_);
		g_object_unref(bus_);
		bus_ = nullptr;
	}
	environment_.reset();
	if (!scratch_.empty())
	{
		remove_tree(scratch_.c_str());
		scratch_.clear();
	}
}

}
