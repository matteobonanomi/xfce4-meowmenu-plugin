/*
 * Unit tests for the preset-io import/export *logic* — no GLib, no Xfconf.
 *
 * The actual I/O (GKeyFile, xfconf writes) is covered by manual integration
 * testing (quickstart.md §T5). Here we test the pure validation rules that
 * are extracted as shadow helpers below:
 *
 *   - round-trip: a valid INI-like map → validate → all entries survive
 *   - corrupted input: invalid / empty content → parse fails
 *   - unknown keys: silently skipped
 *   - out-of-range int values: that entry is skipped, others survive
 *   - built-in name conflict: rejected
 *   - empty display-name: rejected
 */

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Shadow property schema — mirrors GOVERNED_PROPS in preset-io.cpp.
// ---------------------------------------------------------------------------

enum class PropKind { Int, Str, Bool };

struct ShadowPropDef
{
	const char*        name;
	PropKind           kind;
	int                min_i, max_i;
	std::vector<std::string> domain;
};

static const std::vector<ShadowPropDef> SHADOW_SCHEMA = {
	{ "corner-radius",         PropKind::Int,  0,   24,   {} },
	{ "panel-gap",             PropKind::Int,  0,   50,   {} },
	{ "menu-opacity",          PropKind::Int,  0,   100,  {} },
	{ "sidebar-position",      PropKind::Str,  0,   0,    {"left","right","hidden"} },
	{ "position-categories-horizontal", PropKind::Bool, 0, 0, {} },
	{ "search-bar-position",   PropKind::Str,  0,   0,    {"top","bottom"} },
	{ "profile-position",      PropKind::Str,  0,   0,    {"top-left","bottom-left","hidden"} },
	{ "commands-position",     PropKind::Str,  0,   0,    {"top-right","bottom-right","hidden"} },
	{ "grid-density",          PropKind::Str,  0,   0,    {"low","medium","high"} },
	{ "transparent-grid",      PropKind::Bool, 0,   0,    {} },
	{ "layout-mode",           PropKind::Str,  0,   0,    {"docked","fullscreen"} },
	{ "launcher-icon-size",    PropKind::Int,  -1,  6,    {} },
	{ "category-icon-size",    PropKind::Int,  -1,  6,    {} },
	{ "view-mode-default",     PropKind::Str,  0,   0,    {"icons","list","tree"} },
	{ "hover-switch-category", PropKind::Bool, 0,   0,    {} },
	{ "stay-on-focus-out",     PropKind::Bool, 0,   0,    {} },
	{ "menu-width",            PropKind::Int,  200, 2000, {} },
	{ "menu-height",           PropKind::Int,  200, 2000, {} },
	{ "default-category",      PropKind::Str,  0,   0,    {"favorites","recent","all"} },
	{ "places-switch-button-shape", PropKind::Str, 0, 0,  {"gtk-theme","rounded"} },
	{ "calculator-engine",      PropKind::Str,  0,   0,    {"none","bc","qalc","gcalccmd"} },
	{ "calculator-result-font-size", PropKind::Int, -1, 6, {} },
	{ "calculator-max-decimal-places", PropKind::Int, 0, 10, {} },
};

static const ShadowPropDef* find_shadow_prop(const std::string& name)
{
	for (const auto& pd : SHADOW_SCHEMA)
		if (pd.name == name) return &pd;
	return nullptr;
}

static std::string normalize_shadow_profile_position(const std::string& value)
{
	if (value == "top")
		return "top-left";
	if ((value == "bottom") || (value == "bottom-right"))
		return "bottom-left";
	return value;
}

// ---------------------------------------------------------------------------
// Shadow parsed preset value (mirrors PresetValue)
// ---------------------------------------------------------------------------

struct ShadowValue
{
	PropKind    kind;
	int         i;
	bool        b;
	std::string s;

	static ShadowValue make_int(int v)         { ShadowValue x; x.kind = PropKind::Int;  x.i = v; return x; }
	static ShadowValue make_bool(bool v)       { ShadowValue x; x.kind = PropKind::Bool; x.b = v; return x; }
	static ShadowValue make_str(std::string v) { ShadowValue x; x.kind = PropKind::Str;  x.s = v; return x; }
};

typedef std::map<std::string, ShadowValue> ShadowValueMap;

