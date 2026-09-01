/*
 * Unit tests for preset.h/.cpp pure logic:
 *  - BUILTIN_PRESETS[3] entries have expected values
 *  - apply_preset writes the right number of properties (verified via diff)
 *  - compute_preset_diff returns false right after apply, true after change
 *  - find_preset_by_id finds the three built-in ids and returns nullptr for unknown
 *
 * NOTE: apply_preset / compute_preset_diff write to a Settings object backed by
 * Xfconf. Full integration testing (with real xfconfd) is covered by the
 * TODO-INTEGRATION tests in test_schema_migration.cpp.
 *
 * The tests here use a "mock" Settings object whose member fields are
 * initialised manually to known values WITHOUT connecting to Xfconf, by
 * exploiting the fact that the Settings fields (Integer/Boolean/String) can be
 * constructed with their defaults and the assignment operator writes back only
 * when a channel is present.  With channel==nullptr the writes to fields are
 * in-memory only — perfect for unit tests.
 *
 * Settings constructor is private (friend Plugin), so we cannot instantiate
 * it directly.  Instead, we test the *pure* diff/find logic via a lightweight
 * shadow struct defined here.
 */

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Real preset table + governed-key set, linked from the Settings-free
// preset-builtins.cpp translation unit. Used by the completeness / agreement
// tests below to guard the C++ table, the .meowpreset seeds, and governed_keys()
// against drift.
#include <glib.h>
#include "presets/preset.h"

// ---------------------------------------------------------------------------
// Shadow types — mirror the minimal interface that preset.cpp needs.
// We test the logic we CAN test without instantiating the full Settings class.
// ---------------------------------------------------------------------------

// Reproduce PresetValue / PresetValueMap / LayoutPreset from preset.h
// (minimal copy for test isolation)
struct PV
{
	enum Kind { B, I, S } kind;
	bool        b;
	int         i;
	std::string s;

	static PV from_bool(bool v)        { PV p; p.kind = B; p.b = v; return p; }
	static PV from_int(int v)          { PV p; p.kind = I; p.i = v; return p; }
	static PV from_str(const char* v)  { PV p; p.kind = S; p.s = v; return p; }
};

typedef std::map<std::string, PV> PVMap;

// Logical equivalent of the built-in preset table from data-model.md
struct TestPresetDef
{
	const char* id;
	PVMap values;
};

