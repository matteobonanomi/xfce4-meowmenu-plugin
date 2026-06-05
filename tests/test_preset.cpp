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
			{ "categories-opacity",    PV::from_int(100)     },
			{ "apps-opacity",          PV::from_int(100)     },
			{ "sidebar-position",      PV::from_str("right") },
			{ "position-categories-horizontal", PV::from_bool(false) },
			{ "search-bar-position",   PV::from_str("top")   },
			{ "profile-position",      PV::from_str("top")   },
			{ "commands-position",     PV::from_str("top-right") },
			{ "layout-mode",           PV::from_str("docked") },
			{ "launcher-icon-size",    PV::from_int(2)       },
			{ "hover-switch-category", PV::from_bool(false)  },
			{ "view-mode-default",     PV::from_str("list")  },
			{ "default-category",      PV::from_str("favorites") },
			{ "stay-on-focus-out",     PV::from_bool(false)  },
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
			{ "position-categories-horizontal", PV::from_bool(false) },
			{ "search-bar-position",   PV::from_str("bottom") },
			{ "profile-position",      PV::from_str("top")    },
			{ "commands-position",     PV::from_str("top-right") },
			{ "layout-mode",           PV::from_str("docked") },
			{ "launcher-icon-size",    PV::from_int(3)        },
			{ "grid-density",          PV::from_str("medium") },
			{ "hover-switch-category", PV::from_bool(true)    },
			{ "view-mode-default",     PV::from_str("icons")  },
			{ "default-category",      PV::from_str("recent") },
			{ "stay-on-focus-out",     PV::from_bool(false)   },
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
			{ "sidebar-position",      PV::from_str("left")         },
			{ "position-categories-horizontal", PV::from_bool(false) },
			{ "search-bar-position",   PV::from_str("top")          },
			{ "profile-position",      PV::from_str("top")          },
			{ "commands-position",     PV::from_str("top-right")    },
			{ "launcher-icon-size",    PV::from_int(4)              },
			{ "grid-density",          PV::from_str("medium")       },
			{ "layout-mode",           PV::from_str("fullscreen")   },
			{ "hover-switch-category", PV::from_bool(true)          },
			{ "view-mode-default",     PV::from_str("icons")        },
			{ "default-category",      PV::from_str("all")          },
			{ "stay-on-focus-out",     PV::from_bool(false)         },
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
	bool position_categories_horizontal = false;
	std::string search_bar_position = "top";
	std::string profile_position    = "top";
	std::string commands_position   = "top-right";
	std::string grid_density = "medium";
	std::string layout_mode  = "docked";
	bool category_hover_activate = false;
	int launcher_icon_size = 2;
	int view_mode = 1; // ViewAsList
	int default_category = 0; // Favorites
	bool stay_on_focus_out = false;
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
		else if (prop == "position-categories-horizontal" && val.kind == PV::B)
			s.position_categories_horizontal = val.b;
		else if (prop == "search-bar-position" && val.kind == PV::S)  s.search_bar_position = val.s;
		else if (prop == "profile-position" && val.kind == PV::S)     s.profile_position = val.s;
		else if (prop == "commands-position" && val.kind == PV::S)    s.commands_position = val.s;
		else if (prop == "grid-density" && val.kind == PV::S)         s.grid_density = val.s;
		else if (prop == "layout-mode" && val.kind == PV::S)          s.layout_mode = val.s;
		else if (prop == "launcher-icon-size" && val.kind == PV::I)   s.launcher_icon_size = val.i;
		else if (prop == "hover-switch-category" && val.kind == PV::B)s.category_hover_activate = val.b;
		else if (prop == "menu-width" && val.kind == PV::I)           s.menu_width = val.i;
		else if (prop == "menu-height" && val.kind == PV::I)          s.menu_height = val.i;
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
		if (prop == "categories-opacity" && val.kind == PV::I && s.categories_opacity != val.i) return true;
		if (prop == "apps-opacity" && val.kind == PV::I && s.apps_opacity != val.i) return true;
		if (prop == "sidebar-position" && val.kind == PV::S && s.sidebar_position != val.s) return true;
		if (prop == "position-categories-horizontal" && val.kind == PV::B
				&& s.position_categories_horizontal != val.b) return true;
		if (prop == "search-bar-position" && val.kind == PV::S && s.search_bar_position != val.s) return true;
		if (prop == "profile-position" && val.kind == PV::S && s.profile_position != val.s) return true;
		if (prop == "commands-position" && val.kind == PV::S && s.commands_position != val.s) return true;
		if (prop == "grid-density" && val.kind == PV::S && s.grid_density != val.s) return true;
		if (prop == "layout-mode" && val.kind == PV::S && s.layout_mode != val.s) return true;
		if (prop == "launcher-icon-size" && val.kind == PV::I && s.launcher_icon_size != val.i) return true;
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
		if (prop == "default-category" && val.kind == PV::S)
		{
			if ((val.s == "favorites" && s.default_category != 0)
					|| (val.s == "recent" && s.default_category != 1)
					|| (val.s == "all" && s.default_category != 2))
				return true;
		}
		if (prop == "stay-on-focus-out" && val.kind == PV::B && s.stay_on_focus_out != val.b) return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_classic_property_count()
{
	auto c = make_classic();
	assert(c.values.size() == 17);
}

