/*
 * Supported Xfconf key inventory guard.
 */

#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <vector>

#ifndef MEOWMENU_TEST_XFCONF_SNAPSHOT_PATH
#error "MEOWMENU_TEST_XFCONF_SNAPSHOT_PATH must be defined"
#endif

static std::vector<std::string> read_snapshot()
{
	std::ifstream file(MEOWMENU_TEST_XFCONF_SNAPSHOT_PATH);
	assert(file.good());
	std::vector<std::string> keys;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
			keys.push_back(line.substr(1, line.size() - 2));
	}
	return keys;
}

static bool contains(const std::vector<std::string>& keys, const char* key)
{
	return std::find(keys.begin(), keys.end(), key) != keys.end();
}

int main()
{
	const std::vector<std::string> keys = read_snapshot();
	for (const char* required : {
		"/plugins/plugin-7",
		"/layout-mode",
		"/search-bar-position",
		"/show-profile",
		"/show-session",
		"/sidebar-enabled",
		"/sidebar-position",
		"/transparent-grid",
		"/places/switch-button-shape",
		"/extras/calculator-engine",
		"/extras/calculator-result-font-size",
		"/extras/calculator-max-decimal-places",
	})
		assert(contains(keys, required));

	for (const char* retired : {
		"/position-categories-horizontal",
		"/profile-position",
		"/commands-position",
		"/unified-bar",
	})
		assert(!contains(keys, retired));
	return 0;
}
