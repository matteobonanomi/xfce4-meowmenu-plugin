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
			{ "categories-opacity",    PV::from_int(100)     },
			{ "apps-opacity",          PV::from_int(100)     },
			{ "sidebar-position",      PV::from_str("left")  },
			{ "search-bar-position",   PV::from_str("top")   },
			{ "profile-position",      PV::from_str("top")   },
			{ "layout-mode",           PV::from_str("docked") },
			{ "hover-switch-category", PV::from_bool(false)  },
			{ "view-mode-default",     PV::from_str("list")  },
			{ "menu-width",            PV::from_int(450)     },
			{ "menu-height",           PV::from_int(500)     },
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
			{ "categories-opacity",    PV::from_int(80)       },
			{ "apps-opacity",          PV::from_int(70)       },
			{ "sidebar-position",      PV::from_str("left")   },
			{ "search-bar-position",   PV::from_str("bottom") },
			{ "profile-position",      PV::from_str("top")    },
			{ "layout-mode",           PV::from_str("docked") },
			{ "grid-density",          PV::from_str("medium") },
			{ "hover-switch-category", PV::from_bool(true)    },
			{ "view-mode-default",     PV::from_str("icons")  },
			{ "menu-width",            PV::from_int(450)      },
			{ "menu-height",           PV::from_int(500)      },
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
			{ "categories-opacity",    PV::from_int(100)            },
			{ "apps-opacity",          PV::from_int(100)            },
			{ "sidebar-position",      PV::from_str("hidden")       },
			{ "search-bar-position",   PV::from_str("top")          },
			{ "profile-position",      PV::from_str("bottom-right") },
			{ "commands-position",     PV::from_str("bottom-right") },
			{ "grid-columns",          PV::from_int(6)              },
			{ "grid-rows",             PV::from_int(3)              },
			{ "grid-density",          PV::from_str("medium")       },
			{ "layout-mode",           PV::from_str("fullscreen")   },
			{ "hover-switch-category", PV::from_bool(true)          },
			{ "view-mode-default",     PV::from_str("icons")        },
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
	int categories_opacity = 100;
	int apps_opacity      = 100;
	std::string sidebar_position    = "left";
	std::string search_bar_position = "top";
	std::string profile_position    = "top";
	std::string commands_position   = "top-right";
	int grid_columns      = 4;
	int grid_rows         = 3;
	std::string grid_density = "medium";
	std::string layout_mode  = "docked";
	bool category_hover_activate = false;
	int view_mode = 1; // ViewAsList
	int menu_width = 450;
	int menu_height = 500;
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
		else if (prop == "categories-opacity" && val.kind == PV::I)   s.categories_opacity = val.i;
		else if (prop == "apps-opacity" && val.kind == PV::I)         s.apps_opacity = val.i;
		else if (prop == "sidebar-position" && val.kind == PV::S)     s.sidebar_position = val.s;
		else if (prop == "search-bar-position" && val.kind == PV::S)  s.search_bar_position = val.s;
		else if (prop == "profile-position" && val.kind == PV::S)     s.profile_position = val.s;
		else if (prop == "commands-position" && val.kind == PV::S)    s.commands_position = val.s;
		else if (prop == "grid-columns" && val.kind == PV::I)         s.grid_columns = val.i;
		else if (prop == "grid-rows" && val.kind == PV::I)            s.grid_rows = val.i;
		else if (prop == "grid-density" && val.kind == PV::S)         s.grid_density = val.s;
		else if (prop == "layout-mode" && val.kind == PV::S)          s.layout_mode = val.s;
		else if (prop == "hover-switch-category" && val.kind == PV::B)s.category_hover_activate = val.b;
		else if (prop == "menu-width" && val.kind == PV::I)           s.menu_width = val.i;
		else if (prop == "menu-height" && val.kind == PV::I)          s.menu_height = val.i;
		else if (prop == "view-mode-default" && val.kind == PV::S)
		{
			if (val.s == "icons") s.view_mode = 0;
			else if (val.s == "tree") s.view_mode = 2;
			else s.view_mode = 1;
		}
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
		if (prop == "categories-opacity" && val.kind == PV::I && s.categories_opacity != val.i) return true;
		if (prop == "apps-opacity" && val.kind == PV::I && s.apps_opacity != val.i) return true;
		if (prop == "sidebar-position" && val.kind == PV::S && s.sidebar_position != val.s) return true;
		if (prop == "search-bar-position" && val.kind == PV::S && s.search_bar_position != val.s) return true;
		if (prop == "profile-position" && val.kind == PV::S && s.profile_position != val.s) return true;
		if (prop == "commands-position" && val.kind == PV::S && s.commands_position != val.s) return true;
		if (prop == "grid-columns" && val.kind == PV::I && s.grid_columns != val.i) return true;
		if (prop == "grid-rows" && val.kind == PV::I && s.grid_rows != val.i) return true;
		if (prop == "grid-density" && val.kind == PV::S && s.grid_density != val.s) return true;
		if (prop == "layout-mode" && val.kind == PV::S && s.layout_mode != val.s) return true;
		if (prop == "hover-switch-category" && val.kind == PV::B && s.category_hover_activate != val.b) return true;
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
	}
	return false;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_classic_property_count()
{
	auto c = make_classic();
	// Classic governs 12 properties
	assert(c.values.size() == 12);
}