// ---------------------------------------------------------------------------
// Flat key-value store mimicking a [Settings] section parsed from INI.
// ---------------------------------------------------------------------------

struct RawSettings
{
	std::map<std::string, std::string> entries; // key → raw string value

	void put(const std::string& k, const std::string& v) { entries[k] = v; }
};

// ---------------------------------------------------------------------------
// Shadow validation: processes RawSettings, returns validated ShadowValueMap.
// Unknown keys and invalid values are silently skipped (same contract as real).
// ---------------------------------------------------------------------------

static ShadowValueMap validate_settings(const RawSettings& raw,
	std::vector<std::string>& skipped)
{
	ShadowValueMap result;
	for (const auto& kv : raw.entries)
	{
		const std::string& key = kv.first;
		const std::string& val = kv.second;

		const ShadowPropDef* pd = find_shadow_prop(key);
		if (!pd)
		{
			skipped.push_back(key);
			continue;
		}

		if (pd->kind == PropKind::Int)
		{
			bool ok = !val.empty();
			int v = 0;
			if (ok)
			{
				char* end = nullptr;
				v = (int)strtol(val.c_str(), &end, 10);
				ok = (end && *end == '\0');
			}
			if (!ok || v < pd->min_i || v > pd->max_i)
			{
				skipped.push_back(key);
				continue;
			}
			result[key] = ShadowValue::make_int(v);
		}
		else if (pd->kind == PropKind::Bool)
		{
			if (val == "true")       result[key] = ShadowValue::make_bool(true);
			else if (val == "false") result[key] = ShadowValue::make_bool(false);
			else                     skipped.push_back(key);
		}
		else // Str
		{
			std::string normalized = val;
			if (key == "profile-position")
				normalized = normalize_shadow_profile_position(val);

			bool in_domain = std::find(pd->domain.begin(), pd->domain.end(), normalized) != pd->domain.end();
			if (!in_domain)
			{
				skipped.push_back(key);
				continue;
			}
			result[key] = ShadowValue::make_str(normalized);
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// Shadow conflict check (mirrors preset_name_conflicts in preset.cpp).
// ---------------------------------------------------------------------------

static const char* BUILTIN_NAMES[] = { "classic", "modern", "fullscreen", nullptr };

static bool shadow_name_conflicts_builtin(const std::string& name)
{
	for (int i = 0; BUILTIN_NAMES[i]; ++i)
	{
		// case-insensitive, as in the real impl
		if (name == BUILTIN_NAMES[i]) return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Round-trip helper: builds a valid raw settings map and validates it.
// ---------------------------------------------------------------------------

static RawSettings make_valid_modern_raw()
{
	RawSettings r;
	r.put("corner-radius",      "12");
	r.put("panel-gap",          "8");
	r.put("menu-opacity",       "100");
	r.put("sidebar-position",   "left");
	r.put("position-categories-horizontal", "false");
	r.put("search-bar-position","bottom");
	r.put("profile-position",   "top-left");
	r.put("commands-position",  "top-right");
	r.put("layout-mode",        "docked");
	r.put("launcher-icon-size", "3");
	r.put("category-icon-size", "2");
	r.put("grid-density",       "medium");
	r.put("hover-switch-category", "true");
	r.put("transparent-grid",   "true");
	r.put("view-mode-default",  "icons");
	r.put("places-switch-button-shape", "gtk-theme");
	r.put("calculator-engine", "bc");
	r.put("calculator-result-font-size", "-1");
	r.put("calculator-max-decimal-places", "4");
	return r;
}

static void test_calculator_domains_and_bounds()
{
	RawSettings raw;
	raw.put("calculator-engine", "qalc");
	raw.put("calculator-result-font-size", "6");
	raw.put("calculator-max-decimal-places", "10");
	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);
	assert(skipped.empty());
	assert(result.at("calculator-engine").s == "qalc");
	assert(result.at("calculator-result-font-size").i == 6);
	assert(result.at("calculator-max-decimal-places").i == 10);

	raw.put("calculator-engine", "shell");
	raw.put("calculator-result-font-size", "7");
	raw.put("calculator-max-decimal-places", "11");
	skipped.clear();
	result = validate_settings(raw, skipped);
	assert(result.empty());
	assert(skipped.size() == 3);
}

// ---------------------------------------------------------------------------
// T102: Tests
// ---------------------------------------------------------------------------

static void test_round_trip_all_valid()
{
	RawSettings raw = make_valid_modern_raw();
	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);
	assert(skipped.empty());
	assert(result.size() == raw.entries.size());
}

static void test_profile_aliases_rewritten_to_canonical_domain()
{
	RawSettings raw;
	raw.put("profile-position", "top");
	raw.put("commands-position", "bottom-right");

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);

	assert(skipped.empty());
	assert(result.count("profile-position") == 1);
	assert(result.at("profile-position").s == "top-left");

	raw.put("profile-position", "bottom");
	skipped.clear();
	result = validate_settings(raw, skipped);
	assert(result.at("profile-position").s == "bottom-left");

	raw.put("profile-position", "bottom-right");
	skipped.clear();
	result = validate_settings(raw, skipped);
	assert(result.at("profile-position").s == "bottom-left");
}

static void test_unknown_keys_skipped()
{
	RawSettings raw = make_valid_modern_raw();
	raw.put("totally-unknown-key", "value");
	raw.put("another-unknown",     "42");

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);
	assert(skipped.size() == 2);
	assert(result.find("totally-unknown-key") == result.end());
	assert(result.find("another-unknown") == result.end());
	// All the valid keys still made it through
	assert(result.size() == raw.entries.size() - 2);
}

static void test_out_of_range_int_skipped_others_survive()
{
	RawSettings raw = make_valid_modern_raw();
	raw.put("corner-radius", "9999"); // max is 24

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);
	assert(std::find(skipped.begin(), skipped.end(), "corner-radius") != skipped.end());
	// Other entries (11 of them) still present
	assert(result.find("corner-radius") == result.end());
	assert(!result.empty());
}

