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
#include "launcher/favorite-projection.h"
#include "settings-defaults.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <xfconf/xfconf.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef MEOWMENU_RC_UPGRADE_FIXTURE
#error "MEOWMENU_RC_UPGRADE_FIXTURE must be defined"
#endif
#ifndef MEOWMENU_AFFECTED_UPGRADE_FIXTURE
#error "MEOWMENU_AFFECTED_UPGRADE_FIXTURE must be defined"
#endif

namespace
{

GTestDBus* g_test_bus = nullptr;
GPid g_xfconfd_pid = 0;
std::string g_scratch_dir;
int g_unique_counter = 0;

std::string write_meowpreset(const std::string& dir, const std::string& stem,
	const std::string& content);

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

struct FixtureNode
{
	std::string path;
	std::string type;
	GPtrArray* values = nullptr;
};

struct FixtureLoader
{
	XfconfChannel* channel;
	std::vector<FixtureNode> nodes;
};

const char* attribute_value(const gchar** names, const gchar** values,
	const char* wanted)
{
	if (!names || !values)
		return nullptr;
	for (int i = 0; names[i] && values[i]; ++i)
		if (std::strcmp(names[i], wanted) == 0)
			return values[i];
	return nullptr;
}

GValue* fixture_value(const char* type, const char* value)
{
	GValue* result = g_new0(GValue, 1);
	if (g_strcmp0(type, "string") == 0)
	{
		g_value_init(result, G_TYPE_STRING);
		g_value_set_string(result, value ? value : "");
	}
	else if (g_strcmp0(type, "int") == 0)
	{
		g_value_init(result, G_TYPE_INT);
		g_value_set_int(result, static_cast<int>(g_ascii_strtoll(value, nullptr, 10)));
	}
	else if (g_strcmp0(type, "bool") == 0)
	{
		g_value_init(result, G_TYPE_BOOLEAN);
		g_value_set_boolean(result, g_strcmp0(value, "true") == 0);
	}
	else
	{
		g_free(result);
		return nullptr;
	}
	return result;
}

void fixture_start_element(GMarkupParseContext*, const gchar* element,
	const gchar** names, const gchar** values, gpointer user_data, GError** error)
{
	FixtureLoader* loader = static_cast<FixtureLoader*>(user_data);
	if (std::strcmp(element, "property") == 0)
	{
		const char* name = attribute_value(names, values, "name");
		const char* type = attribute_value(names, values, "type");
		if (!name || !type)
		{
			g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"property requires name and type");
			return;
		}

		FixtureNode node;
		node.path = loader->nodes.empty()
			? "/" + std::string(name)
			: loader->nodes.back().path + "/" + name;
		node.type = type;
		if (node.type == "array")
			node.values = g_ptr_array_new();
		loader->nodes.push_back(node);

		const char* value = attribute_value(names, values, "value");
		GValue* scalar = fixture_value(type, value);
		if (scalar)
		{
			xfconf_channel_set_property(loader->channel,
				loader->nodes.back().path.c_str(), scalar);
			g_value_unset(scalar);
			g_free(scalar);
		}
	}
	else if (std::strcmp(element, "value") == 0)
	{
		if (loader->nodes.empty() || !loader->nodes.back().values)
		{
			g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				"value outside array property");
			return;
		}
		GValue* value = fixture_value(attribute_value(names, values, "type"),
			attribute_value(names, values, "value"));
		if (!value)
		{
			g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
				"unsupported fixture value type");
			return;
		}
		g_ptr_array_add(loader->nodes.back().values, value);
	}
}

void fixture_end_element(GMarkupParseContext*, const gchar* element,
	gpointer user_data, GError**)
{
	FixtureLoader* loader = static_cast<FixtureLoader*>(user_data);
	if (std::strcmp(element, "property") != 0 || loader->nodes.empty())
		return;
	FixtureNode& node = loader->nodes.back();
	if (node.values)
	{
		xfconf_channel_set_arrayv(loader->channel, node.path.c_str(), node.values);
		xfconf_array_free(node.values);
	}
	loader->nodes.pop_back();
}

