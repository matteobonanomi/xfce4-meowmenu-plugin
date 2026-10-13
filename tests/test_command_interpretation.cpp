/*
 * Production-linked command parsing and PATH-boundary characterization.
 */

#include "launcher/command.h"
#include "launcher/command-interpreter.h"
#include "launcher/element.h"
#include "launcher/launcher-safety.h"
#include "config/xfce-helpers.h"
#include "search/run-action.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

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
bool g_xfce_spawn_result = false;
bool g_glib_spawn_result = true;
std::vector<std::string> g_spawn_argv;
std::string g_spawn_directory;
GSpawnFlags g_spawn_flags = GSpawnFlags(0);
bool g_spawn_startup_notify = false;
std::string g_spawn_icon;
bool g_spawn_inherits_environment = false;
bool g_spawn_ignores_child_pid = false;
bool g_spawn_child_watch = false;

class SpawnProbe : public Element
{
public:
	void run_spawn(const char* command) const
	{
		spawn(nullptr, command, nullptr, false, nullptr);
	}

	void run_spawn(const CommandInterpretation& interpretation,
			const char* directory, bool startup_notify,
			const char* icon) const
	{
		spawn(nullptr, interpretation, directory, startup_notify, icon);
	}
};

void capture_argv(gchar** argv)
{
	g_spawn_argv.clear();
	if (!argv)
		return;
	for (int i = 0; argv[i]; ++i)
		g_spawn_argv.emplace_back(argv[i]);
}

void expect_both(const char* command_line, bool expected)
{
	assert(command_line_is_available(command_line) == expected);
	assert(run_action_command_is_available(command_line) == expected);
}

/* expect_interpretation:
 * @command_line: input parsed with GLib shell syntax.
 * @expected: exact argv boundary expected without expansion.
 *
 * Pins one hostile or ordinary input against the production-owned parser.
 */
void expect_interpretation(const char* command_line,
		const std::vector<std::string>& expected)
{
	CommandInterpretation interpretation =
			CommandInterpretation::parse(command_line);
	assert(interpretation.valid());
	assert(interpretation.argument_count() == static_cast<int>(expected.size()));
	for (std::size_t i = 0; i < expected.size(); ++i)
		assert(expected[i] == interpretation.argument(static_cast<int>(i)));
	assert(interpretation.argument(static_cast<int>(expected.size())) == nullptr);
}

/* expect_launch_boundaries:
 *
 * Verifies the separate session and Xfce launch policies consume the same
 * retained argv without changing their flags, directory, notification, or icon.
 */
void expect_launch_boundaries()
{
	CommandInterpretation interpretation = CommandInterpretation::parse(
			"meow-command-probe --flag 'argument with spaces'");
	assert(interpretation.available());

	g_glib_spawn_result = true;
	GError* error = nullptr;
	assert(spawn_session_command_async(interpretation, &error));
	assert(error == nullptr);
	assert((g_spawn_argv == std::vector<std::string>{
			"meow-command-probe", "--flag", "argument with spaces"}));
	assert(g_spawn_directory.empty());
	assert(g_spawn_flags == G_SPAWN_SEARCH_PATH);
	assert(g_spawn_inherits_environment);
	assert(g_spawn_ignores_child_pid);

	g_xfce_spawn_result = true;
	SpawnProbe probe;
	probe.run_spawn(interpretation, "/tmp/meow working directory", true,
			"meow-icon");
	assert((g_spawn_argv == std::vector<std::string>{
			"meow-command-probe", "--flag", "argument with spaces"}));
	assert(g_spawn_directory == "/tmp/meow working directory");
	assert(g_spawn_flags == G_SPAWN_SEARCH_PATH);
	assert(g_spawn_startup_notify);
	assert(g_spawn_icon == "meow-icon");
	assert(g_spawn_inherits_environment);
	assert(g_spawn_child_watch);
	g_xfce_spawn_result = false;
}

/* expect_session_spawn_failure:
 *
 * Pins the session boundary's structured spawn error while retaining its
 * inherited directory/environment and fire-and-forget policy.
 */
void expect_session_spawn_failure()
{
	CommandInterpretation interpretation =
			CommandInterpretation::parse("meow-command-probe");
	g_glib_spawn_result = false;
	GError* error = nullptr;
	assert(!spawn_session_command_async(interpretation, &error));
	assert(error != nullptr);
	assert(error->domain == G_SPAWN_ERROR);
	assert(g_spawn_directory.empty());
	assert(g_spawn_inherits_environment);
	assert(g_spawn_ignores_child_pid);
	g_clear_error(&error);
	g_glib_spawn_result = true;
}

/* expect_stale_availability_failure:
 * @interpretation: interpretation retained while its executable still existed.
 *
 * Simulates disappearance between lookup and activation, proving launch keeps
 * the stored argv and exposes the ordinary spawn failure instead of reparsing.
 */
void expect_stale_availability_failure(
		const CommandInterpretation& interpretation)
{
	g_xfce_spawn_result = false;
	g_spawn_failure_mode = SpawnFailureMode::WithError;
	g_dialog_count = 0;
	SpawnProbe probe;
	probe.run_spawn(interpretation, nullptr, false, nullptr);
	assert(g_dialog_count == 1);
	assert((g_spawn_argv == std::vector<std::string>{"meow-command-probe"}));
}

/* expect_session_parse_failure:
 *
 * Ensures an invalid retained interpretation returns its structured parse
 * error without reaching the process-spawn boundary.
 */