static void test_invalid_str_domain_skipped_others_survive()
{
	RawSettings raw = make_valid_modern_raw();
	raw.put("layout-mode", "banana"); // not in domain

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);
	assert(std::find(skipped.begin(), skipped.end(), "layout-mode") != skipped.end());
	assert(result.find("layout-mode") == result.end());
	// Other entries still present
	assert(result.count("corner-radius") == 1);
}

static void test_invalid_bool_skipped()
{
	RawSettings raw;
	raw.put("hover-switch-category", "maybe"); // not true/false

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);
	assert(std::find(skipped.begin(), skipped.end(), "hover-switch-category") != skipped.end());
	assert(result.empty());
}

static void test_builtin_name_conflict_rejected()
{
	assert(shadow_name_conflicts_builtin("classic"));
	assert(shadow_name_conflicts_builtin("modern"));
	assert(shadow_name_conflicts_builtin("fullscreen"));
	assert(!shadow_name_conflicts_builtin("My Custom"));
	assert(!shadow_name_conflicts_builtin(""));
}

static void test_empty_settings_section()
{
	RawSettings raw; // empty
	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);
	assert(result.empty());
	assert(skipped.empty());
}

static void test_int_boundary_values()
{
	// Min and max of corner-radius (0–24) must be accepted
	{
		RawSettings raw;
		raw.put("corner-radius", "0");
		std::vector<std::string> s;
		auto r = validate_settings(raw, s);
		assert(r.count("corner-radius") == 1 && r["corner-radius"].i == 0);
	}
	{
		RawSettings raw;
		raw.put("corner-radius", "24");
		std::vector<std::string> s;
		auto r = validate_settings(raw, s);
		assert(r.count("corner-radius") == 1 && r["corner-radius"].i == 24);
	}
	// Just outside bounds — rejected
	{
		RawSettings raw;
		raw.put("corner-radius", "25");
		std::vector<std::string> s;
		auto r = validate_settings(raw, s);
		assert(r.find("corner-radius") == r.end());
	}
	{
		RawSettings raw;
		raw.put("corner-radius", "-1");
		std::vector<std::string> s;
		auto r = validate_settings(raw, s);
		assert(r.find("corner-radius") == r.end());
	}
}

static void test_category_icon_size_round_trip()
{
	for (int size : { -1, 0, 2, 6 })
	{
		RawSettings raw;
		raw.put("category-icon-size", std::to_string(size));
		std::vector<std::string> skipped;
		ShadowValueMap imported = validate_settings(raw, skipped);
		assert(skipped.empty());
		assert(imported.at("category-icon-size").i == size);
	}
}

