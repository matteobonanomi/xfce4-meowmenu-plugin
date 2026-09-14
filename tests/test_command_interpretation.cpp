/*
 * Production-linked command parsing and PATH-boundary characterization.
 */

#include "launcher/command.h"
#include "launcher/element.h"
#include "search/run-action.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>

#include <glib/gstdio.h>

using namespace WhiskerMenu;

namespace
{

enum class SpawnFailureMode
{
	WithoutError,
	WithError
};

SpawnFailureMode g_spawn_failure_mode = SpawnFailureMode::WithoutError;
unsigned int g_dialog_count = 0;
bool g_dialog_received_error = false;
std::string g_dialog_primary_text;

class SpawnProbe : public Element
{
public:
	void run_spawn(const char* command) const
	{
		spawn(nullptr, command, nullptr, false, nullptr);
	}
};

void expect_both(const char* command_line, bool expected)
{
	assert(command_line_is_available(command_line) == expected);
	assert(run_action_command_is_available(command_line) == expected);
}

/* expect_spawn_failure_dialog:
 * @mode: controls whether the production spawn boundary supplies a GError.
 *
 * Verifies that either failure shape produces exactly one themed dialog. The
 * production caller owns and releases a populated error after the dialog.
 */
void expect_spawn_failure_dialog(SpawnFailureMode mode)
{
	g_spawn_failure_mode = mode;
	g_dialog_count = 0;
	g_dialog_received_error = false;
	g_dialog_primary_text.clear();
	SpawnProbe probe;
	probe.run_spawn("meow-command-probe --flag");
	assert(g_dialog_count == 1);
	assert(g_dialog_received_error == (mode == SpawnFailureMode::WithError));
	assert(g_dialog_primary_text
			== "Failed to execute command \"meow-command-probe --flag\".");
}

}

extern "C" gboolean xfce_spawn(GdkScreen*,
		const gchar*,
		gchar**,
		gchar**,
		GSpawnFlags,
		gboolean,
		guint32,
		const gchar*,
		gboolean,
		GError** error)
{
	if (g_spawn_failure_mode == SpawnFailureMode::WithError)
	{
		g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
				"controlled spawn failure");
	}
	return false;
}

extern "C" void xfce_dialog_show_error(GtkWindow*,
		const GError* error,
		const gchar* primary_format,
		...)
{
	++g_dialog_count;
	g_dialog_received_error = error != nullptr;
	va_list arguments;
	va_start(arguments, primary_format);
	gchar* primary_text = g_strdup_vprintf(primary_format, arguments);
	va_end(arguments);
	g_dialog_primary_text = primary_text;
	g_free(primary_text);
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
	expect_spawn_failure_dialog(SpawnFailureMode::WithError);
	expect_spawn_failure_dialog(SpawnFailureMode::WithoutError);

	assert(g_remove(executable) == 0);
	expect_both("meow-command-probe", false);
	assert(g_setenv("PATH", saved_path.c_str(), TRUE));
	assert(g_rmdir(scratch) == 0);
	g_free(executable);
	g_free(scratch);
	std::printf("test_command_interpretation: ok\n");
	return 0;
}
