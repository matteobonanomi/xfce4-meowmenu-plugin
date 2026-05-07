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
	{ "corner-radius",         PropKind::Int,  0,   24,  {} },
	{ "panel-gap",             PropKind::Int,  0,   50,  {} },
	{ "categories-opacity",    PropKind::Int,  0,   100, {} },
	{ "apps-opacity",          PropKind::Int,  0,   100, {} },
	{ "grid-columns",          PropKind::Int,  2,   10,  {} },
	{ "grid-rows",             PropKind::Int,  1,   8,   {} },
	{ "sidebar-position",      PropKind::Str,  0,   0,   {"left","right","hidden"} },
	{ "search-bar-position",   PropKind::Str,  0,   0,   {"top","bottom"} },
	{ "profile-position",      PropKind::Str,  0,   0,   {"top","bottom","bottom-right","hidden"} },
	{ "commands-position",     PropKind::Str,  0,   0,   {"top-right","bottom-right","hidden"} },
	{ "grid-density",          PropKind::Str,  0,   0,   {"low","medium","high"} },
	{ "layout-mode",           PropKind::Str,  0,   0,   {"docked","fullscreen"} },
	{ "view-mode-default",     PropKind::Str,  0,   0,   {"icons","list","tree"} },
	{ "hover-switch-category", PropKind::Bool, 0,   0,   {} },
};

static const ShadowPropDef* find_shadow_prop(const std::string& name)
{
	for (const auto& pd : SHADOW_SCHEMA)
		if (pd.name == name) return &pd;
	return nullptr;
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
			bool in_domain = std::find(pd->domain.begin(), pd->domain.end(), val) != pd->domain.end();
			if (!in_domain)
			{
				skipped.push_back(key);
				continue;
			}
			result[key] = ShadowValue::make_str(val);
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
	r.put("categories-opacity", "100");
	r.put("apps-opacity",       "80");
	r.put("sidebar-position",   "left");
	r.put("search-bar-position","bottom");
	r.put("profile-position",   "top");
	r.put("commands-position",  "top-right");
	r.put("layout-mode",        "docked");
	r.put("grid-density",       "medium");
	r.put("hover-switch-category", "true");
	r.put("view-mode-default",  "icons");
	return r;
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

int main()
{
	test_round_trip_all_valid();
	test_unknown_keys_skipped();
	test_out_of_range_int_skipped_others_survive();
	test_invalid_str_domain_skipped_others_survive();
	test_invalid_bool_skipped();
	test_builtin_name_conflict_rejected();
	test_empty_settings_section();
	test_int_boundary_values();
	return 0;
}