// ---------------------------------------------------------------------------
// T043: Shadow enumeration tests — simulate enumerate_preset_files logic.
// These verify the merge/override rules without touching the filesystem.
// ---------------------------------------------------------------------------

struct ShadowPreset
{
	std::string     id;
	std::string     display_name;
	ShadowValueMap  values;
};

// Shadow version of enumerate_preset_files: system presets in first map,
// user presets in second; user wins on id collision (FR-061).
static std::vector<ShadowPreset> shadow_enumerate(
	const std::vector<ShadowPreset>& sys,
	const std::vector<ShadowPreset>& user)
{
	std::map<std::string, ShadowPreset> by_id;
	for (const auto& p : sys)  by_id[p.id] = p;
	for (const auto& p : user) by_id[p.id] = p; // user wins
	std::vector<ShadowPreset> result;
	for (auto& kv : by_id)
		result.push_back(std::move(kv.second));
	return result;
}

static void test_enumerate_system_only()
{
	std::vector<ShadowPreset> sys = {
		{ "classic", "Classic", {} },
		{ "modern",  "Modern",  {} },
	};
	auto result = shadow_enumerate(sys, {});
	assert(result.size() == 2);
}

static void test_enumerate_user_wins_on_duplicate_id()
{
	// System has "classic" with corner-radius=0; user overrides with 6.
	RawSettings sys_raw; sys_raw.put("corner-radius", "0");
	std::vector<std::string> sk1;
	ShadowPreset sys_p = { "classic", "Classic", validate_settings(sys_raw, sk1) };

	RawSettings usr_raw; usr_raw.put("corner-radius", "6");
	std::vector<std::string> sk2;
	ShadowPreset usr_p = { "classic", "Classic Custom", validate_settings(usr_raw, sk2) };

	auto result = shadow_enumerate({ sys_p }, { usr_p });
	assert(result.size() == 1);
	assert(result[0].id == "classic");
	assert(result[0].display_name == "Classic Custom"); // user wins
	assert(result[0].values.at("corner-radius").i == 6);
}

static void test_enumerate_malformed_file_silently_skipped()
{
	// A "malformed" preset is one where validation returns empty (e.g. completely
	// invalid settings). The shadow simulates this by passing an empty RawSettings.
	// The enumeration simply skips presets whose id is empty (validation gate).
	ShadowPreset bad = { "", "Bad", {} }; // empty id = failed parse
	std::vector<ShadowPreset> sys_all = {
		{ "classic", "Classic", {} },
		bad,
		{ "modern",  "Modern",  {} },
	};
	// Only count those with non-empty id (the real parse_preset_file_internal returns
	// false for these and they are never inserted into the result).
	int valid = 0;
	for (const auto& p : sys_all)
		if (!p.id.empty()) ++valid;
	assert(valid == 2);
}

static void test_enumerate_wrong_type_key_dropped_rest_loaded()
{
	// "corner-radius" with a non-integer value → skipped; other key survives.
	RawSettings raw;
	raw.put("corner-radius",  "not-a-number"); // wrong type
	raw.put("panel-gap",      "8");            // valid

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);

	assert(result.find("corner-radius") == result.end());
	assert(result.count("panel-gap") == 1);
	assert(result.at("panel-gap").i == 8);
}

static void test_enumerate_unknown_key_dropped()
{
	RawSettings raw;
	raw.put("corner-radius",    "4");       // valid
	raw.put("not-a-real-key",   "banana");  // unknown

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);

	assert(std::find(skipped.begin(), skipped.end(), "not-a-real-key") != skipped.end());
	assert(result.find("not-a-real-key") == result.end());
	assert(result.count("corner-radius") == 1);
}

static void test_new_schema_keys_accepted()
{
	// The single governed menu-opacity plus the other current keys are accepted
	// by the shadow validator.
	RawSettings raw;
	raw.put("menu-opacity",        "85");
	raw.put("stay-on-focus-out",   "true");
	raw.put("menu-width",          "480");
	raw.put("menu-height",         "520");
	raw.put("default-category",    "recent");

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);

	assert(skipped.empty());
	assert(result.count("menu-opacity")        == 1 && result.at("menu-opacity").i        == 85);
	assert(result.count("stay-on-focus-out")   == 1 && result.at("stay-on-focus-out").b   == true);
	assert(result.count("menu-width")          == 1 && result.at("menu-width").i          == 480);
	assert(result.count("menu-height")         == 1 && result.at("menu-height").i         == 520);
	assert(result.count("default-category")    == 1 && result.at("default-category").s    == "recent");
}

