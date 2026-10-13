/* Cross-check production-owned preset synchronization metadata. */

#include "presets/preset.h"
#include "ui/properties/preset-sync.h"

#include <cstdio>
#include <set>

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

void test_sync_descriptors_are_unique()
{
	std::set<std::string> keys;
	for (const PresetSyncDescriptor& descriptor : preset_sync_descriptors())
	{
		CHECK(descriptor.key != nullptr);
		CHECK(keys.insert(descriptor.key).second);
	}
	CHECK(keys.size() == preset_sync_descriptors().size());
}

void test_sync_descriptors_cover_governed_settings()
{
	std::set<std::string> keys;
	for (const PresetSyncDescriptor& descriptor : preset_sync_descriptors())
		keys.insert(descriptor.key);
	for (const std::string& key : governed_keys())
		CHECK(keys.find(key) != keys.end());
}

}

int main()
{
	test_sync_descriptors_are_unique();
	test_sync_descriptors_cover_governed_settings();
	return failures == 0 ? 0 : 1;
}
