/*
 * Headless coverage for async application-load generation commit rules.
 */

#include "launcher/application-load-generation.h"

#include <cassert>

using namespace WhiskerMenu;

int main()
{
	assert(application_load_generation_can_commit(7, 7, false, true));
	assert(!application_load_generation_can_commit(8, 7, false, true));
	assert(!application_load_generation_can_commit(7, 7, true, true));
	assert(!application_load_generation_can_commit(7, 7, false, false));
	assert(!application_load_generation_can_commit(8, 7, true, false));

	return 0;
}
