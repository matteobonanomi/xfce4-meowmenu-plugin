/* Production preset-file parser checks. */

#include "presets/preset-io.h"

#include <glib.h>
#include <glib/gstdio.h>

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

void test_parser_accepts_supported_values()
{
	gchar* raw = g_dir_make_tmp("meow-preset-io-XXXXXX", nullptr);
	CHECK(raw != nullptr);
	if (!raw)
		return;
	const std::string dir(raw);
	const std::string path = dir + "/sample.meowpreset";
	const char contents[] =
		"[Preset]\nName=Sample\nSchemaVersion=99\n\n"
		"[Settings]\nmenu-opacity=60\nsidebar-position=left\n";
	CHECK(g_file_set_contents(path.c_str(), contents, -1, nullptr));
	const auto presets = enumerate_preset_files(dir, dir);
	CHECK(presets.size() == 1);
	if (!presets.empty())
	{
		CHECK(presets[0].id == "sample");
		CHECK(presets[0].display_name == "Sample");
		CHECK(presets[0].values.at("menu-opacity").i == 60);
	}
	g_remove(path.c_str());
	g_rmdir(dir.c_str());
	g_free(raw);
}

void test_parser_skips_invalid_and_deduplicates_user_files()
{
	gchar* system_raw = g_dir_make_tmp("meow-preset-system-XXXXXX", nullptr);
	gchar* user_raw = g_dir_make_tmp("meow-preset-user-XXXXXX", nullptr);
	CHECK(system_raw && user_raw);
	if (!system_raw || !user_raw)
	{
		g_free(system_raw);
		g_free(user_raw);
		return;
	}
	const std::string system(system_raw);
	const std::string user(user_raw);
	const char system_file[] =
		"[Preset]\nName=System\n\n[Settings]\nmenu-opacity=80\n";
	const char user_file[] =
		"[Preset]\nName=User\n\n[Settings]\nmenu-opacity=40\n";
	g_file_set_contents((system + "/same.meowpreset").c_str(),
			system_file, -1, nullptr);
	g_file_set_contents((user + "/same.meowpreset").c_str(),
			user_file, -1, nullptr);
	g_file_set_contents((system + "/broken.meowpreset").c_str(),
			"not a key file", -1, nullptr);
	const auto presets = enumerate_preset_files(system, user);
	CHECK(presets.size() == 1);
	if (!presets.empty())
		CHECK(presets[0].display_name == "User");
	for (const char* name : {"same.meowpreset", "broken.meowpreset"})
	{
		g_remove((system + "/" + name).c_str());
		g_remove((user + "/" + name).c_str());
	}
	g_rmdir(system.c_str());
	g_rmdir(user.c_str());
	g_free(system_raw);
	g_free(user_raw);
}

}

int main()
{
	test_parser_accepts_supported_values();
	test_parser_skips_invalid_and_deduplicates_user_files();
	return failures == 0 ? 0 : 1;
}