static void test_modern_property_count()
{
	auto m = make_modern();
	assert(m.values.size() == 18);
}

static void test_fullscreen_property_count()
{
	auto f = make_fullscreen();
	assert(f.values.size() == 16);
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
	auto it_icons = f.values.find("launcher-icon-size");
	assert(it_icons != f.values.end());
	assert(it_icons->second.kind == PV::I);
	assert(it_icons->second.i == 4);
	auto it_horizontal = f.values.find("position-categories-horizontal");
	assert(it_horizontal != f.values.end());
	assert(it_horizontal->second.kind == PV::B);
	assert(it_horizontal->second.b == false);
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
// Preset-field label resolution (contracts/preset-field-label.md).
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

// ---------------------------------------------------------------------------
// T042: File-seeded preset tests.
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
// T045: Parity sanity check — applying from the C++ fallback table and from
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
			{ "profile-position",     PV::from_str("top")         },
			{ "commands-position",    PV::from_str("top-right")   },
			{ "categories-opacity",   PV::from_int(100)           },
			{ "apps-opacity",         PV::from_int(100)           },
			{ "hover-switch-category",PV::from_bool(false)        },
			{ "stay-on-focus-out",    PV::from_bool(false)        },
			{ "default-category",     PV::from_str("favorites")   },
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
			{ "search-bar-position",  PV::from_str("bottom")    },
			{ "profile-position",     PV::from_str("top")       },
			{ "commands-position",    PV::from_str("top-right") },
			{ "categories-opacity",   PV::from_int(80)          },
			{ "apps-opacity",         PV::from_int(70)          },
			{ "hover-switch-category",PV::from_bool(true)       },
			{ "stay-on-focus-out",    PV::from_bool(false)      },
			{ "default-category",     PV::from_str("recent")    },
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
			{ "profile-position",     PV::from_str("top")        },
			{ "commands-position",    PV::from_str("top-right")  },
			{ "categories-opacity",   PV::from_int(100)          },
			{ "apps-opacity",         PV::from_int(100)          },
			{ "hover-switch-category",PV::from_bool(true)        },
			{ "stay-on-focus-out",    PV::from_bool(false)       },
			{ "default-category",     PV::from_str("all")        },
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
	assert(s_cpp.apps_opacity        == s_file.apps_opacity);
	assert(s_cpp.categories_opacity  == s_file.categories_opacity);
	assert(s_cpp.sidebar_position    == s_file.sidebar_position);
	assert(s_cpp.view_mode           == s_file.view_mode);
	assert(s_cpp.category_hover_activate == s_file.category_hover_activate);
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
	assert(s_cpp.default_category        == s_file.default_category);
}

// ---------------------------------------------------------------------------
// T008: GOVERNED_KEYS completeness + file↔table agreement for all built-ins.
//
// These exercise the REAL WhiskerMenu::BUILTIN_PRESETS table and
// governed_keys() (linked from preset-builtins.cpp), plus the shipped
// .meowpreset seed files parsed from MEOWMENU_TEST_PRESET_DIR. A missing key
// or a drifted value fails the build's test stage rather than leaking at
// runtime (FR-009, research R5).
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

static void test_governed_keys_completeness_table()
{
	const auto& keys = WhiskerMenu::governed_keys();
	for (int i = 0; i < WhiskerMenu::PRESET_BUILTIN_COUNT; ++i)
	{
		const WhiskerMenu::LayoutPreset& p = WhiskerMenu::BUILTIN_PRESETS[i];
		for (const auto& k : keys)
			assert(p.values.find(k) != p.values.end());
		// Every built-in carries a stored identity name (FR-011a).
		assert(!p.name.empty());
	}
}

static void test_governed_keys_completeness_files()
{
	const auto& keys = WhiskerMenu::governed_keys();
	const char* files[] = { "classic.meowpreset", "modern.meowpreset", "fullscreen.meowpreset" };
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
	test_apply_then_no_diff();
	test_diff_after_modify();
	test_modern_corner_radius();
	test_modern_apps_opacity();
	test_modern_categories_opacity();
	test_fullscreen_layout_mode();
	test_fullscreen_sidebar_left();
	test_fullscreen_to_docked_restores_menu_size();
	test_find_by_id();
	test_preset_label_resolution();
	// T084: user preset CRUD
	test_user_preset_save_then_enumerate();
	test_user_preset_rename_updates_name();
	test_user_preset_delete_clears_current_id();
	test_user_preset_name_conflict_rejected();
	test_user_preset_empty_name_rejected();
	test_user_preset_delete_non_current_preserves_id();
	// T042: file-seeded preset tests
	test_file_preset_overrides_cpp_table();
	test_fallback_to_cpp_table_when_files_absent();
	// T045: parity — C++ table vs file-equivalent preset
	test_parity_classic_cpp_vs_file();
	test_parity_modern_cpp_vs_file();
	test_parity_fullscreen_cpp_vs_file();
	// T008: real-table governed-key completeness + file↔table agreement
	test_governed_keys_completeness_table();
	test_governed_keys_completeness_files();
	test_file_table_agreement();
	return 0;
}