void expect_session_parse_failure()
{
	CommandInterpretation invalid = CommandInterpretation::parse(
			"meow-command-probe 'unterminated");
	g_spawn_argv = {"sentinel"};
	GError* error = nullptr;
	assert(!spawn_session_command_async(invalid, &error));
	assert(error != nullptr);
	assert(error->domain == G_SHELL_ERROR);
	assert((g_spawn_argv == std::vector<std::string>{"sentinel"}));
	g_clear_error(&error);
}

/* expect_distinct_command_kinds:
 *
 * Keeps explicit-argv launcher editing and quoted Help URLs distinct while
 * proving both retain one-argument payload boundaries.
 */
void expect_distinct_command_kinds()
{
	const char* uri = "file:///tmp/app '$(touch nope)'.desktop";
	gchar** editor_argv = launcher_editor_argv("xfce-desktop-item-edit", uri);
	assert(editor_argv);
	assert(g_strcmp0(editor_argv[0], "xfce-desktop-item-edit") == 0);
	assert(g_strcmp0(editor_argv[1], uri) == 0);
	assert(editor_argv[2] == nullptr);
	g_strfreev(editor_argv);

	const char* help_uri = "https://example.invalid/a path?q='$value'";
	for (XfceDependencyRegime regime :
			{XfceDependencyRegime::Legacy, XfceDependencyRegime::Successor})
	{
		CommandInterpretation help = CommandInterpretation::parse(
				build_help_command(regime, help_uri).c_str());
		assert(help.valid());
		assert(help.argument_count() == 4);
		assert(g_strcmp0(help.argument(0), xfce_opener(regime)) == 0);
		assert(g_strcmp0(help.argument(1), "--launch") == 0);
		assert(g_strcmp0(help.argument(2), "WebBrowser") == 0);
		assert(g_strcmp0(help.argument(3), help_uri) == 0);
	}
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
		const gchar* working_directory,
		gchar** argv,
		gchar** environment,
		GSpawnFlags flags,
		gboolean startup_notify,
		guint32,
		const gchar* icon_name,
		gboolean child_watch,
		GError** error)
{
	capture_argv(argv);
	g_spawn_directory = working_directory ? working_directory : "";
	g_spawn_flags = flags;
	g_spawn_startup_notify = startup_notify;
	g_spawn_icon = icon_name ? icon_name : "";
	g_spawn_inherits_environment = environment == nullptr;
	g_spawn_child_watch = child_watch;
	if (g_xfce_spawn_result)
		return true;
	if (g_spawn_failure_mode == SpawnFailureMode::WithError)
	{
		g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
				"controlled spawn failure");
	}
	return false;
}

extern "C" gboolean g_spawn_async(const gchar* working_directory,
		gchar** argv,
		gchar** environment,
		GSpawnFlags flags,
		GSpawnChildSetupFunc,
		gpointer,
		GPid* child_pid,
		GError** error)
{
	capture_argv(argv);
	g_spawn_directory = working_directory ? working_directory : "";
	g_spawn_flags = flags;
	g_spawn_inherits_environment = environment == nullptr;
	g_spawn_ignores_child_pid = child_pid == nullptr;
	if (!g_glib_spawn_result && error)
	{
		g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
				"controlled GLib spawn failure");
	}
	return g_glib_spawn_result;
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
	expect_interpretation("meow-command-probe", {"meow-command-probe"});
	expect_interpretation("meow-command-probe --flag value",
			{"meow-command-probe", "--flag", "value"});
	expect_interpretation("meow-command-probe \"argument with spaces\"",
			{"meow-command-probe", "argument with spaces"});
	expect_interpretation("meow-command-probe one | two",
			{"meow-command-probe", "one", "|", "two"});
	expect_interpretation("meow-command-probe $HOME",
			{"meow-command-probe", "$HOME"});
	expect_interpretation("meow-command-probe `touch nope`",
			{"meow-command-probe", "`touch", "nope`"});
	expect_interpretation("meow-command-probe $(touch nope)",
			{"meow-command-probe", "$(touch", "nope)"});
	gchar* quoted_executable = g_shell_quote(executable);
	expect_interpretation(quoted_executable, {executable});
	g_free(quoted_executable);
	expect_both("meow-command-probe 'unterminated", false);
	expect_both("meow-command-probe trailing\\", false);
	expect_both("definitely-not-a-meow-command", false);
	expect_both(nullptr, false);
	assert(CommandInterpretation::parse(nullptr).failure()
			== CommandInterpretationFailure::Empty);
	assert(CommandInterpretation::parse("meow-command-probe 'unterminated").failure()
			== CommandInterpretationFailure::Parse);
	expect_launch_boundaries();
	expect_session_spawn_failure();
	expect_session_parse_failure();
	expect_distinct_command_kinds();
	expect_spawn_failure_dialog(SpawnFailureMode::WithError);
	expect_spawn_failure_dialog(SpawnFailureMode::WithoutError);

	CommandInterpretation retained =
			CommandInterpretation::parse("meow-command-probe");
	assert(retained.available());
	assert(g_remove(executable) == 0);
	expect_both("meow-command-probe", false);
	assert(!retained.available());
	expect_stale_availability_failure(retained);
	assert(g_setenv("PATH", saved_path.c_str(), TRUE));
	assert(g_rmdir(scratch) == 0);
	g_free(executable);
	g_free(scratch);
	std::printf("test_command_interpretation: ok\n");
	return 0;
}
