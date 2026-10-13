/* Production-owned built-in preset contract checks. */

#include "presets/preset.h"

#include <cstdio>
#include <cstring>

using namespace WhiskerMenu;

namespace
{

int failures = 0;

#define CHECK(value) do { \
	if (!(value)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); \
		++failures; \
	} \
} while (0)

void test_builtin_identity()
{
	const char* ids[] = {"classic", "modern", "fullscreen", "minimal"};
	for (unsigned i = 0; i < PRESET_BUILTIN_COUNT; ++i)
	{
		CHECK(BUILTIN_PRESETS[i].is_builtin);
		CHECK(BUILTIN_PRESETS[i].id == ids[i]);
		CHECK(!BUILTIN_PRESETS[i].name.empty());
		CHECK(!BUILTIN_PRESETS[i].values.empty());
	}
}

void test_governed_schema_is_complete()
{
	const auto& keys = governed_keys();
	CHECK(!keys.empty());
	for (unsigned i = 0; i < PRESET_BUILTIN_COUNT; ++i)
		for (const std::string& key : keys)
			CHECK(BUILTIN_PRESETS[i].values.find(key)
					!= BUILTIN_PRESETS[i].values.end());
}

void test_builtin_values_and_provenance()
{
	CHECK(BUILTIN_PRESETS[PRESET_CLASSIC].values.at("menu-opacity").i == 100);
	CHECK(BUILTIN_PRESETS[PRESET_FULLSCREEN].values.at("menu-opacity").i == 80);
	CHECK(BUILTIN_PRESETS[PRESET_MINIMAL].values.at("menu-opacity").i == 60);
	CHECK(BUILTIN_PRESETS[PRESET_MODERN].values.at("menu-width").i == 590);
	CHECK(BUILTIN_PRESETS[PRESET_MODERN].values.at("menu-height").i == 500);
	CHECK(preset_name_for_display(BUILTIN_PRESETS[PRESET_MODERN])
			== BUILTIN_PRESETS[PRESET_MODERN].name);
	CHECK(preset_description_for_display(BUILTIN_PRESETS[PRESET_CLASSIC])
			== BUILTIN_PRESETS[PRESET_CLASSIC].description);
}

}

int main()
{
	test_builtin_identity();
	test_governed_schema_is_complete();
	test_builtin_values_and_provenance();
	return failures == 0 ? 0 : 1;
}
