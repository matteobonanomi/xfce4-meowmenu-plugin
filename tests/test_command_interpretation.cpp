/*
 * Production-linked command parsing and PATH-boundary characterization.
 */

#include "launcher/command.h"
#include "search/run-action.h"

#include <cassert>
#include <cstdio>
#include <string>

#include <glib/gstdio.h>

using namespace WhiskerMenu;

namespace
{

void expect_both(const char* command_line, bool expected)
{
	assert(command_line_is_available(command_line) == expected);
	assert(run_action_command_is_available(command_line) == expected);
}

}

int main()
{
	GError* error = nullptr;
	gchar* scratch = g_dir_make_tmp("meow-command-path-XXXXXX", &error);
	assert(scratch && !error);
	gchar* executable = g_build_filename(scratch, "meow-command-probe", nullptr);
	assert(g_file_set_contents(executable, "#!/bin/sh\nexit 0\n", -1, &error));
	assert(!error);
	assert(g_chmod(executable, 0700) == 0);
	const gchar* old_path = g_getenv("PATH");
	const std::string saved_path = old_path ? old_path : "";
	const std::string path = std::string(scratch) + G_SEARCHPATH_SEPARATOR_S
			+ saved_path;
	assert(g_setenv("PATH", path.c_str(), TRUE));

	expect_both("meow-command-probe", true);
	expect_both("meow-command-probe --flag 'argument with spaces'", true);
	expect_both(executable, true);
	expect_both("meow-command-probe 'unterminated", false);
	expect_both("definitely-not-a-meow-command", false);
	expect_both(nullptr, false);

	assert(g_remove(executable) == 0);
	expect_both("meow-command-probe", false);
	assert(g_setenv("PATH", saved_path.c_str(), TRUE));
	assert(g_rmdir(scratch) == 0);
	g_free(executable);
	g_free(scratch);
	std::printf("test_command_interpretation: ok\n");
	return 0;
}
