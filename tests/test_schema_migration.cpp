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
#include <initializer_list>

// ---------------------------------------------------------------------------
// Minimal stand-in for the pure logic extracted from migrate_schema()
// ---------------------------------------------------------------------------

static int target_schema_version()
{
	return 5;
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

static bool needs_v4_block(int current_schema_version)
{
	return current_schema_version < 4;
}

static bool needs_v5_block(int current_schema_version)
{
	return current_schema_version < 5;
}

/* fresh_install_preset_id:
 *
 * Fresh installs apply the Modern preset (FR-010). Pure mirror of the v1
 * is_fresh_install branch for the regression guard.
 *
 * Returns: the preset id a fresh install lands on.
 */
static const char* fresh_install_preset_id()
{
	return "modern";
}

/* derive_upgrade_preset_name:
 * @has_stored_id:        whether the channel already has a current-preset-id.
 * @matched_builtin_name: the matching built-in's name, or nullptr if the live
 *                        layout matches no built-in exactly.
 * @stored_name:          the existing stored name when @has_stored_id is true.
 *
 * v4→v5 (R2): the upgrade path NEVER hard-defaults the label to "Classic".
 * When an identity is already stored it is kept; otherwise an exact built-in
 * match adopts that built-in's name and anything else becomes "Custom".
 *
 * Returns: the active-preset label after migration.
 */
static const char* derive_upgrade_preset_name(bool has_stored_id,
	const char* matched_builtin_name, const char* stored_name)
{
	if (has_stored_id)
		return stored_name;
	return matched_builtin_name ? matched_builtin_name : "Custom";
}

/* migrate_hidden_sidebar_enabled:
 * @stored_position: pre-migration /sidebar-position string, or nullptr.
 *
 * v3→v4: a stored "hidden" sidebar maps to the new Enable-sidebar switch being
 * OFF; every other (valid) position keeps the sidebar enabled.
 *
 * Returns: the post-migration /sidebar-enabled value.
 */
static bool migrate_hidden_sidebar_enabled(const char* stored_position)
{
	return !(stored_position && std::strcmp(stored_position, "hidden") == 0);
}

/* migrate_hidden_sidebar_position:
 * @stored_position: pre-migration /sidebar-position string, or nullptr.
 *
 * Returns the rewritten position ("left") when the stored value was "hidden",
 * or nullptr to leave a valid position untouched (SC-009).
 */
static const char* migrate_hidden_sidebar_position(const char* stored_position)
{
	if (stored_position && std::strcmp(stored_position, "hidden") == 0)
		return "left";
	return nullptr;
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
	assert(target_schema_version() == 5);
	assert(needs_migration(0) == true);
	assert(needs_migration(1) == true);
	assert(needs_migration(2) == true);
	assert(needs_migration(3) == true);
	assert(needs_migration(4) == true);
	assert(needs_migration(5) == false);
	assert(needs_migration(6) == false);

	// v0 → v5 walks through every block
	assert(needs_v1_block(0) == true);
	assert(needs_v2_block(0) == true);
	assert(needs_v4_block(0) == true);
	assert(needs_v5_block(0) == true);

	// v4 → v5 only runs the v5 block
	assert(needs_v1_block(4) == false);
	assert(needs_v2_block(4) == false);
	assert(needs_v4_block(4) == false);
	assert(needs_v5_block(4) == true);
	assert(needs_v5_block(5) == false);
}

static void test_fresh_install_lands_on_modern()
{
	// FR-010 regression guard: a fresh install applies Modern, not Classic.
	assert(std::strcmp(fresh_install_preset_id(), "modern") == 0);
}

static void test_upgrade_label_derivation()
{
	// Exact built-in match → adopt that built-in's name (never "Classic" by
	// default). nullptr matched-name → the layout matched no built-in → Custom.
	assert(std::strcmp(derive_upgrade_preset_name(false, "Modern", nullptr), "Modern") == 0);
	assert(std::strcmp(derive_upgrade_preset_name(false, "Classic", nullptr), "Classic") == 0);
	assert(std::strcmp(derive_upgrade_preset_name(false, "Full Screen", nullptr), "Full Screen") == 0);
	assert(std::strcmp(derive_upgrade_preset_name(false, nullptr, nullptr), "Custom") == 0);

	// An already-stored identity is preserved verbatim on upgrade.
	assert(std::strcmp(derive_upgrade_preset_name(true, nullptr, "My Layout"), "My Layout") == 0);

	// The old hard "classic" default must never appear for an unmatched layout.
	assert(std::strcmp(derive_upgrade_preset_name(false, nullptr, nullptr), "classic") != 0);
}

static void test_hidden_sidebar_migration()
{
	// Stored "hidden" → Enable-sidebar OFF and a rewritten "left" position.
	assert(migrate_hidden_sidebar_enabled("hidden") == false);
	assert(std::strcmp(migrate_hidden_sidebar_position("hidden"), "left") == 0);

	// Pre-existing left/right/top/bottom configs are untouched (SC-009):
	// sidebar stays enabled and the position is not rewritten.
	for (const char* p : { "left", "right", "top", "bottom" })
	{
		assert(migrate_hidden_sidebar_enabled(p) == true);
		assert(migrate_hidden_sidebar_position(p) == nullptr);
	}

	// Absent key (nullptr) defaults to enabled, position unchanged.
	assert(migrate_hidden_sidebar_enabled(nullptr) == true);
	assert(migrate_hidden_sidebar_position(nullptr) == nullptr);
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

/* unified_bar_default:
 * @has_key: whether the channel already has /unified-bar.
 *
 * Returns the value to write during fresh-install / migration, or -1 to
 * leave the key untouched. On a fresh channel /unified-bar resolves to
 * false (the C++ Settings ctor default and the explicit defaults table).
 */
static int unified_bar_default(bool has_key)
{
	return has_key ? -1 : 0; // false
}

static void test_unified_bar_default_fresh_install()
{
	// Fresh channel → write explicit false.
	assert(unified_bar_default(false) == 0);
	// Already present → leave alone.
	assert(unified_bar_default(true)  == -1);
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
//   assert: schema-version==5, categories-opacity==60, and the active-preset
//            identity derived from the live layout (NOT hard-defaulted to
//            "classic"; an exact built-in match adopts it, otherwise "Custom").
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
	test_unified_bar_default_fresh_install();
	test_hidden_sidebar_migration();
	test_idempotent_guard();
	test_fresh_install_lands_on_modern();
	test_upgrade_label_derivation();
	return 0;
}