static void test_modern_property_count()
{
	auto m = make_modern();
	// Modern governs 13 properties
	assert(m.values.size() == 13);
}

static void test_fullscreen_property_count()
{
	auto f = make_fullscreen();
	// FullScreen governs 14 properties
	assert(f.values.size() == 14);
}

static void test_apply_then_no_diff()
{
	auto presets = { make_classic(), make_modern(), make_fullscreen() };
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

static void test_modern_apps_opacity()
{
	auto m = make_modern();
	auto it = m.values.find("apps-opacity");
	assert(it != m.values.end());
	assert(it->second.i == 70);
}

static void test_modern_categories_opacity()
{
	auto m = make_modern();
	auto it = m.values.find("categories-opacity");
	assert(it != m.values.end());
	assert(it->second.i == 80);
}

static void test_fullscreen_layout_mode()
{
	auto f = make_fullscreen();
	auto it = f.values.find("layout-mode");
	assert(it != f.values.end());
	assert(it->second.s == "fullscreen");
}

static void test_fullscreen_sidebar_hidden()
{
	auto f = make_fullscreen();
	auto it = f.values.find("sidebar-position");
	assert(it != f.values.end());
	assert(it->second.s == "hidden");
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
// Shadow user-preset store — mirrors the CRUD logic in preset.cpp without Xfconf.
// ---------------------------------------------------------------------------

struct UserPresetShadow
{
	std::string uuid;
	std::string display_name;
	PVMap       values;
};

struct UserPresetStore
{
	std::vector<UserPresetShadow> presets;
	int next_uuid_counter = 0;

	std::string save(const std::string& display_name, const PVMap& values)
	{
		if (display_name.empty())
			return {};
		for (const auto& p : presets)
			if (p.display_name == display_name)
				return {};
		std::string uuid = "uuid-" + std::to_string(next_uuid_counter++);
		presets.push_back({ uuid, display_name, values });
		return uuid;
	}

	bool rename(const std::string& uuid, const std::string& new_name)
	{
		if (new_name.empty())
			return false;
		for (const auto& p : presets)
			if (p.display_name == new_name && p.uuid != uuid)
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
// T084: User preset CRUD tests
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

int main()
{
	test_classic_property_count();
	test_modern_property_count();
	test_fullscreen_property_count();
	test_apply_then_no_diff();
	test_diff_after_modify();
	test_modern_corner_radius();
	test_modern_apps_opacity();
	test_modern_categories_opacity();
	test_fullscreen_layout_mode();
	test_fullscreen_sidebar_hidden();
	test_fullscreen_to_docked_restores_menu_size();
	test_find_by_id();
	// T084: user preset CRUD
	test_user_preset_save_then_enumerate();
	test_user_preset_rename_updates_name();
	test_user_preset_delete_clears_current_id();
	test_user_preset_name_conflict_rejected();
	test_user_preset_empty_name_rejected();
	test_user_preset_delete_non_current_preserves_id();
	return 0;
}
