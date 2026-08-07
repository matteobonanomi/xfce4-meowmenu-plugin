/*
 * Real integration tests for the Settings-free preset data layer, exercised
 * against a live xfconfd on a private session bus.
 *
 * Scope and rationale:
 *   The save / rename / delete / import / export entry points all take a
 *   Settings& and reach the full Plugin object graph, so they cannot be linked
 *   into a unit test without the entire launcher. The PUBLIC, Settings-free
 *   functions — enumerate_user_presets(XfconfChannel*), find_preset_by_id(), and
 *   enumerate_preset_files()/its seeded-file parser — are linked directly here
 *   (with --gc-sections dropping the Settings-coupled siblings) and run against
 *   a real channel and real on-disk files.
 *
 *   These are precisely the read-side surfaces behind the two behaviours this
 *   feature repairs:
 *     - the "Save as new… does not surface the new preset" defect: after a
 *       save-style write to /presets/<uuid>/, enumerate_user_presets() and
 *       find_preset_by_id() must surface that uuid (R3 hypotheses H2/H3).
 *     - schema-lenient seeded-file import (supported behavior): a .meowpreset with a newer
 *       SchemaVersion must be accepted best-effort rather than skipped.
 *
 * If xfconfd / dbus-daemon are unavailable the test prints a TAP SKIP and
 * returns 0, matching test_migration.cpp. The project CI containers ship
 * xfconfd, so the assertions run there.
 */

#include "presets/preset.h"
#include "presets/preset-io.h"
#include "settings-defaults.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <xfconf/xfconf.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

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
		if (g_file_test(*p, G_FILE_TEST_IS_EXECUTABLE))
			return *p;
	gchar* in_path = g_find_program_in_path("xfconfd");
	if (in_path != nullptr)
		return in_path; // NOTE: leak intentional — process-lifetime constant.
	return nullptr;
}