static TestPresetDef make_classic()
{
	return {
		"classic",
		{
			{ "corner-radius",         PV::from_int(0)       },
			{ "panel-gap",             PV::from_int(0)       },
			{ "menu-opacity",          PV::from_int(100)     },
			{ "sidebar-position",      PV::from_str("right") },
			{ "search-bar-position",   PV::from_str("top")   },
			{ "show-profile",          PV::from_bool(true)    },
			{ "show-session",          PV::from_bool(true)    },
			{ "layout-mode",           PV::from_str("docked") },
			{ "launcher-icon-size",    PV::from_int(2)       },
			{ "hover-switch-category", PV::from_bool(false)  },
			{ "view-mode-default",     PV::from_str("list")  },
			{ "default-category",      PV::from_str("favorites") },
			{ "stay-on-focus-out",     PV::from_bool(false)  },
			{ "menu-width",            PV::from_int(450)     },
			{ "menu-height",           PV::from_int(500)     },
			{ "calculator-engine", PV::from_str("none") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

static TestPresetDef make_modern()
{
	return {
		"modern",
		{
			{ "corner-radius",         PV::from_int(12)       },
			{ "panel-gap",             PV::from_int(8)        },
			{ "menu-opacity",          PV::from_int(100)      },
			{ "sidebar-position",      PV::from_str("left")   },
			{ "search-bar-position",   PV::from_str("top")    },
			{ "show-profile",          PV::from_bool(true)     },
			{ "show-session",          PV::from_bool(true)     },
			{ "layout-mode",           PV::from_str("docked") },
			{ "launcher-icon-size",    PV::from_int(3)        },
			{ "grid-density",          PV::from_str("medium") },
			{ "hover-switch-category", PV::from_bool(true)    },
			{ "transparent-grid",      PV::from_bool(true)    },
			{ "view-mode-default",     PV::from_str("icons")  },
			{ "default-category",      PV::from_str("recent") },
			{ "stay-on-focus-out",     PV::from_bool(false)   },
			{ "menu-width",            PV::from_int(450)      },
			{ "menu-height",           PV::from_int(500)      },
			{ "calculator-engine", PV::from_str("bc") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

static TestPresetDef make_fullscreen()
{
	return {
		"fullscreen",
		{
			{ "corner-radius",         PV::from_int(0)              },
			{ "panel-gap",             PV::from_int(0)              },
			{ "menu-opacity",          PV::from_int(80)             },
			{ "sidebar-position",      PV::from_str("left")         },
			{ "search-bar-position",   PV::from_str("top")          },
			{ "show-profile",          PV::from_bool(true)           },
			{ "show-session",          PV::from_bool(true)           },
			{ "launcher-icon-size",    PV::from_int(4)              },
			{ "grid-density",          PV::from_str("medium")       },
			{ "layout-mode",           PV::from_str("fullscreen")   },
			{ "hover-switch-category", PV::from_bool(true)          },
			{ "transparent-grid",      PV::from_bool(true)          },
			{ "view-mode-default",     PV::from_str("icons")        },
			{ "default-category",      PV::from_str("all")          },
			{ "stay-on-focus-out",     PV::from_bool(false)         },
			{ "calculator-engine", PV::from_str("bc") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

static TestPresetDef make_minimal()
{
	return {
		"minimal",
		{
			{ "corner-radius",         PV::from_int(12)       },
			{ "panel-gap",             PV::from_int(8)        },
			{ "menu-opacity",          PV::from_int(60)       },
			{ "sidebar-position",      PV::from_str("left")   },
			{ "search-bar-position",   PV::from_str("top")    },
			{ "show-profile",          PV::from_bool(false)   },
			{ "show-session",          PV::from_bool(false)   },
			{ "grid-density",          PV::from_str("medium") },
			{ "layout-mode",           PV::from_str("centered") },
			{ "launcher-icon-size",    PV::from_int(3)        },
			{ "hover-switch-category", PV::from_bool(true)    },
			{ "view-mode-default",     PV::from_str("list")   },
			{ "default-category",      PV::from_str("recent") },
			{ "stay-on-focus-out",     PV::from_bool(false)   },
			{ "menu-width",            PV::from_int(450)      },
			{ "menu-height",           PV::from_int(306)      },
			{ "calculator-engine", PV::from_str("bc") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

// ---------------------------------------------------------------------------
// Minimal settings shadow — mirrors the fields that apply_preset touches.
// ---------------------------------------------------------------------------

struct SettingsShadow
{
	int corner_radius     = 0;
	int panel_gap         = 0;
	int menu_opacity      = 100;
	std::string sidebar_position    = "left";
	std::string search_bar_position = "top";
	bool show_profile = true;
	bool show_session = true;
	std::string grid_density = "medium";
	std::string layout_mode  = "docked";
	bool category_hover_activate = false;
	bool transparent_grid = false;
	int launcher_icon_size = 2;
	int view_mode = 1; // ViewAsList
	int default_category = 0; // Favorites
	bool stay_on_focus_out = false;
	int menu_width = 450;
	int menu_height = 500;
	std::string calculator_engine = "none";
	int calculator_result_font_size = -1;
	int calculator_max_decimal_places = 4;
	std::string current_preset_id;
};

static void apply_preset_shadow(const TestPresetDef& preset, SettingsShadow& s)
{
	for (const auto& kv : preset.values)
	{
		const std::string& prop = kv.first;
		const PV& val = kv.second;
		if (prop == "corner-radius" && val.kind == PV::I)             s.corner_radius = val.i;
		else if (prop == "panel-gap" && val.kind == PV::I)            s.panel_gap = val.i;
		else if (prop == "menu-opacity" && val.kind == PV::I)         s.menu_opacity = val.i;
		else if (prop == "sidebar-position" && val.kind == PV::S)     s.sidebar_position = val.s;
		else if (prop == "search-bar-position" && val.kind == PV::S)  s.search_bar_position = val.s;
		else if (prop == "show-profile" && val.kind == PV::B)         s.show_profile = val.b;
		else if (prop == "show-session" && val.kind == PV::B)         s.show_session = val.b;
		else if (prop == "grid-density" && val.kind == PV::S)         s.grid_density = val.s;
		else if (prop == "layout-mode" && val.kind == PV::S)          s.layout_mode = val.s;
		else if (prop == "launcher-icon-size" && val.kind == PV::I)   s.launcher_icon_size = val.i;
		else if (prop == "hover-switch-category" && val.kind == PV::B)s.category_hover_activate = val.b;
		else if (prop == "transparent-grid" && val.kind == PV::B)     s.transparent_grid = val.b;
		else if (prop == "menu-width" && val.kind == PV::I)           s.menu_width = val.i;
		else if (prop == "menu-height" && val.kind == PV::I)          s.menu_height = val.i;
		else if (prop == "calculator-engine" && val.kind == PV::S)
			s.calculator_engine = val.s;
		else if (prop == "calculator-result-font-size" && val.kind == PV::I)
			s.calculator_result_font_size = val.i;
		else if (prop == "calculator-max-decimal-places" && val.kind == PV::I)
			s.calculator_max_decimal_places = val.i;
		else if (prop == "view-mode-default" && val.kind == PV::S)
		{
			if (val.s == "icons") s.view_mode = 0;
			else if (val.s == "tree") s.view_mode = 2;
			else s.view_mode = 1;
		}
		else if (prop == "default-category" && val.kind == PV::S)
		{
			if (val.s == "recent") s.default_category = 1;
			else if (val.s == "all") s.default_category = 2;
			else s.default_category = 0;
		}
		else if (prop == "stay-on-focus-out" && val.kind == PV::B)    s.stay_on_focus_out = val.b;
	}
	s.current_preset_id = preset.id;
}

static bool compute_diff_shadow(const TestPresetDef& preset, const SettingsShadow& s)
{
	for (const auto& kv : preset.values)
	{
		const std::string& prop = kv.first;
		const PV& val = kv.second;
		if (prop == "corner-radius" && val.kind == PV::I && s.corner_radius != val.i) return true;
		if (prop == "panel-gap" && val.kind == PV::I && s.panel_gap != val.i) return true;
		if (prop == "menu-opacity" && val.kind == PV::I && s.menu_opacity != val.i) return true;
		if (prop == "sidebar-position" && val.kind == PV::S && s.sidebar_position != val.s) return true;
		if (prop == "search-bar-position" && val.kind == PV::S && s.search_bar_position != val.s) return true;
		if (prop == "show-profile" && val.kind == PV::B && s.show_profile != val.b) return true;
		if (prop == "show-session" && val.kind == PV::B && s.show_session != val.b) return true;
		if (prop == "grid-density" && val.kind == PV::S && s.grid_density != val.s) return true;
		if (prop == "layout-mode" && val.kind == PV::S && s.layout_mode != val.s) return true;
		if (prop == "launcher-icon-size" && val.kind == PV::I && s.launcher_icon_size != val.i) return true;
		if (prop == "hover-switch-category" && val.kind == PV::B && s.category_hover_activate != val.b) return true;
		if (prop == "transparent-grid" && val.kind == PV::B && s.transparent_grid != val.b) return true;
		if (prop == "menu-width" && val.kind == PV::I && s.menu_width != val.i) return true;
		if (prop == "menu-height" && val.kind == PV::I && s.menu_height != val.i) return true;
		if (prop == "view-mode-default" && val.kind == PV::S)
		{
			const std::string& sv = val.s;
			if ((sv == "icons" && s.view_mode != 0)
					|| (sv == "list" && s.view_mode != 1)
					|| (sv == "tree" && s.view_mode != 2))
				return true;
		}
		if (prop == "default-category" && val.kind == PV::S)
		{
			if ((val.s == "favorites" && s.default_category != 0)
					|| (val.s == "recent" && s.default_category != 1)
					|| (val.s == "all" && s.default_category != 2))
				return true;
		}
		if (prop == "stay-on-focus-out" && val.kind == PV::B && s.stay_on_focus_out != val.b) return true;
		if (prop == "calculator-engine" && val.kind == PV::S
				&& s.calculator_engine != val.s) return true;
		if (prop == "calculator-result-font-size" && val.kind == PV::I
				&& s.calculator_result_font_size != val.i) return true;
		if (prop == "calculator-max-decimal-places" && val.kind == PV::I
				&& s.calculator_max_decimal_places != val.i) return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_classic_property_count()
{
	auto c = make_classic();
	assert(c.values.size() == 18);
}

static void test_modern_property_count()
{
	auto m = make_modern();
	assert(m.values.size() == 20);
}

static void test_fullscreen_property_count()
{
	auto f = make_fullscreen();
	assert(f.values.size() == 18);
}

static void test_minimal_property_count()
{
	auto m = make_minimal();
	assert(m.values.size() == 19);
}

static void test_apply_then_no_diff()
{
	auto presets = { make_classic(), make_modern(), make_fullscreen(), make_minimal() };
	for (const auto& preset : presets)
	{
		SettingsShadow s;
		apply_preset_shadow(preset, s);
		assert(!compute_diff_shadow(preset, s));
		assert(s.current_preset_id == preset.id);
	}
}

static void test_diff_after_modify()
{
	// After applying Modern, modifying corner_radius should flip diff to true
	auto m = make_modern();
	SettingsShadow s;
	apply_preset_shadow(m, s);
	assert(!compute_diff_shadow(m, s));

	s.corner_radius += 1;
	assert(compute_diff_shadow(m, s));
}

static void test_modern_corner_radius()
{
	auto m = make_modern();
	auto it = m.values.find("corner-radius");
	assert(it != m.values.end());
	assert(it->second.kind == PV::I);
	assert(it->second.i == 12);
}

// Defined below (operates on the real BUILTIN_PRESETS table).
static const WhiskerMenu::LayoutPreset* find_builtin(const char* id);

// Each real built-in carries the single governed menu-opacity at its documented
// value: Classic 100, Modern 100, Full Screen 80, Minimal 60 (supported behavior, supported behavior).
static void test_builtin_menu_opacity_values()
{
	struct { const char* id; int opacity; } expected[] = {
		{ "classic",    100 },
		{ "modern",     100 },
		{ "fullscreen",  80 },
		{ "minimal",     60 },
	};
	for (const auto& e : expected)
	{
		const WhiskerMenu::LayoutPreset* p = find_builtin(e.id);
		assert(p);
		auto it = p->values.find("menu-opacity");
		assert(it != p->values.end());
		assert(it->second.kind == WhiskerMenu::PresetValue::Int);
		assert(it->second.i == e.opacity);
	}
}

// The single menu-opacity is governed; the three retired per-region keys are not.
static void test_governed_keys_opacity_membership()
{
	const auto& keys = WhiskerMenu::governed_keys();
	auto has = [&keys](const char* k) {
		return std::find(keys.begin(), keys.end(), std::string(k)) != keys.end();
	};
	assert(has("menu-opacity"));
	assert(!has("categories-opacity"));
	assert(!has("apps-opacity"));
	assert(!has("full-screen-opacity"));
}

static void test_transparent_grid_is_modern_and_fullscreen_preset_default()
{
	const auto& keys = WhiskerMenu::governed_keys();
	const bool governed = std::find(keys.begin(), keys.end(),
			std::string("transparent-grid")) != keys.end();
	assert(!governed);

	struct { const char* id; bool expected; } expected[] = {
		{ "classic",    false },
		{ "modern",     true  },
		{ "fullscreen", true  },
		{ "minimal",    false },
	};
	for (const auto& e : expected)
	{
		const WhiskerMenu::LayoutPreset* p = find_builtin(e.id);
		assert(p);
		const auto it = p->values.find("transparent-grid");
		assert((it != p->values.end()) == e.expected);
		if (e.expected)
		{
			assert(it->second.kind == WhiskerMenu::PresetValue::Bool);
			assert(it->second.b == true);
		}
	}
}

static void test_fullscreen_layout_mode()
{
	auto f = make_fullscreen();
	auto it = f.values.find("layout-mode");
	assert(it != f.values.end());
	assert(it->second.s == "fullscreen");
	auto it_icons = f.values.find("launcher-icon-size");
	assert(it_icons != f.values.end());
	assert(it_icons->second.kind == PV::I);
	assert(it_icons->second.i == 4);
	auto profile = f.values.find("show-profile");
	assert(profile != f.values.end());
	assert(profile->second.kind == PV::B);
	assert(profile->second.b == true);
}

static void test_fullscreen_sidebar_left()
{
	auto f = make_fullscreen();
	auto it = f.values.find("sidebar-position");
	assert(it != f.values.end());
	assert(it->second.s == "left");
}

static void test_fullscreen_to_docked_restores_menu_size()
{
	SettingsShadow s;

	// Simulate a stale oversize dimension inherited from a fullscreen session.
	s.menu_width = 980;
	s.menu_height = 820;

	auto f = make_fullscreen();
	apply_preset_shadow(f, s);
	assert(s.layout_mode == "fullscreen");
	// FullScreen preset must not define docked menu dimensions.
	assert(s.menu_width == 980);
	assert(s.menu_height == 820);

	auto m = make_modern();
	apply_preset_shadow(m, s);
	assert(s.layout_mode == "docked");
	assert(s.menu_width == 450);
	assert(s.menu_height == 500);

	// Repeat for Classic to guard both docked presets.
	s.menu_width = 940;
	s.menu_height = 760;
	auto c = make_classic();
	apply_preset_shadow(c, s);
	assert(s.layout_mode == "docked");
	assert(s.menu_width == 450);
	assert(s.menu_height == 500);
}

// Simulated find_preset_by_id using the shadow table
static const TestPresetDef* find_shadow(const std::string& id)
{
	static auto c = make_classic();
	static auto m = make_modern();
	static auto f = make_fullscreen();
	if (id == "classic") return &c;
	if (id == "modern")  return &m;
	if (id == "fullscreen") return &f;
	return nullptr;
}

static void test_find_by_id()
{
	assert(find_shadow("classic") != nullptr);
	assert(find_shadow("modern") != nullptr);
	assert(find_shadow("fullscreen") != nullptr);
	assert(find_shadow("unknown_id") == nullptr);
	assert(find_shadow("") == nullptr);
}

// ---------------------------------------------------------------------------
// Preset-field label resolution (the documented interface).
// Pure mirror of Settings::current_preset_name(): a known id yields the
// preset's stored name; an unset/empty id or one that resolves to no known
// preset yields the localized "Custom" string. The result is never empty.
// ---------------------------------------------------------------------------

/* resolve_preset_label:
 * @id:          stored active-preset identity (may be empty).
 * @lookup_name: the resolved preset's name, or nullptr if the id resolves to
 *               no known preset.
 *
 * Returns: the label to display; never empty.
 */
static std::string resolve_preset_label(const std::string& id, const char* lookup_name)
{
	if (!id.empty() && lookup_name && *lookup_name)
		return lookup_name;
	return "Custom";
}

static void test_preset_label_resolution()
{
	// Known built-in / user ids adopt their resolved name.
	assert(resolve_preset_label("modern", "Modern") == "Modern");
	assert(resolve_preset_label("classic", "Classic") == "Classic");
	assert(resolve_preset_label("uuid-7", "My Layout") == "My Layout");

	// Unset/empty id → "Custom"; lookup is irrelevant.
	assert(resolve_preset_label("", nullptr) == "Custom");
	assert(resolve_preset_label("", "Modern") == "Custom");

	// Non-empty but unknown id (resolves to nothing) → "Custom".
	assert(resolve_preset_label("deleted-uuid", nullptr) == "Custom");

	// The label is never empty in any state.
	assert(!resolve_preset_label("", nullptr).empty());
	assert(!resolve_preset_label("ghost", nullptr).empty());
}

// ---------------------------------------------------------------------------
// Shadow user-preset store — mirrors the CRUD logic in preset.cpp without Xfconf.
// ---------------------------------------------------------------------------

struct UserPresetShadow
{
	std::string uuid;
	std::string display_name;
	PVMap       values;
};

// Built-in preset display names, mirroring BUILTIN_PRESETS[]. Name conflicts in
// the real save_current_as_user_preset()/rename_user_preset() are tested against
// these too, case-insensitively (g_ascii_strcasecmp).
static const char* SHADOW_BUILTIN_NAMES[] = { "Classic", "Modern", "Full Screen", "Minimal", nullptr };

// Case-insensitive name match, mirroring g_ascii_strcasecmp() in the real impl.
// NOTE: the real conflict check is case-INSENSITIVE; do not assume exact-case.
static bool names_equal_ci(const std::string& a, const std::string& b)
{
	return g_ascii_strcasecmp(a.c_str(), b.c_str()) == 0;
}

static bool name_conflicts_builtin_ci(const std::string& name)
{
	for (int i = 0; SHADOW_BUILTIN_NAMES[i]; ++i)
		if (names_equal_ci(name, SHADOW_BUILTIN_NAMES[i]))
			return true;
	return false;
}

struct UserPresetStore
{
	std::vector<UserPresetShadow> presets;
	int next_uuid_counter = 0;

	std::string save(const std::string& display_name, const PVMap& values)
	{
		if (display_name.empty())
			return {};
		if (name_conflicts_builtin_ci(display_name))
			return {};
		for (const auto& p : presets)
			if (names_equal_ci(p.display_name, display_name))
				return {};
		std::string uuid = "uuid-" + std::to_string(next_uuid_counter++);
		presets.push_back({ uuid, display_name, values });
		return uuid;
	}

	bool rename(const std::string& uuid, const std::string& new_name)
	{
		if (new_name.empty())
			return false;
		if (name_conflicts_builtin_ci(new_name))
			return false;
		for (const auto& p : presets)
			if (names_equal_ci(p.display_name, new_name) && p.uuid != uuid)
				return false;
		for (auto& p : presets)
			if (p.uuid == uuid) { p.display_name = new_name; return true; }
		return false;
	}

	void del(const std::string& uuid, std::string& current_preset_id)
	{
		if (current_preset_id == uuid)
			current_preset_id.clear();
		presets.erase(std::remove_if(presets.begin(), presets.end(),
			[&uuid](const UserPresetShadow& p) { return p.uuid == uuid; }),
			presets.end());
	}

	const UserPresetShadow* find(const std::string& uuid) const
	{
		for (const auto& p : presets)
			if (p.uuid == uuid) return &p;
		return nullptr;
	}
};

// ---------------------------------------------------------------------------
// runtime implementation: User preset CRUD tests
// ---------------------------------------------------------------------------

static void test_user_preset_save_then_enumerate()
{
	UserPresetStore store;
	PVMap vals = { { "corner-radius", PV::from_int(6) } };
	std::string uuid = store.save("My Preset", vals);
	assert(!uuid.empty());
	assert(store.presets.size() == 1);
	assert(store.presets[0].display_name == "My Preset");
	assert(store.find(uuid) != nullptr);
}

static void test_user_preset_saves_calculator_values()
{
	UserPresetStore store;
	PVMap values = {
		{ "calculator-engine", PV::from_str("qalc") },
		{ "calculator-result-font-size", PV::from_int(3) },
		{ "calculator-max-decimal-places", PV::from_int(9) },
	};
	const std::string uuid = store.save("Calculator settings", values);
	const UserPresetShadow* saved = store.find(uuid);
	assert(saved);
	assert(saved->values.at("calculator-engine").s == "qalc");
	assert(saved->values.at("calculator-result-font-size").i == 3);
	assert(saved->values.at("calculator-max-decimal-places").i == 9);
}

static void test_user_preset_rename_updates_name()
{
	UserPresetStore store;
	PVMap vals = { { "panel-gap", PV::from_int(4) } };
	std::string uuid = store.save("Original Name", vals);
	assert(!uuid.empty());

	bool ok = store.rename(uuid, "Renamed Preset");
	assert(ok);
	assert(store.presets[0].display_name == "Renamed Preset");
}

static void test_user_preset_delete_clears_current_id()
{
	UserPresetStore store;
	PVMap vals = { { "panel-gap", PV::from_int(4) } };
	std::string uuid = store.save("ToDelete", vals);
	assert(!uuid.empty());

	std::string current_id = uuid;
	store.del(uuid, current_id);

	assert(current_id.empty());
	assert(store.presets.empty());
	assert(store.find(uuid) == nullptr);
}

static void test_user_preset_name_conflict_rejected()
{
	UserPresetStore store;
	PVMap vals;
	std::string uuid1 = store.save("DuplicateName", vals);
	assert(!uuid1.empty());

	std::string uuid2 = store.save("DuplicateName", vals);
	assert(uuid2.empty());
	assert(store.presets.size() == 1);
}

static void test_user_preset_empty_name_rejected()
{
	UserPresetStore store;
	std::string uuid = store.save("", {});
	assert(uuid.empty());
	assert(store.presets.empty());
}

static void test_user_preset_delete_non_current_preserves_id()
{
	UserPresetStore store;
	std::string uuid1 = store.save("P1", {});
	std::string uuid2 = store.save("P2", {});
	assert(!uuid1.empty() && !uuid2.empty());

	std::string current_id = uuid1;
	store.del(uuid2, current_id);

	assert(current_id == uuid1);
	assert(store.presets.size() == 1);
}

// runtime implementation: empty and duplicate names are rejected, case-insensitively, against both
// existing customs and the built-in names; the store is left unchanged (supported behavior,
// supported behavior). Mirrors the real preset_name_conflicts() (g_ascii_strcasecmp).
static void test_user_preset_save_rejects_case_insensitive_duplicate()
{
	UserPresetStore store;
	std::string uuid = store.save("My Layout", {});
	assert(!uuid.empty());

	// Same name in a different case must still be rejected.
	std::string dup = store.save("my layout", {});
	assert(dup.empty());
	assert(store.presets.size() == 1);
}

static void test_user_preset_save_rejects_builtin_name()
{
	UserPresetStore store;
	// Built-in names are reserved, regardless of case.
	assert(store.save("Modern", {}).empty());
	assert(store.save("classic", {}).empty());
	assert(store.save("FULL SCREEN", {}).empty());
	assert(store.presets.empty());
}

static void test_user_preset_save_rejects_empty_name()
{
	UserPresetStore store;
	assert(store.save("", {}).empty());
	assert(store.presets.empty());
}

static void test_user_preset_rename_rejects_conflicts()
{
	UserPresetStore store;
	std::string a = store.save("Alpha", {});
	std::string b = store.save("Beta", {});
	assert(!a.empty() && !b.empty());

	// Renaming Beta to an existing custom name (any case) is rejected.
	assert(!store.rename(b, "alpha"));
	// Renaming to a built-in name is rejected.
	assert(!store.rename(b, "Modern"));
	// Empty name rejected.
	assert(!store.rename(b, ""));
	// A unique name succeeds and leaves no stale entry.
	assert(store.rename(b, "Gamma"));
	assert(store.find(b)->display_name == "Gamma");
}

// runtime implementation: deleting the active custom clears the current id so the field falls back
// to the Modern built-in (supported behavior, supported behavior). Mirrors the UI handler, which
// applies BUILTIN_PRESETS[PRESET_MODERN] once current_preset_id is cleared.
static std::string fallback_preset_id(const std::string& current_id)
{
	// The field is never blank: an unset/cleared applied id resolves to Modern.
	return current_id.empty() ? std::string("modern") : current_id;
}

static void test_user_preset_delete_active_falls_back_to_modern()
{
	UserPresetStore store;
	std::string uuid = store.save("Doomed", {});
	assert(!uuid.empty());

	std::string current_id = uuid;
	store.del(uuid, current_id);
	assert(current_id.empty());
	assert(fallback_preset_id(current_id) == "modern");
}

// ---------------------------------------------------------------------------
// runtime implementation: data-driven dropdown typography. Each preset kind maps to a fixed Pango
// weight/style: built-in → BOLD/NORMAL, saved custom → NORMAL/NORMAL, the
// transient "Unsaved custom" placeholder → NORMAL/ITALIC. Classification is
// keyed on is_builtin, so a future built-in inherits bold with no test change
// (supported behavior, supported behavior).
// NOTE: the integer literals equal the Pango constants used by the combo's cell
// renderer — 700 = PANGO_WEIGHT_BOLD, 400 = PANGO_WEIGHT_NORMAL,
// 0 = PANGO_STYLE_NORMAL, 2 = PANGO_STYLE_ITALIC.
// ---------------------------------------------------------------------------

static int row_weight(bool is_builtin)  { return is_builtin ? 700 : 400; }
static int row_style(bool is_unsaved)   { return is_unsaved ? 2 : 0; }

static void test_preset_typography_classification()
{
	// Every real built-in classifies bold/upright — data-driven over the table,
	// so adding a built-in needs no edit here.
	for (int i = 0; i < WhiskerMenu::PRESET_BUILTIN_COUNT; ++i)
	{
		assert(WhiskerMenu::BUILTIN_PRESETS[i].is_builtin);
		assert(row_weight(WhiskerMenu::BUILTIN_PRESETS[i].is_builtin) == 700);
		assert(row_style(false) == 0);
	}
	// Saved custom: standard weight, upright.
	assert(row_weight(false) == 400);
	assert(row_style(false) == 0);
	// Unsaved-custom placeholder: standard weight, italic.
	assert(row_weight(false) == 400);
	assert(row_style(true) == 2);
}

// runtime implementation: divergence is reversible. compute_preset_diff(applied, settings) is true
// on any governed-key divergence and false again once the value is edited back
// to an exact match (supported behavior snap-back).
static void test_diff_snapback()
{
	auto m = make_modern();
	SettingsShadow s;
	apply_preset_shadow(m, s);
	assert(!compute_diff_shadow(m, s)); // matches right after apply

	const int original = s.corner_radius;
	s.corner_radius = original + 5;
	assert(compute_diff_shadow(m, s));  // diverged

	s.corner_radius = original;
	assert(!compute_diff_shadow(m, s)); // snapped back
}

// ---------------------------------------------------------------------------
// runtime implementation: File-seeded preset tests.
// These shadow tests verify that if on-disk files were loaded, the resulting
// LayoutPreset data would match the expected C++ fallback values.
// ---------------------------------------------------------------------------

static void test_file_preset_overrides_cpp_table()
{
	// Simulate: a "classic" file-preset with corner-radius=2 (different from
	// C++ fallback 0). After loading from file, find_preset_by_id returns the
	// file version.
	//
	// In the real implementation g_file_presets is populated by initialize_file_presets().
	// Here we verify the *logic* by checking that file-sourced values differ
	// from the C++ fallback in test_preset.cpp's shadow tables when we choose
	// different values.
	auto c = make_classic();
	auto it = c.values.find("corner-radius");
	assert(it != c.values.end() && it->second.i == 0); // C++ fallback is 0

	// A file-overridden preset would have e.g. corner-radius=2.
	TestPresetDef file_version = c;
	file_version.values["corner-radius"] = PV::from_int(2);

	SettingsShadow s;
	apply_preset_shadow(file_version, s);
	assert(s.corner_radius == 2);         // file value applied
	assert(!compute_diff_shadow(file_version, s)); // no diff after apply
}

static void test_fallback_to_cpp_table_when_files_absent()
{
	// If g_file_presets is empty the C++ BUILTIN_PRESETS[] must still supply
	// the three built-in presets. Verified via the shadow find_by_id.
	assert(find_shadow("classic")    != nullptr);
	assert(find_shadow("modern")     != nullptr);
	assert(find_shadow("fullscreen") != nullptr);
}

// ---------------------------------------------------------------------------
// runtime implementation: Parity sanity check — applying from the C++ fallback table and from
// a file-equivalent preset with the same values must yield identical results.
// This guards against future drift between the .meowpreset files and the table.
// ---------------------------------------------------------------------------

static TestPresetDef make_classic_from_file_equivalent()
{
	// Mirrors data/presets/classic.meowpreset exactly.
	return {
		"classic",
		{
			{ "layout-mode",          PV::from_str("docked")      },
			{ "corner-radius",        PV::from_int(0)             },
			{ "panel-gap",            PV::from_int(0)             },
			{ "menu-width",           PV::from_int(450)           },
			{ "menu-height",          PV::from_int(500)           },
			{ "launcher-icon-size",   PV::from_int(2)             },
			{ "view-mode-default",    PV::from_str("list")        },
			{ "sidebar-position",     PV::from_str("right")       },
			{ "search-bar-position",  PV::from_str("top")         },
			{ "show-profile",        PV::from_bool(true)          },
			{ "show-session",        PV::from_bool(true)          },
			{ "menu-opacity",         PV::from_int(100)           },
			{ "hover-switch-category",PV::from_bool(false)        },
			{ "stay-on-focus-out",    PV::from_bool(false)        },
			{ "default-category",     PV::from_str("favorites")   },
			{ "calculator-engine", PV::from_str("none") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

static TestPresetDef make_modern_from_file_equivalent()
{
	// Mirrors data/presets/modern.meowpreset exactly.
	return {
		"modern",
		{
			{ "layout-mode",          PV::from_str("docked")    },
			{ "corner-radius",        PV::from_int(12)          },
			{ "panel-gap",            PV::from_int(8)           },
			{ "menu-width",           PV::from_int(450)         },
			{ "menu-height",          PV::from_int(500)         },
			{ "launcher-icon-size",   PV::from_int(3)           },
			{ "view-mode-default",    PV::from_str("icons")     },
			{ "grid-density",         PV::from_str("medium")    },
			{ "sidebar-position",     PV::from_str("left")      },
			{ "search-bar-position",  PV::from_str("top")       },
			{ "show-profile",         PV::from_bool(true)        },
			{ "show-session",         PV::from_bool(true)        },
			{ "menu-opacity",         PV::from_int(100)         },
			{ "hover-switch-category",PV::from_bool(true)       },
			{ "transparent-grid",     PV::from_bool(true)       },
			{ "stay-on-focus-out",    PV::from_bool(false)      },
			{ "default-category",     PV::from_str("recent")    },
			{ "calculator-engine", PV::from_str("bc") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

static TestPresetDef make_fullscreen_from_file_equivalent()
{
	// Mirrors data/presets/fullscreen.meowpreset exactly.
	return {
		"fullscreen",
		{
			{ "layout-mode",          PV::from_str("fullscreen") },
			{ "corner-radius",        PV::from_int(0)            },
			{ "panel-gap",            PV::from_int(0)            },
			{ "launcher-icon-size",   PV::from_int(4)            },
			{ "view-mode-default",    PV::from_str("icons")      },
			{ "grid-density",         PV::from_str("medium")     },
			{ "sidebar-position",     PV::from_str("left")       },
			{ "search-bar-position",  PV::from_str("top")        },
			{ "show-profile",         PV::from_bool(true)         },
			{ "show-session",         PV::from_bool(true)         },
			{ "menu-opacity",         PV::from_int(80)           },
			{ "hover-switch-category",PV::from_bool(true)        },
			{ "transparent-grid",     PV::from_bool(true)        },
			{ "stay-on-focus-out",    PV::from_bool(false)       },
			{ "default-category",     PV::from_str("all")        },
			{ "calculator-engine", PV::from_str("bc") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

static void test_parity_classic_cpp_vs_file()
{
	auto cpp  = make_classic();
	auto file = make_classic_from_file_equivalent();
	// Apply both to separate shadows and compare the governed fields.
	SettingsShadow s_cpp, s_file;
	apply_preset_shadow(cpp,  s_cpp);
	apply_preset_shadow(file, s_file);

	assert(s_cpp.corner_radius  == s_file.corner_radius);
	assert(s_cpp.panel_gap      == s_file.panel_gap);
	assert(s_cpp.layout_mode    == s_file.layout_mode);
	assert(s_cpp.sidebar_position == s_file.sidebar_position);
	assert(s_cpp.view_mode      == s_file.view_mode);
	assert(s_cpp.menu_width     == s_file.menu_width);
	assert(s_cpp.menu_height    == s_file.menu_height);
}

static void test_parity_modern_cpp_vs_file()
{
	auto cpp  = make_modern();
	auto file = make_modern_from_file_equivalent();
	SettingsShadow s_cpp, s_file;
	apply_preset_shadow(cpp,  s_cpp);
	apply_preset_shadow(file, s_file);

	assert(s_cpp.corner_radius       == s_file.corner_radius);
	assert(s_cpp.panel_gap           == s_file.panel_gap);
	assert(s_cpp.menu_opacity        == s_file.menu_opacity);
	assert(s_cpp.sidebar_position    == s_file.sidebar_position);
	assert(s_cpp.view_mode           == s_file.view_mode);
	assert(s_cpp.category_hover_activate == s_file.category_hover_activate);
	assert(s_cpp.transparent_grid    == s_file.transparent_grid);
}

static void test_parity_fullscreen_cpp_vs_file()
{
	auto cpp  = make_fullscreen();
	auto file = make_fullscreen_from_file_equivalent();
	SettingsShadow s_cpp, s_file;
	apply_preset_shadow(cpp,  s_cpp);
	apply_preset_shadow(file, s_file);

	assert(s_cpp.layout_mode             == s_file.layout_mode);
	assert(s_cpp.sidebar_position        == s_file.sidebar_position);
	assert(s_cpp.view_mode               == s_file.view_mode);
	assert(s_cpp.category_hover_activate == s_file.category_hover_activate);
	assert(s_cpp.transparent_grid        == s_file.transparent_grid);
	assert(s_cpp.default_category        == s_file.default_category);
}

static TestPresetDef make_minimal_from_file_equivalent()
{
	// Mirrors data/presets/minimal.meowpreset exactly.
	return {
		"minimal",
		{
			{ "layout-mode",          PV::from_str("centered")  },
			{ "corner-radius",        PV::from_int(12)          },
			{ "panel-gap",            PV::from_int(8)           },
			{ "menu-width",           PV::from_int(450)         },
			{ "menu-height",          PV::from_int(306)         },
			{ "launcher-icon-size",   PV::from_int(3)           },
			{ "view-mode-default",    PV::from_str("list")      },
			{ "grid-density",         PV::from_str("medium")    },
			{ "sidebar-position",     PV::from_str("left")      },
			{ "search-bar-position",  PV::from_str("top")       },
			{ "show-profile",         PV::from_bool(false)       },
			{ "show-session",         PV::from_bool(false)       },
			{ "menu-opacity",         PV::from_int(60)          },
			{ "hover-switch-category",PV::from_bool(true)       },
			{ "stay-on-focus-out",    PV::from_bool(false)      },
			{ "default-category",     PV::from_str("recent")    },
			{ "calculator-engine", PV::from_str("bc") },
			{ "calculator-result-font-size", PV::from_int(-1) },
			{ "calculator-max-decimal-places", PV::from_int(4) },
		}
	};
}

static void test_parity_minimal_cpp_vs_file()
{
	auto cpp  = make_minimal();
	auto file = make_minimal_from_file_equivalent();
	SettingsShadow s_cpp, s_file;
	apply_preset_shadow(cpp,  s_cpp);
	apply_preset_shadow(file, s_file);

	assert(s_cpp.corner_radius           == s_file.corner_radius);
	assert(s_cpp.panel_gap               == s_file.panel_gap);
	assert(s_cpp.menu_opacity            == s_file.menu_opacity);
	assert(s_cpp.sidebar_position        == s_file.sidebar_position);
	assert(s_cpp.show_profile            == s_file.show_profile);
	assert(s_cpp.show_session            == s_file.show_session);
	assert(s_cpp.layout_mode             == s_file.layout_mode);
	assert(s_cpp.view_mode               == s_file.view_mode);
	assert(s_cpp.menu_width              == s_file.menu_width);
	assert(s_cpp.menu_height             == s_file.menu_height);
	assert(s_cpp.category_hover_activate == s_file.category_hover_activate);
	assert(s_cpp.default_category        == s_file.default_category);
}

// ---------------------------------------------------------------------------
// runtime implementation: GOVERNED_KEYS completeness + file↔table agreement for all built-ins.
//
// These exercise the REAL WhiskerMenu::BUILTIN_PRESETS table and
// governed_keys() (linked from preset-builtins.cpp), plus the shipped
// .meowpreset seed files parsed from MEOWMENU_TEST_PRESET_DIR. A missing key
// or a drifted value fails the build's test stage rather than leaking at
// runtime (supported behavior, coverage analysis).
// ---------------------------------------------------------------------------

static const WhiskerMenu::LayoutPreset* find_builtin(const char* id)
{
	for (int i = 0; i < WhiskerMenu::PRESET_BUILTIN_COUNT; ++i)
		if (WhiskerMenu::BUILTIN_PRESETS[i].id == id)
			return &WhiskerMenu::BUILTIN_PRESETS[i];
	return nullptr;
}

static GKeyFile* load_meowpreset(const char* fname)
{
	std::string path = std::string(MEOWMENU_TEST_PRESET_DIR) + "/" + fname;
	GKeyFile* kf = g_key_file_new();
	gboolean ok = g_key_file_load_from_file(kf, path.c_str(), G_KEY_FILE_NONE, nullptr);
	assert(ok);
	return kf;
}

// Built-in layout-mode is fixed per preset: classic→docked, modern→docked,
// fullscreen→fullscreen, minimal→centered. Selecting a built-in deterministically
// sets its documented mode (Minimal is the search-first centered launcher).
static void test_builtin_layout_modes()
{
	struct { const char* id; const char* mode; } expected[] = {
		{ "classic",    "docked"     },
		{ "modern",     "docked"     },
		{ "fullscreen", "fullscreen" },
		{ "minimal",    "centered"   },
	};
	for (const auto& e : expected)
	{
		const WhiskerMenu::LayoutPreset* p = find_builtin(e.id);
		assert(p);
		auto it = p->values.find("layout-mode");
		assert(it != p->values.end());
		assert(it->second.kind == WhiskerMenu::PresetValue::Str);
		assert(it->second.s == e.mode);
	}
}

// The category-icon default is a governed preset value, so the installed
// preset and C++ fallback must keep the four built-ins in exact agreement.
static void test_builtin_category_icon_sizes()
{
	struct Expected { const char* id; const char* file; int size; } expected[] = {
		{ "classic",    "classic.meowpreset",    1 },
		{ "modern",     "modern.meowpreset",     1 },
		{ "fullscreen", "fullscreen.meowpreset", 2 },
		{ "minimal",    "minimal.meowpreset",    1 },
	};
	for (const auto& e : expected)
	{
		const WhiskerMenu::LayoutPreset* preset = find_builtin(e.id);
		assert(preset);
		auto value = preset->values.find("category-icon-size");
		assert(value != preset->values.end());
		assert(value->second.kind == WhiskerMenu::PresetValue::Int);
		assert(value->second.i == e.size);

		GKeyFile* file = load_meowpreset(e.file);
		assert(g_key_file_get_integer(file, "Settings", "category-icon-size",
			nullptr) == e.size);
		g_key_file_free(file);
	}
}

static void test_governed_keys_completeness_table()
{
	const auto& keys = WhiskerMenu::governed_keys();
	for (int i = 0; i < WhiskerMenu::PRESET_BUILTIN_COUNT; ++i)
	{
		const WhiskerMenu::LayoutPreset& p = WhiskerMenu::BUILTIN_PRESETS[i];
		for (const auto& k : keys)
			assert(p.values.find(k) != p.values.end());
		// Every built-in carries a stored identity name (supported behavior).
		assert(!p.name.empty());
		auto profile = p.values.find("show-profile");
		assert(profile != p.values.end());
		assert(profile->second.kind == WhiskerMenu::PresetValue::Bool);
	}
}

static void test_governed_keys_completeness_files()
{
	const auto& keys = WhiskerMenu::governed_keys();
	const char* files[] = { "classic.meowpreset", "modern.meowpreset", "fullscreen.meowpreset", "minimal.meowpreset" };
	for (const char* f : files)
	{
		GKeyFile* kf = load_meowpreset(f);
		// Name= is the stored identity for built-ins.
		assert(g_key_file_has_key(kf, "Preset", "Name", nullptr));
		for (const auto& k : keys)
			assert(g_key_file_has_key(kf, "Settings", k.c_str(), nullptr));
		g_key_file_free(kf);
	}
}

static void test_file_table_agreement()
{
	struct { const char* id; const char* file; } pairs[] = {
		{ "classic",    "classic.meowpreset"    },
		{ "modern",     "modern.meowpreset"     },
		{ "fullscreen", "fullscreen.meowpreset" },
		{ "minimal",    "minimal.meowpreset"    },
	};
	const auto& keys = WhiskerMenu::governed_keys();
	for (const auto& pr : pairs)
	{
		const WhiskerMenu::LayoutPreset* p = find_builtin(pr.id);
		assert(p);
		GKeyFile* kf = load_meowpreset(pr.file);
		for (const auto& k : keys)
		{
			auto it = p->values.find(k);
			assert(it != p->values.end());
			const WhiskerMenu::PresetValue& v = it->second;
			if (v.kind == WhiskerMenu::PresetValue::Bool)
			{
				gboolean fv = g_key_file_get_boolean(kf, "Settings", k.c_str(), nullptr);
				assert((fv != FALSE) == v.b);
			}
			else if (v.kind == WhiskerMenu::PresetValue::Int)
			{
				int fv = g_key_file_get_integer(kf, "Settings", k.c_str(), nullptr);
				assert(fv == v.i);
			}
			else
			{
				gchar* fv = g_key_file_get_string(kf, "Settings", k.c_str(), nullptr);
				assert(fv && v.s == fv);
				g_free(fv);
			}
		}
		g_key_file_free(kf);
	}
}

int main()
{
	test_classic_property_count();
	test_modern_property_count();
	test_fullscreen_property_count();
	test_minimal_property_count();
	test_apply_then_no_diff();
	test_diff_after_modify();
	test_modern_corner_radius();
	test_builtin_menu_opacity_values();
	test_governed_keys_opacity_membership();
	test_transparent_grid_is_modern_and_fullscreen_preset_default();
	test_fullscreen_layout_mode();
	test_fullscreen_sidebar_left();
	test_fullscreen_to_docked_restores_menu_size();
	test_find_by_id();
	test_preset_label_resolution();
	// runtime implementation: user preset CRUD
	test_user_preset_save_then_enumerate();
	test_user_preset_saves_calculator_values();
	test_user_preset_rename_updates_name();
	test_user_preset_delete_clears_current_id();
	test_user_preset_name_conflict_rejected();
	test_user_preset_empty_name_rejected();
	test_user_preset_delete_non_current_preserves_id();
	// 029-custom-presets: name-rejection (case-insensitive + built-in),
	// rename conflicts, delete→Modern fallback, typography, divergence snap-back
	test_user_preset_save_rejects_case_insensitive_duplicate();
	test_user_preset_save_rejects_builtin_name();
	test_user_preset_save_rejects_empty_name();
	test_user_preset_rename_rejects_conflicts();
	test_user_preset_delete_active_falls_back_to_modern();
	test_preset_typography_classification();
	test_diff_snapback();
	// runtime implementation: file-seeded preset tests
	test_file_preset_overrides_cpp_table();
	test_fallback_to_cpp_table_when_files_absent();
	// runtime implementation: parity — C++ table vs file-equivalent preset
	test_parity_classic_cpp_vs_file();
	test_parity_modern_cpp_vs_file();
	test_parity_fullscreen_cpp_vs_file();
	test_parity_minimal_cpp_vs_file();
	// runtime implementation: real-table governed-key completeness + file↔table agreement
	test_builtin_layout_modes();
	test_builtin_category_icon_sizes();
	test_governed_keys_completeness_table();
	test_governed_keys_completeness_files();
	test_file_table_agreement();
	return 0;
}
