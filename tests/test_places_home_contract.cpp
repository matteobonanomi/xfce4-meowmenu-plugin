/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "places/home-shortcuts.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace WhiskerMenu;

namespace
{

int failures = 0;

#define CHECK(condition) do { \
		if (!(condition)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", \
					__FILE__, __LINE__, #condition); \
			++failures; \
		} \
	} while (0)

}

int main()
{
	GError* error = nullptr;
	gchar* root = g_dir_make_tmp("meowmenu-home-contract-XXXXXX", &error);
	if (!root)
	{
		std::fprintf(stderr, "temporary directory failed: %s\n",
				error ? error->message : "unknown error");
		g_clear_error(&error);
		return EXIT_FAILURE;
	}

	gchar* home = g_build_filename(root, "home", nullptr);
	gchar* desktop = g_build_filename(root, "Desktop", nullptr);
	gchar* documents = g_build_filename(root, "Documents", nullptr);
	gchar* missing = g_build_filename(root, "Missing", nullptr);
	CHECK(g_mkdir(home, 0700) == 0);
	CHECK(g_mkdir(desktop, 0700) == 0);
	CHECK(g_mkdir(documents, 0700) == 0);

	const std::vector<const char*> candidates = {
		nullptr,
		"",
		missing,
		home,
		desktop,
		documents,
		desktop
	};
	const std::vector<std::string> expected = {
		home,
		desktop,
		documents
	};

	for (int rebuild = 0; rebuild < 20; ++rebuild)
	{
		const std::vector<std::string> actual =
				qualify_home_shortcuts(home, candidates);
		CHECK(actual == expected);
	}

	CHECK(qualify_home_shortcuts(nullptr, candidates).size() == 3);
	CHECK(qualify_home_shortcuts("", {}).empty());

	CHECK(g_rmdir(documents) == 0);
	CHECK(g_rmdir(desktop) == 0);
	CHECK(g_rmdir(home) == 0);
	CHECK(g_rmdir(root) == 0);
	g_free(missing);
	g_free(documents);
	g_free(desktop);
	g_free(home);
	g_free(root);

	if (failures != 0)
	{
		std::fprintf(stderr, "test_places_home_contract: %d failure(s)\n",
				failures);
		return EXIT_FAILURE;
	}
	std::printf("test_places_home_contract: ok\n");
	return EXIT_SUCCESS;
}