/* load_xfconf_fixture:
 * @channel: base-less private Xfconf channel receiving the fixture.
 * @path: absolute path to an xfconf-query-compatible XML snapshot.
 *
 * Loads scalar and array properties through the real Xfconf API. Parent
 * properties only provide paths; the private daemon owns the resulting typed
 * values exactly as it would after an xfconf-query import.
 */
void load_xfconf_fixture(XfconfChannel* channel, const char* path)
{
	gchar* xml = nullptr;
	gsize length = 0;
	GError* error = nullptr;
	assert(g_file_get_contents(path, &xml, &length, &error));
	assert(error == nullptr);

	FixtureLoader loader = { channel, {} };
	const GMarkupParser parser = {
		fixture_start_element,
		fixture_end_element,
		nullptr,
		nullptr,
		nullptr,
	};
	GMarkupParseContext* context = g_markup_parse_context_new(&parser,
		G_MARKUP_TREAT_CDATA_AS_TEXT, &loader, nullptr);
	assert(g_markup_parse_context_parse(context, xml, length, &error));
	assert(g_markup_parse_context_end_parse(context, &error));
	assert(error == nullptr);
	assert(loader.nodes.empty());
	g_markup_parse_context_free(context);
	g_free(xml);
}

std::string value_text(const GValue* value)
{
	if (G_VALUE_HOLDS_STRING(value))
		return "s:" + std::string(g_value_get_string(value));
	if (G_VALUE_HOLDS_INT(value))
		return "i:" + std::to_string(g_value_get_int(value));
	if (G_VALUE_HOLDS_BOOLEAN(value))
		return g_value_get_boolean(value) ? "b:true" : "b:false";
	if (G_VALUE_HOLDS(value, G_TYPE_PTR_ARRAY))
	{
		std::string result = "a:[";
		const GPtrArray* values = static_cast<const GPtrArray*>(g_value_get_boxed(value));
		for (guint i = 0; values && i < values->len; ++i)
		{
			if (i)
				result += ",";
			result += value_text(static_cast<const GValue*>(g_ptr_array_index(values, i)));
		}
		return result + "]";
	}
	gchar* rendered = g_strdup_value_contents(value);
	const std::string result = rendered ? rendered : "";
	g_free(rendered);
	return result;
}

std::vector<std::string> property_snapshot(XfconfChannel* channel,
	const std::vector<std::string>& prefixes = {})
{
	std::vector<std::string> result;
	GHashTable* properties = xfconf_channel_get_properties(channel, nullptr);
	if (!properties)
		return result;
	GHashTableIter iter;
	gpointer key = nullptr;
	gpointer value = nullptr;
	g_hash_table_iter_init(&iter, properties);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		const std::string path(static_cast<const gchar*>(key));
		bool selected = prefixes.empty();
		for (const std::string& prefix : prefixes)
			selected = selected || path.find(prefix) != std::string::npos;
		if (selected)
			result.push_back(path + "=" + value_text(static_cast<const GValue*>(value)));
	}
	g_hash_table_unref(properties);
	std::sort(result.begin(), result.end());
	return result;
}

std::vector<std::string> durable_snapshot(XfconfChannel* channel)
{
	return property_snapshot(channel, {
		"/favorites", "/recent", "/search/", "/search-actions",
		"/usage/", "/command-", "/show-command-", "/presets/",
		"/migration/composition-reset-",
	});
}

std::vector<std::string> string_array(XfconfChannel* channel, const char* key)
{
	std::vector<std::string> result;
	GPtrArray* values = xfconf_channel_get_arrayv(channel, key);
	for (guint i = 0; values && i < values->len; ++i)
	{
		const GValue* value = static_cast<const GValue*>(g_ptr_array_index(values, i));
		if (G_VALUE_HOLDS_STRING(value))
			result.emplace_back(g_value_get_string(value));
	}
	if (values)
		xfconf_array_free(values);
	return result;
}

