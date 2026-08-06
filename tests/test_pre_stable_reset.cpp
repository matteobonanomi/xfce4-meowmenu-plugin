/*
 * Pure reset classification and destructive-target validation tests.
 */

#include "settings-defaults.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

#ifndef MEOWMENU_080_RESET_FIXTURE
#error "MEOWMENU_080_RESET_FIXTURE must be defined"
#endif
#ifndef MEOWMENU_RC_RESET_FIXTURE
#error "MEOWMENU_RC_RESET_FIXTURE must be defined"
#endif

using namespace WhiskerMenu;

int main()
{
	assert(decide_pre_stable_reset(0, "", false, 0)
		== PreStableResetDecision::Fresh);
	assert(decide_pre_stable_reset(0, "", true, 0)
		== PreStableResetDecision::Reset);
	assert(decide_pre_stable_reset(0, "", false, 4)
		== PreStableResetDecision::Reset);
	assert(decide_pre_stable_reset(COMPOSITION_RESET_GENERATION,
		"pending", false, 0) == PreStableResetDecision::Reset);
	assert(decide_pre_stable_reset(COMPOSITION_RESET_GENERATION,
		"complete", true, 9) == PreStableResetDecision::Load);

	assert(valid_meowmenu_property_base("/plugins/meowmenu-1"));
	assert(valid_meowmenu_property_base("/plugins/meowmenu-999"));
	assert(!valid_meowmenu_property_base(nullptr));
	assert(!valid_meowmenu_property_base(""));
	assert(!valid_meowmenu_property_base("/plugins/meowmenu-all"));
	assert(!valid_meowmenu_property_base("/plugins/other-1"));
	assert(!valid_meowmenu_property_base("/plugins/meowmenu-1/child"));

	for (const char* path : { MEOWMENU_080_RESET_FIXTURE,
			MEOWMENU_RC_RESET_FIXTURE })
	{
		std::ifstream file(path);
		assert(file.good());
		const std::string fixture((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		assert(fixture.find("name=\"initialized\"") != std::string::npos);
		assert(fixture.find("composition-reset-state") == std::string::npos);
	}
	return 0;
}
