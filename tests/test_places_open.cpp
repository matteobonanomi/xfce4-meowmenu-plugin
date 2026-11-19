/*
 * Unit tests for the Places open-path hardening logic.
 *
 * Covers the open policy, exact executable path and working directory, helper
 * quoting, and the open-failure reason string without launching a process.
 *
 * NOTE: the script-choice dialog and single-dialog discipline remain UI-flow
 * properties that cannot be driven by this headless harness.
 *
 * Like the other tests in this folder, the production translation unit
 * (places-item.cpp) is compiled directly into the binary rather than linked
 * against the plugin library; element.cpp supplies the Element base symbols it
 * pulls in.
 */

#include <cassert>
#include <cstring>
#include <string>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "places/places-item.h"
#include "config/xfce-helpers.h"

using WhiskerMenu::PlacesItem;
using WhiskerMenu::PlacesOpenAction;
using WhiskerMenu::XfceDependencyRegime;
using WhiskerMenu::build_file_manager_command;
using WhiskerMenu::build_terminal_command;
using WhiskerMenu::build_terminal_execution_command;

// Reject the C0 control range (and DEL) — an open-failure message must be
// legible, so any control byte counts as garbage for this check. Tab/newline
// are not expected in a one-line dialog message either.
static bool has_control_bytes(const char* s)
{
	for (const guchar* p = reinterpret_cast<const guchar*>(s); *p; ++p)
	{
		if (*p < 0x20 || *p == 0x7f)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// the documented behavior: the reason string maps a GError + display name to a non-empty,
// named, legible message.
// ---------------------------------------------------------------------------

static void test_error_message_contains_name_and_reason()
{
	GError* error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			"No application is registered as handling this file");
	gchar* msg = PlacesItem::build_open_error_message(error, "report.odt");

	assert(msg && *msg);                                  // non-empty (the documented behavior)
	assert(strstr(msg, "report.odt") != nullptr);         // names the item (the documented behavior)
	assert(strstr(msg, "No application is registered") != nullptr); // reason (the documented behavior)
	assert(g_utf8_validate(msg, -1, nullptr));
	assert(!has_control_bytes(msg));                      // legible (the documented behavior)

	g_free(msg);
	g_error_free(error);
}

static void test_error_message_nonempty_without_gerror()
{
	// No GError object at all must still yield a non-empty, named reason.
	gchar* msg = PlacesItem::build_open_error_message(nullptr, "weird name.bin");
	assert(msg && *msg);
	assert(strstr(msg, "weird name.bin") != nullptr);
	assert(!has_control_bytes(msg));
	g_free(msg);
}

static void test_error_message_strips_control_bytes()
{
	// A reason carrying stray control bytes must be sanitised, never surfaced
	// raw into the dialog.
	GError* error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_FAILED,
			"bad\x01\x02reason\x7f");
	gchar* msg = PlacesItem::build_open_error_message(error, "f.txt");
	assert(msg && *msg);
	assert(!has_control_bytes(msg));
	assert(g_utf8_validate(msg, -1, nullptr));
	g_free(msg);
	g_error_free(error);
}

// ---------------------------------------------------------------------------
// the documented behavior: a path quoted with g_shell_quote round-trips through
// g_shell_parse_argv (the parse Element::spawn performs) to the exact literal
// path, so the external helper receives the right target and no embedded
// command can run.
// ---------------------------------------------------------------------------

static void assert_quote_round_trip(const char* path)
{
	for (XfceDependencyRegime regime :
			{XfceDependencyRegime::Legacy, XfceDependencyRegime::Successor})
	{
		for (const std::string& command : {
				build_file_manager_command(regime, path),
				build_terminal_command(regime, path)})
		{
			gchar** argv = nullptr;
			GError* error = nullptr;
			const gboolean ok = g_shell_parse_argv(
					command.c_str(), nullptr, &argv, &error);
			assert(ok && argv);
			guint n = g_strv_length(argv);
			assert(n >= 4);
			assert(g_strcmp0(argv[n - 1], path) == 0);
			g_strfreev(argv);
		}
	}
}