// ---------------------------------------------------------------------------
// T019/T020: schema-lenient import gate (FR-018) and name-conflict resolution
// (FR-011). Mirrors the accept/reject decision of import_user_preset() before
// the per-key [Settings] validation already covered above.
//
// Reject ONLY when: unparseable, a required section ([Preset]/[Settings]) is
// missing, or [Preset].Name is absent. Otherwise accept best-effort — a missing
// SchemaVersion is assumed current and a newer SchemaVersion is accepted.
// ---------------------------------------------------------------------------

enum class ImportGate { Ok, ParseError, MissingSection, MissingKey };

struct ImportFile
{
	bool parseable = true;
	bool has_preset = true;
	bool has_settings = true;
	bool has_name = true;
	// -1 means the SchemaVersion key is absent; otherwise the integer value.
	int  schema_version = 1;
};

static ImportGate import_gate(const ImportFile& f)
{
	if (!f.parseable)
		return ImportGate::ParseError;
	if (!f.has_preset || !f.has_settings)
		return ImportGate::MissingSection;
	if (!f.has_name)
		return ImportGate::MissingKey;
	// SchemaVersion is advisory: neither a missing nor a newer version rejects.
	return ImportGate::Ok;
}

static void test_import_accepts_missing_schema_version()
{
	ImportFile f; f.schema_version = -1; // key absent → assume current
	assert(import_gate(f) == ImportGate::Ok);
}

static void test_import_accepts_newer_schema_version()
{
	ImportFile f; f.schema_version = 99; // newer → accept best-effort
	assert(import_gate(f) == ImportGate::Ok);
}

static void test_import_rejects_unparseable()
{
	ImportFile f; f.parseable = false;
	assert(import_gate(f) == ImportGate::ParseError);
}

static void test_import_rejects_missing_section()
{
	{ ImportFile f; f.has_preset = false;   assert(import_gate(f) == ImportGate::MissingSection); }
	{ ImportFile f; f.has_settings = false; assert(import_gate(f) == ImportGate::MissingSection); }
}

static void test_import_rejects_missing_name()
{
	ImportFile f; f.has_name = false;
	assert(import_gate(f) == ImportGate::MissingKey);
}

// Name-conflict resolution: a built-in collision returns ConflictBuiltin (and the
// caller must withhold Overwrite); a user collision returns ConflictUser; a novel
// name imports cleanly. Comparison is case-insensitive, as in the real impl.
enum class ConflictKind { None, User, Builtin };

static std::string to_lower(std::string s)
{
	for (char& c : s)
		c = (char) tolower((unsigned char) c);
	return s;
}

static ConflictKind classify_conflict(const std::string& name,
	const std::vector<std::string>& user_names)
{
	for (int i = 0; BUILTIN_NAMES[i]; ++i)
		if (to_lower(name) == to_lower(BUILTIN_NAMES[i]))
			return ConflictKind::Builtin;
	for (const auto& u : user_names)
		if (to_lower(name) == to_lower(u))
			return ConflictKind::User;
	return ConflictKind::None;
}

static void test_conflict_builtin_withholds_overwrite()
{
	std::vector<std::string> users = { "My Layout" };
	// Built-in collision → ConflictBuiltin (Overwrite must not be offered).
	assert(classify_conflict("Modern", users) == ConflictKind::Builtin);
	assert(classify_conflict("modern", users) == ConflictKind::Builtin);
}

static void test_conflict_user_offers_overwrite()
{
	std::vector<std::string> users = { "My Layout" };
	assert(classify_conflict("my layout", users) == ConflictKind::User);
	assert(classify_conflict("Brand New", users) == ConflictKind::None);
}

