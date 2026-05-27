/* test_settings_key_inventory:
 *
 * Fail-closed characterization test guarding the Xfconf channel-root
 * literals captured by tests/fixtures/xfconf-keys.txt against silent drift
 * during the structural refactor.
 *
 * The snapshot lists
 * every quoted Xfconf channel-base literal matching the pattern
 *   /plugins/(meowmenu|whiskermenu)-<id>
 * that appears in the source files enumerated by the same contract.
 *
 * What this test does:
 *   1. Reads the snapshot file at runtime.
 *   2. Confirms the artifact is non-empty and contains at least one
 *      meowmenu-<id> base and one whiskermenu-<id> base that share the
 *      same numeric id.
 *   3. Exercises production migrate_legacy_xfconf() (compiled into the
 *      test executable) with the meowmenu base from the snapshot, after
 *      seeding a marker property at the whiskermenu base. Production code
 *      must observe the legacy data and copy it under the meowmenu base.
 *
 * Step 3 indirectly exercises compute_legacy_base() inside migration.cpp;
 * a rename of either channel-root form (or a break of the meowmenu↔
 * whiskermenu derivation) will make the migration miss the seeded marker
 * and fail the test. This is the fail-closed behaviour required by
 * RF-CONFIG-002 and gap-row 3 of spec §12.3.
 *
 * Linking: per research.md §3.10, this test links panel-plugin/
 * migration.cpp directly (same pattern as tests/test_migration.cpp). It
 * does NOT link settings.cpp because the snapshot filter excludes all of
 * settings.cpp's relative key literals; adding settings.cpp would add a
 * heavy dependency surface (plugin.h, command.h, preset.h, search-action.h)
 * without contributing any signal to the drift check.
 *
 * Bus setup: a private GTestDBus session + xfconfd is spawned, mirroring
 * test_migration.cpp. If xfconfd or dbus-daemon is unavailable on the
 * build host the test prints a SKIP marker and exits 0 — the contract
 * behaviour is still exercised in CI where xfconfd is present.
 */

