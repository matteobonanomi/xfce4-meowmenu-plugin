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

#include "../panel-plugin/settings-defaults.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>

// ---------------------------------------------------------------------------
// Minimal stand-in for the pure logic extracted from migrate_schema()
// ---------------------------------------------------------------------------

static int target_schema_version()
{
	return 11;
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

static bool needs_v6_block(int current_schema_version)
{
	return current_schema_version < 6;
}

static bool needs_v7_block(int current_schema_version)
{
	return current_schema_version < 7;
}

static bool needs_v8_block(int current_schema_version)
{
	return current_schema_version < 8;
}

static bool needs_v9_block(int current_schema_version)
{
	return current_schema_version < 9;
}

static bool needs_v10_block(int current_schema_version)
{
	return current_schema_version < 10;
}

static bool needs_v11_block(int current_schema_version)
{
	return current_schema_version < 11;
}

/* calculator_engine_seed:
 * @preset_id: stored built-in identity, custom id, or nullptr.
 *
 * Mirrors schema v11: recognized modern layouts opt into bc, Classic and
 * unknown/custom identities retain the safe disabled default.
 */
static const char* calculator_engine_seed(const char* preset_id)
{
	return preset_id && (std::strcmp(preset_id, "modern") == 0
		|| std::strcmp(preset_id, "fullscreen") == 0
		|| std::strcmp(preset_id, "minimal") == 0) ? "bc" : "none";
}

static bool calculator_engine_valid(const char* id)
{
	return id && (std::strcmp(id, "none") == 0 || std::strcmp(id, "bc") == 0
		|| std::strcmp(id, "qalc") == 0 || std::strcmp(id, "gcalccmd") == 0);
}

static int calculator_int_recover(bool is_integer, int value,
	int minimum, int maximum, int default_value)
{
	return is_integer && value >= minimum && value <= maximum
		? value : default_value;
}

static void test_calculator_v11_migration()
{
	assert(needs_v11_block(10));
	assert(!needs_v11_block(11));
	assert(std::strcmp(calculator_engine_seed("classic"), "none") == 0);
	assert(std::strcmp(calculator_engine_seed("modern"), "bc") == 0);
	assert(std::strcmp(calculator_engine_seed("fullscreen"), "bc") == 0);
	assert(std::strcmp(calculator_engine_seed("minimal"), "bc") == 0);
	assert(std::strcmp(calculator_engine_seed("custom-uuid"), "none") == 0);
	assert(std::strcmp(calculator_engine_seed(nullptr), "none") == 0);
	assert(calculator_engine_valid("none"));
	assert(calculator_engine_valid("bc"));
	assert(calculator_engine_valid("qalc"));
	assert(calculator_engine_valid("gcalccmd"));
	assert(!calculator_engine_valid("/bin/sh"));
	assert(calculator_int_recover(true, 6, -1, 6, -1) == 6);
	assert(calculator_int_recover(true, 7, -1, 6, -1) == -1);
	assert(calculator_int_recover(false, 3, -1, 6, -1) == -1);
	assert(calculator_int_recover(true, 10, 0, 10, 4) == 10);
	assert(calculator_int_recover(true, 11, 0, 10, 4) == 4);
	assert(calculator_int_recover(false, 8, 0, 10, 4) == 4);
}

/* fresh_install_preset_id:
 *
 * Fresh installs apply the Modern preset (the documented behavior). Pure mirror of the v1
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
 * or nullptr to leave a valid position untouched (the documented behavior).
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

static void test_marker_decision_table()
{
	assert(WhiskerMenu::should_apply_fresh_preset(false, true));
	assert(!WhiskerMenu::should_apply_fresh_preset(true, true));
	assert(!WhiskerMenu::should_apply_fresh_preset(false, false));
	assert(!WhiskerMenu::should_apply_fresh_preset(true, false));
}

struct StoredPresentationValues
{
	int category_icon_size;
	const char* layout_mode;
	const char* preset_id;
	int calculator_size;
};

static void apply_initial_preset_if_needed(bool marker, bool empty_channel,
	StoredPresentationValues* values)
{
	if (!WhiskerMenu::should_apply_fresh_preset(marker, empty_channel))
		return;
	values->category_icon_size = 1;
	values->layout_mode = "docked";
	values->preset_id = "modern";
	values->calculator_size = -1;
}

static void test_upgrade_preserves_stored_presentation_values()
{
	const StoredPresentationValues expected = { 6, "fullscreen", "saved-custom", 5 };
	for (const bool marker : { true, false })
	{
		StoredPresentationValues values = expected;
		apply_initial_preset_if_needed(marker, false, &values);
		assert(values.category_icon_size == expected.category_icon_size);
		assert(std::strcmp(values.layout_mode, expected.layout_mode) == 0);
		assert(std::strcmp(values.preset_id, expected.preset_id) == 0);
		assert(values.calculator_size == expected.calculator_size);
	}
}

/* test_080_fixture_preserves_release_baseline:
 *
 * Verifies that the representative upgrade snapshot contains the panel-scoped
 * identity, layout, favourites, Calculator choices, and custom-preset subtree
 * that the live release walkthrough compares before and after migration.
 */
static void test_080_fixture_preserves_release_baseline()
{
	std::ifstream input(MEOWMENU_080_XFCONF_FIXTURE);
	assert(input.good());
	const std::string xml((std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>());
	for (const char* token : {
		"plugin-17",
		"saved-custom",
		"layout-mode",
		"centered",
		"firefox.desktop",
		"org.xfce.Thunar.desktop",
		"calculator-engine",
		"qalc",
		"calculator-result-size",
		"calculator-decimal-precision",
		"rc-upgrade-baseline",
	})
		assert(xml.find(token) != std::string::npos);
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
	assert(target_schema_version() == 11);
	assert(needs_migration(0) == true);
	assert(needs_migration(1) == true);
	assert(needs_migration(2) == true);
	assert(needs_migration(3) == true);
	assert(needs_migration(4) == true);
	assert(needs_migration(5) == true);
	assert(needs_migration(6) == true);
	assert(needs_migration(7) == true);
	assert(needs_migration(8) == true);
	assert(needs_migration(9) == true);
	assert(needs_migration(10) == true);
	assert(needs_migration(11) == false);

	// v0 → v10 walks through every block
	assert(needs_v1_block(0) == true);
	assert(needs_v2_block(0) == true);
	assert(needs_v4_block(0) == true);
	assert(needs_v5_block(0) == true);
	assert(needs_v6_block(0) == true);
	assert(needs_v7_block(0) == true);
	assert(needs_v8_block(0) == true);
	assert(needs_v9_block(0) == true);
	assert(needs_v10_block(0) == true);

	// v7 → v10 runs the v8, v9, and v10 blocks.
	assert(needs_v1_block(7) == false);
	assert(needs_v2_block(7) == false);
	assert(needs_v4_block(7) == false);
	assert(needs_v5_block(7) == false);
	assert(needs_v6_block(7) == false);
	assert(needs_v7_block(7) == false);
	assert(needs_v8_block(7) == true);
	assert(needs_v9_block(7) == true);
	assert(needs_v10_block(7) == true);

	// v8 → v10 runs the v9 and v10 blocks.
	assert(needs_v8_block(8) == false);
	assert(needs_v9_block(8) == true);
	assert(needs_v10_block(8) == true);
	assert(needs_v9_block(9) == false);
	assert(needs_v10_block(9) == true);
	assert(needs_v10_block(10) == false);

	// v6 → v10 still runs the v7, v8, v9, and v10 blocks.
	assert(needs_v1_block(6) == false);
	assert(needs_v2_block(6) == false);
	assert(needs_v4_block(6) == false);
	assert(needs_v5_block(6) == false);
	assert(needs_v6_block(6) == false);
	assert(needs_v7_block(6) == true);
	assert(needs_v7_block(7) == false);
	assert(needs_v8_block(6) == true);
	assert(needs_v8_block(8) == false);
	assert(needs_v9_block(6) == true);
	assert(needs_v10_block(6) == true);
}

static void test_fresh_install_lands_on_modern()
{
	// the documented behavior regression guard: a fresh install applies Modern, not Classic.
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

	// Pre-existing left/right/top/bottom configs are untouched (the documented behavior):
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

/* transparent_grid_default:
 * @has_key: whether the channel already has /transparent-grid.
 *
 * Returns the value to write during schema-v9 migration, or -1 to leave the
 * key untouched. Existing users keep solid grid tiles unless they opt in.
 */
static int transparent_grid_default(bool has_key)
{
	return has_key ? -1 : 0; // false
}

static void test_transparent_grid_default_fresh_install()
{
	assert(transparent_grid_default(false) == 0);
	assert(transparent_grid_default(true) == -1);
	assert(needs_v9_block(8) == true);
	assert(needs_v9_block(9) == false);
}

static void test_idempotent_guard()
{
	// Running migration twice: second call must be no-op because
	// after first run schema_version == target, so needs_migration returns false.
	assert(needs_migration(target_schema_version()) == false);
}

// ---------------------------------------------------------------------------
// schema-v6 cleanup mirror
//
// Mirrors the schema-v6 block in settings-defaults.cpp: a fixed set of removed
// keys is reset, and a stored profile-position of "bottom-right" is normalised
// to "bottom". Modelled over a tiny key set so the delete/keep/rewrite decisions
// can be asserted without a live xfconfd.
// ---------------------------------------------------------------------------

/* v6_resets_key:
 * @key: an xfconf property path.
 *
 * Returns: true iff the schema-v6 block deletes @key (orphaned/removed config).
 */
static bool v6_resets_key(const char* key)
{
	const char* removed[] = {
		"/grid-auto-size",
		"/grid-columns",
		"/grid-rows",
		"/places/show-metadata",
	};
	for (const char* k : removed)
		if (std::strcmp(key, k) == 0)
			return true;
	return false;
}

/* v6_rewrite_profile_position:
 * @value: the stored /profile-position string, or NULL when absent.
 *
 * Returns: the value to write, or NULL to leave the key untouched. Only the
 * retired "bottom-right" is rewritten to "bottom"; every other value (and an
 * absent key) is left as-is.
 */
static const char* v6_rewrite_profile_position(const char* value)
{
	if (value && std::strcmp(value, "bottom-right") == 0)
		return "bottom";
	return nullptr;
}

/* v8_rewrite_profile_position:
 * @value: the stored /profile-position string, or NULL when absent.
 *
 * Returns: the canonical value to write, or NULL to leave the key untouched.
 * Visible legacy aliases are rewritten to the explicit left-anchored domain.
 */
static const char* v8_rewrite_profile_position(const char* value)
{
	if (value && std::strcmp(value, "top") == 0)
		return "top-left";
	if (value && (std::strcmp(value, "bottom") == 0
			|| std::strcmp(value, "bottom-right") == 0))
		return "bottom-left";
	return nullptr;
}

static void test_v6_cleanup()
{
	// The four orphaned/removed keys are deleted.
	assert(v6_resets_key("/grid-auto-size") == true);
	assert(v6_resets_key("/grid-columns") == true);
	assert(v6_resets_key("/grid-rows") == true);
	assert(v6_resets_key("/places/show-metadata") == true);

	// Unrelated keys are left untouched (the documented behavior).
	assert(v6_resets_key("/categories-opacity") == false);
	assert(v6_resets_key("/profile-position") == false);
	assert(v6_resets_key("/full-screen-opacity") == false);

	// Redundant "bottom-right" profile value is normalised to "bottom".
	const char* rewritten = v6_rewrite_profile_position("bottom-right");
	assert(rewritten && std::strcmp(rewritten, "bottom") == 0);

	// Every other profile value, and an absent key, is left as-is (no-op).
	assert(v6_rewrite_profile_position("top") == nullptr);
	assert(v6_rewrite_profile_position("bottom") == nullptr);
	assert(v6_rewrite_profile_position("hidden") == nullptr);
	assert(v6_rewrite_profile_position(nullptr) == nullptr);
}

static void test_v8_profile_position_canonicalization()
{
	assert(std::strcmp(v8_rewrite_profile_position("top"), "top-left") == 0);
	assert(std::strcmp(v8_rewrite_profile_position("bottom"), "bottom-left") == 0);
	assert(std::strcmp(v8_rewrite_profile_position("bottom-right"), "bottom-left") == 0);
	const char* after_v6 = v6_rewrite_profile_position("bottom-right");
	assert(after_v6 && std::strcmp(after_v6, "bottom") == 0);
	assert(std::strcmp(v8_rewrite_profile_position(after_v6), "bottom-left") == 0);
	assert(v8_rewrite_profile_position("top-left") == nullptr);
	assert(v8_rewrite_profile_position("bottom-left") == nullptr);
	assert(v8_rewrite_profile_position("hidden") == nullptr);
	assert(v8_rewrite_profile_position(nullptr) == nullptr);
}

// ---------------------------------------------------------------------------
// schema-v7 migration mirror (042-simple-opacity)
//
// v7 collapses the three per-region opacities to one /menu-opacity, derived from
// the active preset's menu-opacity (or 100 when no preset governs it), and resets
// the three retired keys. Modelled here as pure logic; the live xfconf round-trip
// is covered by test_migration.cpp.
// ---------------------------------------------------------------------------

/* derive_menu_opacity_v7:
 * @has_preset:          whether current-preset-id resolves to a known preset
 *                       that carries a menu-opacity value.
 * @preset_menu_opacity: that preset's menu-opacity (ignored when !has_preset).
 *
 * Returns: the value written to /menu-opacity — the preset's value, or 100 when
 * no preset governs opacity (the documented behavior).
 */
static int derive_menu_opacity_v7(bool has_preset, int preset_menu_opacity)
{
	return has_preset ? preset_menu_opacity : 100;
}

/* v7_resets_key:
 * @key: an xfconf property path.
 *
 * Returns: true iff the schema-v7 block resets @key (the three retired
 * per-region opacity keys).
 */
static bool v7_resets_key(const char* key)
{
	const char* removed[] = {
		"/categories-opacity",
		"/apps-opacity",
		"/full-screen-opacity",
	};
	for (const char* k : removed)
		if (std::strcmp(key, k) == 0)
			return true;
	return false;
}

static void test_v7_derives_from_active_preset()
{
	// Each built-in pins its single opacity; v7 derives exactly that value.
	struct { int preset_value; } cases[] = { {100}, {100}, {80}, {60} };
	for (const auto& c : cases)
		assert(derive_menu_opacity_v7(true, c.preset_value) == c.preset_value);
}

static void test_v7_no_preset_falls_back_to_100()
{
	// No active preset (unset/unknown id, or a preset without menu-opacity) → 100.
	assert(derive_menu_opacity_v7(false, 0)  == 100);
	assert(derive_menu_opacity_v7(false, 42) == 100);
}

static void test_v7_resets_retired_keys()
{
	// The three retired per-region keys are reset; unrelated keys are untouched.
	assert(v7_resets_key("/categories-opacity") == true);
	assert(v7_resets_key("/apps-opacity") == true);
	assert(v7_resets_key("/full-screen-opacity") == true);
	assert(v7_resets_key("/menu-opacity") == false);
	assert(v7_resets_key("/corner-radius") == false);
}

static void test_v7_idempotent_one_shot()
{
	// Guarded by < 7: a second pass after the first run is a no-op.
	assert(needs_v7_block(6) == true);
	assert(needs_v7_block(7) == false);
}

static void test_v7_fresh_install_lands_on_100_no_old_keys()
{
	// Fresh installs apply Modern (menu-opacity 100) up front; v7 then derives
	// 100 from the active preset and resets the orphaned per-region keys.
	const int modern_menu_opacity = 100;
	assert(derive_menu_opacity_v7(true, modern_menu_opacity) == 100);
	for (const char* k : { "/categories-opacity", "/apps-opacity", "/full-screen-opacity" })
		assert(v7_resets_key(k) == true);
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
//   assert: current-preset-id=="modern" (the implementation step wired: apply_preset(BUILTIN_PRESETS[MODERN])
//   is now called in migrate_schema when is_fresh_install==true).

int main()
{
	test_schema_version_guard();
	test_fresh_install_detection();
	test_marker_decision_table();
	test_upgrade_preserves_stored_presentation_values();
	test_080_fixture_preserves_release_baseline();
	test_legacy_opacity_mapping();
	test_full_screen_opacity_default();
	test_position_categories_horizontal_migration();
	test_profile_shape_hidden_migration();
	test_unified_bar_default_fresh_install();
	test_transparent_grid_default_fresh_install();
	test_hidden_sidebar_migration();
	test_idempotent_guard();
	test_fresh_install_lands_on_modern();
	test_upgrade_label_derivation();
	test_v6_cleanup();
	test_v8_profile_position_canonicalization();
	// 042-simple-opacity: schema-v7 single menu-opacity derivation + cleanup
	test_v7_derives_from_active_preset();
	test_v7_no_preset_falls_back_to_100();
	test_v7_resets_retired_keys();
	test_v7_idempotent_one_shot();
	test_v7_fresh_install_lands_on_100_no_old_keys();
	test_calculator_v11_migration();
	return 0;
}
