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

static int target_schema_version()
{
	return 2;
}

static bool needs_migration(int current_schema_version)
{
	return current_schema_version < target_schema_version();
}

static bool needs_v1_block(int current_schema_version)
{
	return current_schema_version < 1;
}

static bool needs_v2_block(int current_schema_version)
{
	return current_schema_version < 2;
}

/* full_screen_opacity_default:
 * @has_key: whether the channel already has /full-screen-opacity.
 * @preset_value: integer 0-100 from active preset, or -1 if preset doesn't pin one.
 *
 * Returns the value to write during v2 migration, or -1 to leave the key
 * untouched. See contracts/xfconf-keys.md §Schema migration.
 */
static int full_screen_opacity_default(bool has_key, int preset_value)
{
	if (has_key)
		return -1;
	return preset_value >= 0 ? preset_value : 100;
}

/* migrate_position_categories_horizontal:
 * @old_value: pre-migration boolean from /position-categories-horizontal.
 * @current_sidebar: pre-migration string ("left"/"right"/"top"/"bottom"/"hidden") or nullptr.
 *
 * Returns the post-migration sidebar-position string, or nullptr if the
 * sidebar position should not be rewritten. See contracts/tab-inventory.md
 * §Deprecated keys.
 */
static const char* migrate_position_categories_horizontal(bool old_value, const char* current_sidebar)
{
	if (!old_value)
		return nullptr;
	const bool vertical = !current_sidebar
			|| std::strcmp(current_sidebar, "left") == 0
			|| std::strcmp(current_sidebar, "right") == 0;
	return vertical ? "top" : nullptr;
}

/* migrate_profile_shape_hidden:
 * @old_shape: 0=Round, 1=Square, 2=Hidden.
 *
 * Returns the post-migration shape (always 0 or 1) and writes "hidden" into
 * @out_position when @old_shape was Hidden.
 */
static int migrate_profile_shape_hidden(int old_shape, const char** out_position)
{
	*out_position = nullptr;
	if (old_shape == 2)
	{
		*out_position = "hidden";
		return 0;
	}
	return old_shape;
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
	assert(target_schema_version() == 2);
	assert(needs_migration(0) == true);
	assert(needs_migration(1) == true);
	assert(needs_migration(2) == false);
	assert(needs_migration(3) == false);

	// v0 → v2 walks through every block
	assert(needs_v1_block(0) == true);
	assert(needs_v2_block(0) == true);

	// v1 → v2 only runs the v2 block
	assert(needs_v1_block(1) == false);
	assert(needs_v2_block(1) == true);
}

static void test_full_screen_opacity_default()
{
	// New install / key absent, preset omits → fall back to 100.
	assert(full_screen_opacity_default(false, -1) == 100);
	// Preset pins a value → use it.
	assert(full_screen_opacity_default(false, 85) == 85);
	assert(full_screen_opacity_default(false, 0)  == 0);
	// Already set → no-op (-1 sentinel).
	assert(full_screen_opacity_default(true,  42) == -1);
}

static void test_position_categories_horizontal_migration()
{
	// Old value off → no rewrite.
	assert(migrate_position_categories_horizontal(false, "left")  == nullptr);
	assert(migrate_position_categories_horizontal(false, nullptr) == nullptr);

	// Old value on + sidebar was vertical → promote sidebar to "top".
	assert(std::strcmp(migrate_position_categories_horizontal(true, "left"),  "top") == 0);
	assert(std::strcmp(migrate_position_categories_horizontal(true, "right"), "top") == 0);
	assert(std::strcmp(migrate_position_categories_horizontal(true, nullptr), "top") == 0);

	// Old value on + sidebar already horizontal → leave sidebar alone.
	assert(migrate_position_categories_horizontal(true, "top")    == nullptr);
	assert(migrate_position_categories_horizontal(true, "bottom") == nullptr);

	// Old value on + sidebar hidden → leave sidebar alone.
	assert(migrate_position_categories_horizontal(true, "hidden") == nullptr);
}

static void test_profile_shape_hidden_migration()
{
	const char* pos = nullptr;
	// Round / Square pass through unchanged.
	assert(migrate_profile_shape_hidden(0, &pos) == 0 && pos == nullptr);
	assert(migrate_profile_shape_hidden(1, &pos) == 1 && pos == nullptr);
	// Hidden → reset shape to Round + emit profile-position="hidden".
	assert(migrate_profile_shape_hidden(2, &pos) == 0);
	assert(pos && std::strcmp(pos, "hidden") == 0);
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
	// after first run schema_version == target, so needs_migration returns false.
	assert(needs_migration(target_schema_version()) == false);
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
	test_full_screen_opacity_default();
	test_position_categories_horizontal_migration();
	test_profile_shape_hidden_migration();
	test_idempotent_guard();
	return 0;
}