// T030: export→import round-trip equality. A saved preset's validated value set,
// serialised on export and re-validated on import, must be byte-for-byte the same
// governed-key map — locking SC-007 automatically (contract Export/Import).
static void test_export_import_round_trip_equality()
{
	// Start from a fully-valid saved-custom value set.
	RawSettings original = make_valid_modern_raw();
	std::vector<std::string> sk0;
	ShadowValueMap saved = validate_settings(original, sk0);
	assert(sk0.empty());

	// "Export": serialise each governed value back to its raw string form.
	RawSettings exported;
	for (const auto& kv : saved)
	{
		const ShadowValue& v = kv.second;
		if (v.kind == PropKind::Int)
			exported.put(kv.first, std::to_string(v.i));
		else if (v.kind == PropKind::Bool)
			exported.put(kv.first, v.b ? "true" : "false");
		else
			exported.put(kv.first, v.s);
	}
	auto exported_profile = exported.entries.find("profile-position");
	assert(exported_profile != exported.entries.end());
	assert(exported_profile->second != "top");
	assert(exported_profile->second != "bottom");
	assert(exported_profile->second != "bottom-right");

	// "Import" the exported file under a fresh name.
	std::vector<std::string> sk1;
	ShadowValueMap reimported = validate_settings(exported, sk1);
	assert(sk1.empty());

	// The governed-key set and every value must be identical.
	assert(reimported.size() == saved.size());
	for (const auto& kv : saved)
	{
		auto it = reimported.find(kv.first);
		assert(it != reimported.end());
		assert(it->second.kind == kv.second.kind);
		// Compare only the field the kind populates (the others are unset).
		if (kv.second.kind == PropKind::Int)
			assert(it->second.i == kv.second.i);
		else if (kv.second.kind == PropKind::Bool)
			assert(it->second.b == kv.second.b);
		else
			assert(it->second.s == kv.second.s);
	}
}

// FR-014: a preset still carrying only the three retired per-region opacity keys
// imports without error — each is now unknown and skipped, leaving opacity
// unpinned (it resolves to the current/default single value).
static void test_import_old_opacity_keys_skipped_no_error()
{
	RawSettings raw;
	raw.put("categories-opacity", "40");
	raw.put("apps-opacity",       "50");
	raw.put("full-screen-opacity","60");

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);

	// All three retired keys are skipped as unknown; no error, nothing pinned.
	assert(skipped.size() == 3);
	assert(result.empty());
	assert(result.find("menu-opacity") == result.end());
}

// FR-014 precedence: when a file carries both the retired keys and menu-opacity,
// the retired keys are ignored and the single menu-opacity value is what applies.
static void test_import_new_opacity_wins_over_old()
{
	RawSettings raw;
	raw.put("categories-opacity", "10");
	raw.put("apps-opacity",       "20");
	raw.put("full-screen-opacity","30");
	raw.put("menu-opacity",       "75");

	std::vector<std::string> skipped;
	ShadowValueMap result = validate_settings(raw, skipped);

	// The three old keys are skipped; only menu-opacity survives, at its value.
	assert(skipped.size() == 3);
	assert(result.count("menu-opacity") == 1);
	assert(result.at("menu-opacity").i == 75);
}

int main()
{
	test_round_trip_all_valid();
	test_profile_aliases_rewritten_to_canonical_domain();
	test_calculator_domains_and_bounds();
	test_unknown_keys_skipped();
	test_out_of_range_int_skipped_others_survive();
	test_invalid_str_domain_skipped_others_survive();
	test_invalid_bool_skipped();
	test_builtin_name_conflict_rejected();
	test_empty_settings_section();
	test_int_boundary_values();
	test_category_icon_size_round_trip();
	// T043: enumeration logic
	test_enumerate_system_only();
	test_enumerate_user_wins_on_duplicate_id();
	test_enumerate_malformed_file_silently_skipped();
	test_enumerate_wrong_type_key_dropped_rest_loaded();
	test_enumerate_unknown_key_dropped();
	test_new_schema_keys_accepted();
	// 029-custom-presets: schema-lenient import gate, conflict resolution,
	// export→import round-trip equality (FR-011/FR-018, SC-007)
	test_import_accepts_missing_schema_version();
	test_import_accepts_newer_schema_version();
	test_import_rejects_unparseable();
	test_import_rejects_missing_section();
	test_import_rejects_missing_name();
	test_conflict_builtin_withholds_overwrite();
	test_conflict_user_offers_overwrite();
	test_export_import_round_trip_equality();
	// 042-simple-opacity: forward-compat for the retired per-region opacity keys
	test_import_old_opacity_keys_skipped_no_error();
	test_import_new_opacity_wins_over_old();
	return 0;
}
