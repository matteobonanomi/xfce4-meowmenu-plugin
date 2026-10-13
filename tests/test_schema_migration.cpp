/* Production-owned schema policy checks for fresh and upgraded profiles. */

#include "settings-defaults.h"

#include <cstdio>

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

void test_schema_boundaries()
{
	CHECK(SETTINGS_SCHEMA_VERSION == 13);
	CHECK(!settings_schema_needs_upgrade(SETTINGS_SCHEMA_VERSION));
	CHECK(settings_schema_needs_upgrade(SETTINGS_SCHEMA_VERSION - 1));
	CHECK(settings_schema_needs_upgrade(0));
}

void test_fresh_profile_policy()
{
	CHECK(should_apply_fresh_preset(false, true));
	CHECK(!should_apply_fresh_preset(true, true));
	CHECK(!should_apply_fresh_preset(false, false));
	CHECK(settings_relative_property("/plugins/plugin-1/menu-width",
			"/plugins/plugin-1") != nullptr);
	CHECK(settings_relative_property("/plugins/plugin-1", "/plugins/plugin-1")
			== nullptr);
}

void test_retired_keys_are_inert()
{
	CHECK(is_retired_settings_key("/unified-bar"));
	CHECK(is_retired_settings_key("/places/switch-button-shape"));
	CHECK(!is_retired_settings_key("/menu-opacity"));
	CHECK(!is_retired_settings_key(nullptr));
}

}

int main()
{
	test_schema_boundaries();
	test_fresh_profile_policy();
	test_retired_keys_are_inert();
	return failures == 0 ? 0 : 1;
}
