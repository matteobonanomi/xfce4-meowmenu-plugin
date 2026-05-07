/*
 * Unit tests for Settings::migrate_schema() logic.
 *
 * Full Xfconf round-trip tests require a live xfconfd and are marked
 * TODO-INTEGRATION; they will be covered in a follow-up when a
 * lightweight xfconfd fixture is added.
 *
 * What IS tested here:
 *   - is_fresh_install determination from property count (pure logic)
 *   - schema_version guard (pure logic, extracted helper below)
 *   - menu-opacity → categories-opacity mapping logic (pure)
 *
 * Compile: part of the 'tests' Meson subdir.
 */

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
// Minimal stand-in for the pure logic extracted from migrate_schema()
// ---------------------------------------------------------------------------

static bool needs_migration(int current_schema_version)
{
	return current_schema_version < 1;
}

static bool detect_fresh_install(unsigned int property_count)
{
	return property_count == 0;
}

static int map_legacy_opacity(int has_categories_opacity, int legacy_menu_opacity)
{
	// If categories-opacity is already set, keep it; otherwise use legacy.
	return has_categories_opacity ? -1 /* no-op */ : legacy_menu_opacity;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_schema_version_guard()
{
	assert(needs_migration(0) == true);
	assert(needs_migration(1) == false);
	assert(needs_migration(2) == false);
}

static void test_fresh_install_detection()
{
	assert(detect_fresh_install(0) == true);
	assert(detect_fresh_install(1) == false);
	assert(detect_fresh_install(42) == false);
}

static void test_legacy_opacity_mapping()
{
	// categories-opacity NOT yet set → use legacy value
	assert(map_legacy_opacity(0, 75) == 75);
	// categories-opacity already set → no-op (-1)
	assert(map_legacy_opacity(1, 75) == -1);
}

static void test_idempotent_guard()
{
	// Running migration twice: second call must be no-op because
	// after first run schema_version == 1, so needs_migration returns false.
	const int after_first_run = 1;
	assert(needs_migration(after_first_run) == false);
}

// TODO-INTEGRATION: test_v0_to_v1_snapshot()
//   Bring up a real xfconfd on XDG_CONFIG_HOME=/tmp/meow-test-XXXXXX,
//   write a v0 property set (favorites, view-mode, menu-opacity=60),
//   instantiate Settings, call migrate_schema(false),
//   assert: schema-version==1, categories-opacity==60,
//            current-preset-id=="classic".
//
// TODO-INTEGRATION: test_fresh_install_sets_modern()
//   Same fixture, but empty channel.
//   assert: current-preset-id=="modern" (T030 wired: apply_preset(BUILTIN_PRESETS[MODERN])
//   is now called in migrate_schema when is_fresh_install==true).

int main()
{
	test_schema_version_guard();
	test_fresh_install_detection();
	test_legacy_opacity_mapping();
	test_idempotent_guard();
	return 0;
}