void set_string_array(XfconfChannel* channel, const char* key,
		const std::vector<std::string>& strings)
{
	GPtrArray* values = g_ptr_array_new();
	for (const std::string& string : strings)
	{
		GValue* value = g_new0(GValue, 1);
		g_value_init(value, G_TYPE_STRING);
		g_value_set_string(value, string.c_str());
		g_ptr_array_add(values, value);
	}
	assert(xfconf_channel_set_arrayv(channel, key, values));
	xfconf_array_free(values);
}

/* run_bounded_upgrade_pass:
 * @channel: property-base-anchored private Xfconf channel.
 *
 * Runs the real schema-v13 cleanup when required, then advances the same schema
 * key that Settings advances after synchronizing its in-memory fields. Current
 * profiles take the no-op path and legacy reset markers remain untouched.
 */
void run_bounded_upgrade_pass(XfconfChannel* channel)
{
	if (xfconf_channel_get_int(channel, "/schema-version", 0) >= 13)
		return;
	WhiskerMenu::migrate_layout_schema_v13(channel);
	xfconf_channel_set_int(channel, "/schema-version", 13);
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
	assert(!p->identity_localizable);
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

/* test_panel_registration_is_not_fresh_profile_state:
 *
 * Reproduces the real xfce4-panel channel shape: the plugin base is a string
 * registration property while launcher settings, when present, are children.
 * Property-base enumeration includes both, but only descendants make the
 * launcher profile non-empty for first-run preset selection.
 */
void test_panel_registration_is_not_fresh_profile_state()
{
	++g_unique_counter;
	gchar* name = g_strdup_printf("meow-panel-registration-%d-%d",
			static_cast<int>(g_get_real_time() & 0x7fffffff), g_unique_counter);
	const char* base = "/plugins/plugin-2";
	XfconfChannel* root = xfconf_channel_new(name);
	XfconfChannel* plugin = xfconf_channel_new_with_property_base(name, base);
	g_free(name);
	assert(xfconf_channel_set_string(root, base, "meowmenu"));

	auto count_settings = [base](XfconfChannel* channel)
	{
		unsigned int count = 0;
		bool registration_seen = false;
		GHashTable* properties = xfconf_channel_get_properties(channel, nullptr);
		GHashTableIter iter;
		gpointer key = nullptr;
		gpointer value = nullptr;
		g_hash_table_iter_init(&iter, properties);
		while (g_hash_table_iter_next(&iter, &key, &value))
		{
			const char* property = static_cast<const char*>(key);
			registration_seen = registration_seen
					|| std::strcmp(property, base) == 0;
			if (WhiskerMenu::settings_relative_property(property, base))
				++count;
		}
		g_hash_table_unref(properties);
		assert(registration_seen);
		return count;
	};

	assert(count_settings(plugin) == 0);
	assert(WhiskerMenu::should_apply_fresh_preset(false,
			count_settings(plugin) == 0));
	assert(xfconf_channel_set_int(plugin, "/view-mode", 0));
	assert(count_settings(plugin) == 1);
	assert(!WhiskerMenu::should_apply_fresh_preset(false,
			count_settings(plugin) == 0));

	g_object_unref(plugin);
	g_object_unref(root);
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
	assert(!p->identity_localizable);
	g_object_unref(ch);
}

void test_seeded_file_localization_provenance()
{
	gchar* system_template = g_strdup("/tmp/meow-system-presets-XXXXXX");
	gchar* user_template = g_strdup("/tmp/meow-user-presets-XXXXXX");
	const gchar* system_dir_raw = g_mkdtemp(system_template);
	const gchar* user_dir_raw = g_mkdtemp(user_template);
	assert(system_dir_raw && user_dir_raw);
	const std::string system_dir(system_dir_raw);
	const std::string user_dir(user_dir_raw);

	write_meowpreset(system_dir, "packaged",
		"[Preset]\nName=Classic\n\n[Settings]\ncorner-radius=4\n");
	write_meowpreset(user_dir, "drop-in",
		"[Preset]\nName=Classic\n\n[Settings]\ncorner-radius=8\n");

	const auto presets = WhiskerMenu::enumerate_preset_files(system_dir, user_dir);
	const WhiskerMenu::LayoutPreset* packaged = find_in(presets, "packaged");
	const WhiskerMenu::LayoutPreset* drop_in = find_in(presets, "drop-in");
	assert(packaged && packaged->identity_localizable);
	assert(drop_in && !drop_in->identity_localizable);
	assert(WhiskerMenu::preset_name_for_display(*drop_in) == "Classic");

	g_free(system_template);
	g_free(user_template);
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
	xfconf_channel_set_int(ch, "/migration/composition-reset-generation", 1);
	xfconf_channel_set_string(ch, "/migration/composition-reset-state", "complete");
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
		"/migration/composition-reset-generation"));
	assert(xfconf_channel_has_property(ch, "/migration/composition-reset-state"));
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
	assert(xfconf_channel_get_int(ch,
		"/migration/composition-reset-generation", 0) == 1);
	gchar* legacy_state = xfconf_channel_get_string(ch,
		"/migration/composition-reset-state", nullptr);
	assert(legacy_state && std::strcmp(legacy_state, "complete") == 0);
	g_free(legacy_state);

	// And the preserved preset still enumerates with its stored values.
	const auto& presets = WhiskerMenu::enumerate_user_presets(ch);
	const WhiskerMenu::LayoutPreset* p = find_in(presets, uuid);
	assert(p != nullptr);
	assert(p->name == "Keep Me");
	assert(p->values.at("corner-radius").i == 7);

	g_object_unref(ch);
}