static void test_shell_quote_round_trip()
{
	assert_quote_round_trip("/home/u/plain.txt");
	assert_quote_round_trip("/home/u/with space.txt");
	assert_quote_round_trip("/home/u/a \"b\" $(c).txt");
	assert_quote_round_trip("/home/u/weird $(dir) \"name\"");
	assert_quote_round_trip("/home/u/`backtick`.txt");
	assert_quote_round_trip("/home/u/dollar$VAR and (parens).txt");
	assert_quote_round_trip("/home/u/trailing backslash\\");
	assert_quote_round_trip("/home/u/single'quote.txt");
}

// ---------------------------------------------------------------------------

static void test_executable_classification()
{
	using Action = PlacesOpenAction;
	using Type = GFileType;

	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, true, "application/x-executable") == Action::Execute);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, true, "application/x-shellscript") == Action::AskScript);

	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_DIRECTORY,
			true, true, "application/x-executable") == Action::DefaultHandler);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			false, true, "application/x-executable") == Action::DefaultHandler);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, false, "application/x-executable") == Action::DefaultHandler);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, true, nullptr) == Action::DefaultHandler);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, true, "application/x-desktop") == Action::DefaultHandler);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, true, "text/plain") == Action::DefaultHandler);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, true, "application/json") == Action::DefaultHandler);
	assert(PlacesItem::classify_open_action(Type::G_FILE_TYPE_REGULAR,
			true, true, "application/xml") == Action::DefaultHandler);
}

static void test_execution_request_preserves_literal_path()
{
	gchar* directory = g_dir_make_tmp("meowmenu-execute-XXXXXX", nullptr);
	assert(directory);
	gchar* native_path = g_build_filename(directory,
			"Meow '$(touch nope)' `literal` executable", nullptr);
	assert(g_file_set_contents(native_path, "fixture", -1, nullptr));
	GFile* file = g_file_new_for_path(native_path);

	std::string path = "unchanged";
	std::string working_directory = "unchanged";
	assert(PlacesItem::build_execution_request(
			file, &path, &working_directory));
	assert(path == native_path);
	assert(working_directory == directory);

	GFile* remote = g_file_new_for_uri("sftp://example.invalid/program");
	path = "unchanged";
	working_directory = "unchanged";
	assert(!PlacesItem::build_execution_request(
			remote, &path, &working_directory));
	assert(path == "unchanged");
	assert(working_directory == "unchanged");

	g_object_unref(remote);
	g_object_unref(file);
	assert(g_remove(native_path) == 0);
	assert(g_rmdir(directory) == 0);
	g_free(native_path);
	g_free(directory);
}

static void test_terminal_execution_command_round_trip()
{
	const char* path = "/tmp/Meow '$(touch nope)' `literal` executable";
	for (XfceDependencyRegime regime :
			{XfceDependencyRegime::Legacy, XfceDependencyRegime::Successor})
	{
		const std::string command =
				build_terminal_execution_command(regime, path);
		gchar** helper_argv = nullptr;
		assert(g_shell_parse_argv(command.c_str(), nullptr,
				&helper_argv, nullptr));
		assert(g_strv_length(helper_argv) == 4);

		gchar** executable_argv = nullptr;
		assert(g_shell_parse_argv(helper_argv[3], nullptr,
				&executable_argv, nullptr));
		assert(g_strv_length(executable_argv) == 1);
		assert(g_strcmp0(executable_argv[0], path) == 0);

		g_strfreev(executable_argv);
		g_strfreev(helper_argv);
	}
}

// ---------------------------------------------------------------------------

int main()
{
	test_error_message_contains_name_and_reason();
	test_error_message_nonempty_without_gerror();
	test_error_message_strips_control_bytes();
	test_shell_quote_round_trip();
	test_executable_classification();
	test_execution_request_preserves_literal_path();
	test_terminal_execution_command_round_trip();
	return 0;
}