#include "../panel-plugin/migration.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <xfconf/xfconf.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace
{

// ---------------------------------------------------------------------------
// Snapshot path resolution
//
// The absolute path to the snapshot is injected by meson via
// -DMEOWMENU_TEST_XFCONF_SNAPSHOT_PATH so the test binary is independent of
// its working directory. The MEOWMENU_TEST_XFCONF_SNAPSHOT env var still
// overrides the compile-time default; CI matrices use it to point at
// alternate snapshots for negative-control runs.
// ---------------------------------------------------------------------------

#ifndef MEOWMENU_TEST_XFCONF_SNAPSHOT_PATH
#  error "MEOWMENU_TEST_XFCONF_SNAPSHOT_PATH must be defined by the build system"
#endif

std::string resolve_snapshot_path()
{
	const gchar* env = g_getenv("MEOWMENU_TEST_XFCONF_SNAPSHOT");
	if (env != nullptr && env[0] != '\0')
	{
		return std::string(env);
	}
	return std::string(MEOWMENU_TEST_XFCONF_SNAPSHOT_PATH);
}

std::vector<std::string> read_snapshot(const std::string& path)
{
	std::vector<std::string> out;
	std::ifstream f(path);
	std::string line;
	while (std::getline(f, line))
	{
		// Each snapshot line is a quoted Xfconf path literal as
		// produced by `grep -oE '"/[^"]+"'`. Strip the surrounding
		// double quotes; ignore empty / non-conforming lines so the
		// test does not break on a trailing newline.
		if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
		{
			out.push_back(line.substr(1, line.size() - 2));
		}
	}
	return out;
}

struct BasePair
{
	std::string meowmenu;   // e.g. "/plugins/meowmenu-7"
	std::string whiskermenu; // e.g. "/plugins/whiskermenu-7"
};

// pair_bases_by_id:
// @snapshot: every literal extracted from the snapshot file.
//
// Walks the snapshot and matches each "/plugins/meowmenu-<id>" literal with
// its "/plugins/whiskermenu-<id>" counterpart by shared <id> suffix. This is
// snapshot-level inspection only; the test does NOT replicate the production
// derivation in compute_legacy_base — it uses the IDs that already appear in
// the artifact, then lets the production code recompute the legacy base when
// migrate_legacy_xfconf() runs.
//
// Returns: the set of well-formed pairs. An empty vector is a contract
// violation and the caller must abort.
std::vector<BasePair> pair_bases_by_id(const std::vector<std::string>& snapshot)
{
	const std::regex meow_re(R"(^/plugins/meowmenu-(\d+)$)");
	const std::regex whisk_re(R"(^/plugins/whiskermenu-(\d+)$)");

	std::vector<std::pair<std::string, std::string>> meows;
	std::vector<std::pair<std::string, std::string>> whisks;
	for (const std::string& s : snapshot)
	{
		std::smatch m;
		if (std::regex_match(s, m, meow_re))
		{
			meows.emplace_back(m[1].str(), s);
		}
		else if (std::regex_match(s, m, whisk_re))
		{
			whisks.emplace_back(m[1].str(), s);
		}
	}

	std::vector<BasePair> pairs;
	for (const auto& mp : meows)
	{
		for (const auto& wp : whisks)
		{
			if (mp.first == wp.first)
			{
				pairs.push_back({ mp.second, wp.second });
			}
		}
	}
	return pairs;
}

// ---------------------------------------------------------------------------
// Private session bus + xfconfd fixture (mirrors tests/test_migration.cpp).
// ---------------------------------------------------------------------------

GTestDBus* g_test_bus = nullptr;
GPid g_xfconfd_pid = 0;
std::string g_scratch_dir;
int g_unique_counter = 0;

const char* k_xfconfd_candidate_paths[] = {
	"/usr/lib/x86_64-linux-gnu/xfce4/xfconf/xfconfd",
	"/usr/lib/xfce4/xfconf/xfconfd",
	"/usr/libexec/xfconf/xfconfd",
	"/usr/libexec/xfce4/xfconf/xfconfd",
	"/usr/lib64/xfce4/xfconf/xfconfd",
	nullptr,
};

const char* find_xfconfd_binary()
{
	for (const char** p = k_xfconfd_candidate_paths; *p != nullptr; ++p)
	{
		if (g_file_test(*p, G_FILE_TEST_IS_EXECUTABLE))
		{
			return *p;
		}
	}
	gchar* in_path = g_find_program_in_path("xfconfd");
	if (in_path != nullptr)
	{
		// NOTE: leak intentional — process-lifetime constant.
		return in_path;
	}
	return nullptr;
}

bool wait_for_xfconfd_registration()
{
	// HACK: D-Bus auto-activation is not configured for our private bus,
	// so we spawned xfconfd manually. Poll the bus until org.xfce.Xfconf
	// shows up (or give up after 5 s).
	GError* err = nullptr;
	GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
	if (conn == nullptr)
	{
		if (err != nullptr) { g_error_free(err); }
		return false;
	}
	for (int i = 0; i < 50; ++i)
	{
		GVariant* res = g_dbus_connection_call_sync(conn,
				"org.freedesktop.DBus",
				"/org/freedesktop/DBus",
				"org.freedesktop.DBus",
				"NameHasOwner",
				g_variant_new("(s)", "org.xfce.Xfconf"),
				G_VARIANT_TYPE("(b)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1, nullptr, nullptr);
		if (res != nullptr)
		{
			gboolean owned = FALSE;
			g_variant_get(res, "(b)", &owned);
			g_variant_unref(res);
			if (owned)
			{
				g_object_unref(conn);
				return true;
			}
		}
		g_usleep(100 * 1000);
	}
	g_object_unref(conn);
	return false;
}

bool fixture_up()
{
	const char* xfconfd = find_xfconfd_binary();
	if (xfconfd == nullptr)
	{
		std::printf("# SKIP: xfconfd binary not found on this host\n");
		return false;
	}
	gchar* dbus_daemon = g_find_program_in_path("dbus-daemon");
	if (dbus_daemon == nullptr)
	{
		std::printf("# SKIP: dbus-daemon binary not found on this host\n");
		return false;
	}
	g_free(dbus_daemon);

	gchar* tmpl = g_strdup("/tmp/meow-keyinv-test-XXXXXX");
	gchar* dir = g_mkdtemp(tmpl);
	if (dir == nullptr)
	{
		g_free(tmpl);
		std::printf("# SKIP: cannot create scratch dir\n");
		return false;
	}
	g_scratch_dir = dir;
	g_setenv("XDG_CONFIG_HOME", g_scratch_dir.c_str(), TRUE);
	g_setenv("XDG_CACHE_HOME", g_scratch_dir.c_str(), TRUE);
	g_setenv("XDG_RUNTIME_DIR", g_scratch_dir.c_str(), TRUE);
	g_free(tmpl);

	g_test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
	g_test_dbus_up(g_test_bus);

	GError* err = nullptr;
	const gchar* argv[] = { xfconfd, nullptr };
	gboolean ok = g_spawn_async(nullptr,
			const_cast<gchar**>(argv),
			nullptr,
			static_cast<GSpawnFlags>(G_SPAWN_LEAVE_DESCRIPTORS_OPEN
					| G_SPAWN_STDOUT_TO_DEV_NULL
					| G_SPAWN_STDERR_TO_DEV_NULL),
			nullptr, nullptr,
			&g_xfconfd_pid,
			&err);
	if (!ok)
	{
		std::printf("# SKIP: failed to spawn xfconfd: %s\n",
				err != nullptr ? err->message : "(no error)");
		if (err != nullptr) { g_error_free(err); }
		return false;
	}

	if (!wait_for_xfconfd_registration())
	{
		std::printf("# SKIP: xfconfd did not register on the private bus\n");
		return false;
	}

	if (!xfconf_init(&err))
	{
		std::printf("# SKIP: xfconf_init failed: %s\n",
				err != nullptr ? err->message : "(no error)");
		if (err != nullptr) { g_error_free(err); }
		return false;
	}
	return true;
}

void fixture_down()
{
	xfconf_shutdown();
	if (g_xfconfd_pid != 0)
	{
		kill(g_xfconfd_pid, SIGTERM);
		g_spawn_close_pid(g_xfconfd_pid);
		g_xfconfd_pid = 0;
	}
	if (g_test_bus != nullptr)
	{
		g_test_dbus_down(g_test_bus);
		g_object_unref(g_test_bus);
		g_test_bus = nullptr;
	}
}

std::string unique_channel_name()
{
	++g_unique_counter;
	gchar* s = g_strdup_printf("meow-keyinv-test-%d-%d",
			static_cast<int>(g_get_real_time() & 0x7fffffff),
			g_unique_counter);
	std::string r(s);
	g_free(s);
	return r;
}

// ---------------------------------------------------------------------------
// Assertions
// ---------------------------------------------------------------------------

void test_snapshot_well_formed(const std::vector<BasePair>& pairs)
{
	// At least one (meowmenu, whiskermenu) pair with matching id must
	// appear in the snapshot. An empty pair list means either the
	// artifact was lost or the schema literals were renamed without an
	// authorising spec — either is a contract violation per RF-CONFIG-002.
	assert(!pairs.empty()
		&& "snapshot is missing the meowmenu/whiskermenu channel-root pair");
}

void test_production_migration_pairs_match_snapshot(const BasePair& pair)
{
	// Use a unique channel so successive pairs cannot collide on the bus.
	const std::string channel_name = unique_channel_name();
	XfconfChannel* channel = xfconf_channel_new(channel_name.c_str());
	assert(channel != nullptr);

	// Ensure migrate_legacy_xfconf() does NOT short-circuit on the
	// Whisker-present heuristic; we want to observe the legacy copy path.
	g_setenv("MEOWMENU_TEST_WHISKER_PRESENT", "0", TRUE);

	// Seed a marker at the whiskermenu base from the snapshot.
	const std::string marker_key = pair.whiskermenu + "/test-key-inventory-marker";
	xfconf_channel_set_string(channel, marker_key.c_str(), "present");

	// Run the production migration with the meowmenu base from the
	// snapshot. If compute_legacy_base() inside migration.cpp still maps
	// the snapshot's meowmenu literal to the snapshot's whiskermenu
	// literal, the seeded marker will be visible under the meowmenu base.
	const bool ran = WhiskerMenu::migrate_legacy_xfconf(channel,
			pair.meowmenu.c_str());
	assert(ran && "migrate_legacy_xfconf reported no-op for snapshot bases");

	const std::string copied_key = pair.meowmenu + "/test-key-inventory-marker";
	gchar* copied = xfconf_channel_get_string(channel, copied_key.c_str(), nullptr);
	assert(copied != nullptr
		&& "production migration did not copy seeded marker — channel-root pair drift");
	assert(std::strcmp(copied, "present") == 0);
	g_free(copied);

	// Sentinel must have been written under the meowmenu base.
	const std::string sentinel = pair.meowmenu + "/migration/legacy-imported";
	assert(xfconf_channel_has_property(channel, sentinel.c_str())
		&& "migration sentinel missing after successful run");

	g_object_unref(channel);
}

} // anonymous namespace

int main(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	const std::string snapshot_path = resolve_snapshot_path();
	if (!g_file_test(snapshot_path.c_str(), G_FILE_TEST_EXISTS))
	{
		std::fprintf(stderr,
			"FAIL: xfconf-keys snapshot not found at %s\n",
			snapshot_path.c_str());
		return 1;
	}

	const std::vector<std::string> snapshot = read_snapshot(snapshot_path);
	assert(!snapshot.empty() && "snapshot file is empty");

	const std::vector<BasePair> pairs = pair_bases_by_id(snapshot);
	test_snapshot_well_formed(pairs);

	if (!fixture_up())
	{
		fixture_down();
		// Skip cleanly when xfconfd/dbus is unavailable. The drift check
		// against the artifact still runs (above) and would have aborted
		// the process if the snapshot file was lost or malformed.
		return 0;
	}

	for (const BasePair& pair : pairs)
	{
		test_production_migration_pairs_match_snapshot(pair);
	}

	fixture_down();

	std::printf("OK: settings_key_inventory drift check passed (%zu pair(s))\n",
			pairs.size());
	return 0;
}