void test_fresh_channel_has_no_legacy_reset_markers()
{
	const std::string base = "/plugins/plugin-60";
	gchar* name = g_strdup_printf("meow-fresh-profile-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), ++g_unique_counter);
	XfconfChannel* ch = xfconf_channel_new_with_property_base(name, base.c_str());
	g_free(name);

	assert(WhiskerMenu::should_apply_fresh_preset(false, true));
	assert(!xfconf_channel_has_property(ch,
		"/migration/composition-reset-generation"));
	assert(!xfconf_channel_has_property(ch, "/migration/composition-reset-state"));
	g_object_unref(ch);
}

void test_favourite_projection_does_not_write_private_xfconf()
{
	gchar* name = g_strdup_printf("meow-favourite-projection-%d-%d",
			static_cast<int>(g_get_real_time() & 0x7fffffff), ++g_unique_counter);
	XfconfChannel* channel = xfconf_channel_new_with_property_base(name,
			"/plugins/plugin-65");
	g_free(name);
	const std::vector<std::string> stored = {
		"org.example.Terminal.desktop",
		"org.example.Removable.desktop",
		"org.example.Browser.desktop",
	};
	set_string_array(channel, "/favorites", stored);
	const std::vector<std::string> before = property_snapshot(channel);
	for (int reload = 0; reload < 10; ++reload)
	{
		const std::vector<std::string> available = reload == 9
				? stored
				: std::vector<std::string>{
					"org.example.Terminal.desktop",
					"org.example.Browser.desktop",
				};
		const std::vector<std::string> visible =
				WhiskerMenu::favorite_resolved_projection(stored, available);
		assert(visible.size() == (reload == 9 ? 3 : 2));
		assert(property_snapshot(channel) == before);
	}
	assert(string_array(channel, "/favorites") == stored);
	g_object_unref(channel);
}

void test_rc1_upgrade_preserves_durable_families()
{
	const std::string base = "/plugins/meowmenu-18";
	gchar* name_raw = g_strdup_printf("meow-rc1-upgrade-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), ++g_unique_counter);
	const std::string name(name_raw);
	g_free(name_raw);

	XfconfChannel* root = xfconf_channel_new(name.c_str());
	load_xfconf_fixture(root, MEOWMENU_RC_UPGRADE_FIXTURE);
	XfconfChannel* channel = xfconf_channel_new_with_property_base(name.c_str(),
		base.c_str());
	const std::vector<std::string> durable_before = durable_snapshot(channel);
	assert(!durable_before.empty());
	assert(xfconf_channel_get_int(channel, "/schema-version", 0) == 12);

	run_bounded_upgrade_pass(channel);
	assert(durable_snapshot(channel) == durable_before);
	assert(xfconf_channel_get_int(channel, "/schema-version", 0) == 13);
	gchar* sidebar = xfconf_channel_get_string(channel,
		"/sidebar-position", nullptr);
	assert(sidebar && std::strcmp(sidebar, "horizontal") == 0);
	g_free(sidebar);
	assert(!xfconf_channel_has_property(channel, "/profile-position"));
	assert(!xfconf_channel_has_property(channel, "/unified-bar"));

	const std::vector<std::string> after_first = property_snapshot(channel);
	run_bounded_upgrade_pass(channel);
	assert(property_snapshot(channel) == after_first);
	const std::vector<std::string> favorites = string_array(channel, "/favorites");
	assert(favorites.size() == 3);
	assert(favorites[1] == "org.example.Removable.desktop");

	g_object_unref(channel);
	g_object_unref(root);
}

void test_affected_upgrade_preserves_survivors_and_inert_markers()
{
	const std::string base = "/plugins/plugin-64";
	gchar* name_raw = g_strdup_printf("meow-affected-upgrade-%d-%d",
		static_cast<int>(g_get_real_time() & 0x7fffffff), ++g_unique_counter);
	const std::string name(name_raw);
	g_free(name_raw);

	XfconfChannel* root = xfconf_channel_new(name.c_str());
	load_xfconf_fixture(root, MEOWMENU_AFFECTED_UPGRADE_FIXTURE);
	XfconfChannel* channel = xfconf_channel_new_with_property_base(name.c_str(),
		base.c_str());
	const std::vector<std::string> before = property_snapshot(channel);
	const std::vector<std::string> durable_before = durable_snapshot(channel);

	run_bounded_upgrade_pass(channel);
	assert(property_snapshot(channel) == before);
	assert(durable_snapshot(channel) == durable_before);
	run_bounded_upgrade_pass(channel);
	assert(property_snapshot(channel) == before);
	assert(xfconf_channel_get_int(channel,
		"/migration/composition-reset-generation", 0) == 1);
	gchar* state = xfconf_channel_get_string(channel,
		"/migration/composition-reset-state", nullptr);
	assert(state && std::strcmp(state, "pending") == 0);
	g_free(state);

	const std::vector<std::string> favorites = string_array(channel, "/favorites");
	assert((favorites == std::vector<std::string>{
		"org.example.Surviving.desktop",
		"org.example.NewlyAdded.desktop",
	}));
	assert(std::find(favorites.begin(), favorites.end(),
		"org.example.AlreadyErased.desktop") == favorites.end());

	g_object_unref(channel);
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
	test_panel_registration_is_not_fresh_profile_state();
	test_enumerate_drops_missing_display_name();
	test_enumerate_name_falls_back_to_display_name();
	test_upgrade_baseline_enumeration_is_idempotent();
	test_reset_preserves_presets_clears_rest();
	test_fresh_channel_has_no_legacy_reset_markers();
	test_favourite_projection_does_not_write_private_xfconf();
	test_rc1_upgrade_preserves_durable_families();
	test_affected_upgrade_preserves_survivors_and_inert_markers();
	test_seeded_file_accepts_newer_schema();
	test_seeded_file_skips_section_missing();
	test_seeded_file_localization_provenance();
	test_incompatible_user_overlay_cannot_shadow_supported_preset();

	fixture_down();

	std::printf("OK: preset xfconf integration tests passed\n");
	return 0;
}