bool wait_for_xfconfd_registration()
{
	// HACK: D-Bus auto-activation is not configured for the private bus, so
	// xfconfd was spawned manually; poll until org.xfce.Xfconf appears (5 s cap).
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
			"org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
			"NameHasOwner", g_variant_new("(s)", "org.xfce.Xfconf"),
			G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
		if (res != nullptr)
		{
			gboolean owned = FALSE;
			g_variant_get(res, "(b)", &owned);
			g_variant_unref(res);
			if (owned) { g_object_unref(conn); return true; }
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

	gchar* tmpl = g_strdup("/tmp/meow-preset-xfconf-XXXXXX");
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
	gboolean ok = g_spawn_async(nullptr, const_cast<gchar**>(argv), nullptr,
		static_cast<GSpawnFlags>(G_SPAWN_LEAVE_DESCRIPTORS_OPEN
			| G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
		nullptr, nullptr, &g_xfconfd_pid, &err);
	if (!ok)
	{
		std::printf("# SKIP: failed to spawn xfconfd: %s\n", err ? err->message : "(no error)");
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
		std::printf("# SKIP: xfconf_init failed: %s\n", err ? err->message : "(no error)");
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

XfconfChannel* fresh_channel()
{
	++g_unique_counter;
	gchar* name = g_strdup_printf("meow-preset-test-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), g_unique_counter);
	XfconfChannel* ch = xfconf_channel_new(name);
	g_free(name);
	return ch;
}

// Write a /presets/<uuid>/ subtree exactly the way save_current_as_user_preset()
// does (display-name + identity name + a representative set of governed values).
void seed_saved_preset(XfconfChannel* ch, const std::string& uuid,
	const std::string& display_name)
{
	const std::string prefix = "/presets/" + uuid + "/";
	xfconf_channel_set_string(ch, (prefix + "display-name").c_str(), display_name.c_str());
	xfconf_channel_set_string(ch, (prefix + "name").c_str(), display_name.c_str());
	xfconf_channel_set_string(ch, (prefix + "created-by").c_str(), "meowmenu-test");
	xfconf_channel_set_int(ch, (prefix + "corner-radius").c_str(), 7);
	xfconf_channel_set_string(ch, (prefix + "sidebar-position").c_str(), "right");
	xfconf_channel_set_bool(ch, (prefix + "sidebar-enabled").c_str(), TRUE);
}

const WhiskerMenu::LayoutPreset* find_in(const std::vector<WhiskerMenu::LayoutPreset>& v,
	const std::string& id)
{
	for (const auto& p : v)
		if (p.id == id)
			return &p;
	return nullptr;
}

// ---------------------------------------------------------------------------
// runtime implementation: save-refresh regression lock (read side).
// After a save-style write, the enumerated set and find_preset_by_id() must both
// surface a row whose id equals the saved uuid, with the stored name. This is the
// behaviour the "Save as new…" dropdown refresh depends on (supported behavior).
// ---------------------------------------------------------------------------

void test_enumerate_surfaces_saved_uuid()
{
	XfconfChannel* ch = fresh_channel();
	const std::string uuid = "aabbccdd00112233";
	seed_saved_preset(ch, uuid, "My Layout");

	const auto& presets = WhiskerMenu::enumerate_user_presets(ch);
	const WhiskerMenu::LayoutPreset* p = find_in(presets, uuid);
	assert(p != nullptr);                       // the saved uuid is enumerated
	assert(p->name == "My Layout");
	assert(p->display_name == "My Layout");
	assert(!p->is_builtin);
	assert(p->values.find("corner-radius") != p->values.end());
	assert(p->values.at("corner-radius").kind == WhiskerMenu::PresetValue::Int);
	assert(p->values.at("corner-radius").i == 7);

	// find_preset_by_id() reads the freshly-enumerated cache and resolves it too.
	const WhiskerMenu::LayoutPreset* byid = WhiskerMenu::find_preset_by_id(uuid);
	assert(byid != nullptr);
	assert(byid->id == uuid);

	g_object_unref(ch);
}

// Regression lock for the real root cause: the live plugin's Settings channel is
// created with a property base (xfconf_channel_new_with_property_base), and for
// such channels xfconf_channel_get_properties() returns FULL paths that include
// the base — "/plugins/<plugin>/presets/<uuid>/<key>", not "/presets/<uuid>/…".
// enumerate_user_presets() must still surface those presets; the original code
// anchored on a leading "/presets/" and silently dropped every one, so saved
// presets never appeared in the dropdown.
void test_enumerate_with_property_base_channel()
{
	++g_unique_counter;
	gchar* name = g_strdup_printf("meow-preset-base-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), g_unique_counter);
	XfconfChannel* ch = xfconf_channel_new_with_property_base(name, "/plugins/meowmenu-test");
	g_free(name);

	const std::string uuid = "facefeed12345678";
	// Writes are base-relative; they land at "/plugins/meowmenu-test/presets/…".
	seed_saved_preset(ch, uuid, "Based Layout");

	const auto& presets = WhiskerMenu::enumerate_user_presets(ch);
	const WhiskerMenu::LayoutPreset* p = find_in(presets, uuid);
	assert(p != nullptr);                 // must be found despite the base prefix
	assert(p->name == "Based Layout");
	assert(p->values.at("corner-radius").i == 7);

	const WhiskerMenu::LayoutPreset* byid = WhiskerMenu::find_preset_by_id(uuid);
	assert(byid != nullptr && byid->id == uuid);

	g_object_unref(ch);
}

// A preset row lacking display-name is invalid and dropped (R3 H2). This is why
// the save path must write display-name — documented here as a behavioural lock.
void test_enumerate_drops_missing_display_name()
{
	XfconfChannel* ch = fresh_channel();
	const std::string uuid = "deadbeefcafef00d";
	const std::string prefix = "/presets/" + uuid + "/";
	xfconf_channel_set_int(ch, (prefix + "corner-radius").c_str(), 3); // no display-name

	const auto& presets = WhiskerMenu::enumerate_user_presets(ch);
	assert(find_in(presets, uuid) == nullptr);
	g_object_unref(ch);
}

// A pre-v5 custom preset that stored only display-name (no identity "name")
// surfaces with name falling back to display_name.
void test_enumerate_name_falls_back_to_display_name()
{
	XfconfChannel* ch = fresh_channel();
	const std::string uuid = "0123456789abcdef";
	const std::string prefix = "/presets/" + uuid + "/";
	xfconf_channel_set_string(ch, (prefix + "display-name").c_str(), "Legacy");
	xfconf_channel_set_int(ch, (prefix + "corner-radius").c_str(), 5);

	const auto& presets = WhiskerMenu::enumerate_user_presets(ch);
	const WhiskerMenu::LayoutPreset* p = find_in(presets, uuid);
	assert(p != nullptr);
	assert(p->name == "Legacy");
	g_object_unref(ch);
}

/* test_upgrade_baseline_enumeration_is_idempotent:
 *
 * A saved custom preset representing the 0.8.0 upgrade baseline must retain
 * its identity and values across repeated reads, matching the second-pass
 * migration check in the release walkthrough.
 */
void test_upgrade_baseline_enumeration_is_idempotent()
{
	XfconfChannel* ch = fresh_channel();
	const std::string uuid = "rc-upgrade-baseline";
	seed_saved_preset(ch, uuid, "RC Upgrade Baseline");

	for (int pass = 0; pass < 2; ++pass)
	{
		const auto& presets = WhiskerMenu::enumerate_user_presets(ch);
		const WhiskerMenu::LayoutPreset* preset = find_in(presets, uuid);
		assert(preset != nullptr);
		assert(preset->display_name == "RC Upgrade Baseline");
		assert(preset->values.at("corner-radius").i == 7);
		assert(preset->values.at("sidebar-position").s == "right");
	}

	g_object_unref(ch);
}

// ---------------------------------------------------------------------------
// runtime implementation: reset-to-defaults scope (supported behavior).
// The "Reset to defaults" control must clear every non-preset property while
// preserving saved user presets under /presets/<uuid>/. The live plugin's
// channel is property-base-anchored, and get_properties() returns FULL paths
// that embed that base; reset_property() on a based channel expects a
// base-relative path, so a full path double-prefixes and silently no-ops. This
// locks the full-path/base-relative regression the same way the enumerate test
// does for the read side.
// ---------------------------------------------------------------------------

void test_reset_preserves_presets_clears_rest()
{
	const std::string base = "/plugins/meowmenu-reset-test";
	++g_unique_counter;
	gchar* name = g_strdup_printf("meow-preset-reset-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), g_unique_counter);
	XfconfChannel* ch = xfconf_channel_new_with_property_base(name, base.c_str());
	g_free(name);

	// A saved preset (must survive) plus a representative spread of non-preset
	// keys (must be cleared): a scalar, a favourites entry, and a custom
	// search-action key — the exact families the old handler failed to reset.
	const std::string uuid = "0bada55c0ffee123";
	seed_saved_preset(ch, uuid, "Keep Me");
	xfconf_channel_set_int(ch, "/menu-width", 640);
	xfconf_channel_set_string(ch, "/favorites/0", "firefox.desktop");
	xfconf_channel_set_string(ch, "/search-actions/0/name", "Web Search");
	// A user who opted into Centered must be returned to the docked default by a
	// reset (supported behavior): the stored value is cleared, so a read falls back to the
	// "docked" schema default.
	xfconf_channel_set_string(ch, "/layout-mode", "centered");
	// A user who reduced the menu opacity must be returned to fully opaque by a
	// reset (042 supported behavior): the stored value is cleared, so a read falls back to
	// the 100 default. This is the only headless guard against a future
	// default-value or reset-list regression for /menu-opacity.
	xfconf_channel_set_int(ch, "/menu-opacity", 60);

	// Sanity: everything is present before the reset.
	assert(xfconf_channel_has_property(ch, "/menu-width"));
	assert(xfconf_channel_has_property(ch, "/favorites/0"));
	assert(xfconf_channel_has_property(ch, "/search-actions/0/name"));
	assert(xfconf_channel_has_property(ch, "/layout-mode"));
	assert(xfconf_channel_has_property(ch, "/menu-opacity"));
	assert(xfconf_channel_has_property(ch,
		("/presets/" + uuid + "/name").c_str()));

	int reset_count = WhiskerMenu::reset_settings_to_defaults(ch, base);
	assert(reset_count >= 5); // at least the five non-preset keys above

	// Non-preset keys are gone; the saved preset subtree is untouched.
	assert(!xfconf_channel_has_property(ch, "/menu-width"));
	assert(!xfconf_channel_has_property(ch, "/favorites/0"));
	assert(!xfconf_channel_has_property(ch, "/search-actions/0/name"));
	// /layout-mode is cleared, so a read now yields the "docked" default.
	assert(!xfconf_channel_has_property(ch, "/layout-mode"));
	{
		gchar* lm = xfconf_channel_get_string(ch, "/layout-mode", "docked");
		assert(lm && g_strcmp0(lm, "docked") == 0);
		g_free(lm);
	}
	// /menu-opacity is cleared, so a read now yields the fully-opaque 100 default.
	assert(!xfconf_channel_has_property(ch, "/menu-opacity"));
	assert(xfconf_channel_get_int(ch, "/menu-opacity", 100) == 100);
	assert(xfconf_channel_has_property(ch,
		("/presets/" + uuid + "/name").c_str()));
	assert(xfconf_channel_has_property(ch,
		("/presets/" + uuid + "/corner-radius").c_str()));

	// And the preserved preset still enumerates with its stored values.
	const auto& presets = WhiskerMenu::enumerate_user_presets(ch);
	const WhiskerMenu::LayoutPreset* p = find_in(presets, uuid);
	assert(p != nullptr);
	assert(p->name == "Keep Me");
	assert(p->values.at("corner-radius").i == 7);

	g_object_unref(ch);
}

void test_automatic_reset_is_bounded_and_destructive()
{
	const std::string base = "/plugins/plugin-61";
	gchar* name = g_strdup_printf("meow-upgrade-reset-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), ++g_unique_counter);
	XfconfChannel* ch = xfconf_channel_new_with_property_base(name, base.c_str());
	g_free(name);

	xfconf_channel_set_bool(ch, "/initialized", TRUE);
	xfconf_channel_set_int(ch, "/corner-radius", 17);
	seed_saved_preset(ch, "old-layout", "Old Layout");

	assert(WhiskerMenu::reset_instance_for_composition_upgrade(ch, base));
	assert(!xfconf_channel_has_property(ch, "/initialized"));
	assert(!xfconf_channel_has_property(ch, "/corner-radius"));
	assert(!xfconf_channel_has_property(ch, "/presets/old-layout/name"));
	assert(xfconf_channel_get_int(ch,
		WhiskerMenu::COMPOSITION_RESET_GENERATION_KEY, 0)
		== WhiskerMenu::COMPOSITION_RESET_GENERATION);
	gchar* state = xfconf_channel_get_string(ch,
		WhiskerMenu::COMPOSITION_RESET_STATE_KEY, nullptr);
	assert(state && std::strcmp(state, "pending") == 0);
	g_free(state);

	assert(!WhiskerMenu::reset_instance_for_composition_upgrade(ch, ""));
	assert(!WhiskerMenu::reset_instance_for_composition_upgrade(ch,
		"/plugins/meowmenu-all"));
	assert(!WhiskerMenu::reset_instance_for_composition_upgrade(ch,
		"/plugins/plugin-all"));
	g_object_unref(ch);
}

void seed_required_modern_state(XfconfChannel* channel)
{
	xfconf_channel_set_bool(channel, "/initialized", TRUE);
	xfconf_channel_set_int(channel, "/schema-version", 13);
	xfconf_channel_set_string(channel, "/current-preset-id", "modern");
	xfconf_channel_set_string(channel, "/layout-mode", "docked");
	xfconf_channel_set_string(channel, "/search-bar-position", "top");
	xfconf_channel_set_string(channel, "/sidebar-position", "left");
}

void test_reset_lifecycle_is_per_instance_and_completion_last()
{
	using WhiskerMenu::PreStableResetDecision;
	const std::string base_a = "/plugins/plugin-71";
	const std::string base_b = "/plugins/plugin-72";
	gchar* name_raw = g_strdup_printf("meow-reset-matrix-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), ++g_unique_counter);
	const std::string name(name_raw);
	g_free(name_raw);

	XfconfChannel* root = xfconf_channel_new(name.c_str());
	XfconfChannel* a = xfconf_channel_new_with_property_base(name.c_str(), base_a.c_str());
	XfconfChannel* b = xfconf_channel_new_with_property_base(name.c_str(), base_b.c_str());
	xfconf_channel_set_int(root, base_a.c_str(), 61);
	xfconf_channel_set_bool(a, "/initialized", TRUE);
	xfconf_channel_set_int(a, "/corner-radius", 19);
	xfconf_channel_set_bool(b, "/initialized", TRUE);
	xfconf_channel_set_int(b, "/corner-radius", 7);

	assert(WhiskerMenu::inspect_pre_stable_reset(a, base_a.c_str())
		== PreStableResetDecision::Reset);
	assert(WhiskerMenu::reset_instance_for_composition_upgrade(a, base_a));
	assert(xfconf_channel_has_property(root, base_a.c_str()));
	assert(xfconf_channel_get_int(b, "/corner-radius", 0) == 7);
	assert(WhiskerMenu::inspect_pre_stable_reset(a, base_a.c_str())
		== PreStableResetDecision::Reset);

	seed_required_modern_state(a);
	assert(WhiskerMenu::complete_pre_stable_reset(a));
	assert(WhiskerMenu::inspect_pre_stable_reset(a, base_a.c_str())
		== PreStableResetDecision::Load);
	xfconf_channel_set_int(a, "/corner-radius", 9);
	assert(WhiskerMenu::inspect_pre_stable_reset(a, base_a.c_str())
		== PreStableResetDecision::Load);
	assert(xfconf_channel_get_int(a, "/corner-radius", 0) == 9);

	assert(WhiskerMenu::inspect_pre_stable_reset(b, base_b.c_str())
		== PreStableResetDecision::Reset);
	assert(xfconf_channel_get_int(b, "/corner-radius", 0) == 7);

	XfconfChannel* fresh = xfconf_channel_new_with_property_base(name.c_str(),
		"/plugins/plugin-73");
	assert(WhiskerMenu::inspect_pre_stable_reset(fresh, "/plugins/plugin-73")
		== PreStableResetDecision::Fresh);
	seed_required_modern_state(fresh);
	assert(WhiskerMenu::complete_pre_stable_reset(fresh));
	assert(WhiskerMenu::inspect_pre_stable_reset(fresh, "/plugins/plugin-73")
		== PreStableResetDecision::Load);

	assert(WhiskerMenu::inspect_pre_stable_reset(a, "/plugins/meowmenu-all")
		== PreStableResetDecision::Load);
	g_object_unref(fresh);
	g_object_unref(b);
	g_object_unref(a);
	g_object_unref(root);
}

// ---------------------------------------------------------------------------
// runtime implementation: schema-lenient seeded-file import (supported behavior).
// enumerate_preset_files() must accept a .meowpreset carrying a newer
// SchemaVersion best-effort, while still skipping unparseable/section-missing
// files. Built-in identity comes from the [Preset].Name key.
// ---------------------------------------------------------------------------

std::string write_meowpreset(const std::string& dir, const std::string& stem,
	const std::string& body)
{
	const std::string path = dir + G_DIR_SEPARATOR_S + stem + ".meowpreset";
	GError* err = nullptr;
	gboolean ok = g_file_set_contents(path.c_str(), body.c_str(), -1, &err);
	if (!ok)
	{
		if (err) g_error_free(err);
		assert(false && "failed to write fixture preset");
	}
	return path;
}

void test_seeded_file_accepts_newer_schema()
{
	gchar* tmpl = g_strdup("/tmp/meow-preset-files-XXXXXX");
	gchar* dir_c = g_mkdtemp(tmpl);
	assert(dir_c != nullptr);
	std::string dir(dir_c);

	// Newer schema version than we know — must be accepted best-effort (supported behavior).
	write_meowpreset(dir, "future",
		"[Preset]\nName=Future\nSchemaVersion=99\n\n"
		"[Settings]\ncorner-radius=4\nsidebar-position=left\n");
	// A current-version file alongside it.
	write_meowpreset(dir, "present",
		"[Preset]\nName=Present\nSchemaVersion=1\n\n"
		"[Settings]\ncorner-radius=8\n");
	// An unparseable file must still be skipped.
	write_meowpreset(dir, "broken", "this is not a key file {{{");

	std::vector<WhiskerMenu::LayoutPreset> presets =
		WhiskerMenu::enumerate_preset_files(dir, dir);

	const WhiskerMenu::LayoutPreset* fut = find_in(presets, "future");
	assert(fut != nullptr);                     // newer SchemaVersion accepted
	assert(fut->display_name == "Future");
	assert(fut->is_builtin);
	assert(fut->values.at("corner-radius").i == 4);

	assert(find_in(presets, "present") != nullptr);
	assert(find_in(presets, "broken") == nullptr); // unparseable skipped

	g_free(tmpl);
}

void test_seeded_file_skips_section_missing()
{
	gchar* tmpl = g_strdup("/tmp/meow-preset-files-XXXXXX");
	gchar* dir_c = g_mkdtemp(tmpl);
	assert(dir_c != nullptr);
	std::string dir(dir_c);

	// Missing the required [Settings] section → skipped.
	write_meowpreset(dir, "nosettings", "[Preset]\nName=NoSettings\nSchemaVersion=1\n");
	// Missing [Preset].Name → skipped.
	write_meowpreset(dir, "noname", "[Preset]\nSchemaVersion=1\n\n[Settings]\ncorner-radius=2\n");

	std::vector<WhiskerMenu::LayoutPreset> presets =
		WhiskerMenu::enumerate_preset_files(dir, dir);
	assert(find_in(presets, "nosettings") == nullptr);
	assert(find_in(presets, "noname") == nullptr);

	g_free(tmpl);
}

void test_incompatible_user_overlay_cannot_shadow_supported_preset()
{
	gchar* system_template = g_strdup("/tmp/meow-system-presets-XXXXXX");
	gchar* user_template = g_strdup("/tmp/meow-user-presets-XXXXXX");
	gchar* system_dir_raw = g_mkdtemp(system_template);
	gchar* user_dir_raw = g_mkdtemp(user_template);
	assert(system_dir_raw && user_dir_raw);
	const std::string system_dir(system_dir_raw);
	const std::string user_dir(user_dir_raw);

	write_meowpreset(system_dir, "modern",
		"[Preset]\nName=Modern\n\n[Settings]\ncorner-radius=4\n"
		"sidebar-position=left\nshow-profile=true\nshow-session=true\n");
	write_meowpreset(user_dir, "modern",
		"[Preset]\nName=Obsolete override\n\n[Settings]\ncorner-radius=21\n"
		"profile-position=bottom-left\n");
	write_meowpreset(user_dir, "old-horizontal",
		"[Preset]\nName=Old Horizontal\n\n[Settings]\nsidebar-position=bottom\n");

	const auto presets = WhiskerMenu::enumerate_preset_files(system_dir, user_dir);
	const WhiskerMenu::LayoutPreset* modern = find_in(presets, "modern");
	assert(modern);
	assert(modern->display_name == "Modern");
	assert(modern->values.at("corner-radius").i == 4);
	assert(find_in(presets, "old-horizontal") == nullptr);

	g_free(system_template);
	g_free(user_template);
}

} // anonymous namespace

int main()
{
	if (!fixture_up())
	{
		fixture_down();
		return 0; // clean SKIP when xfconfd/private bus unavailable
	}

	test_enumerate_surfaces_saved_uuid();
	test_enumerate_with_property_base_channel();
	test_enumerate_drops_missing_display_name();
	test_enumerate_name_falls_back_to_display_name();
	test_upgrade_baseline_enumeration_is_idempotent();
	test_reset_preserves_presets_clears_rest();
	test_automatic_reset_is_bounded_and_destructive();
	test_reset_lifecycle_is_per_instance_and_completion_last();
	test_seeded_file_accepts_newer_schema();
	test_seeded_file_skips_section_missing();
	test_incompatible_user_overlay_cannot_shadow_supported_preset();

	fixture_down();

	std::printf("OK: preset xfconf integration tests passed\n");
	return 0;
}
