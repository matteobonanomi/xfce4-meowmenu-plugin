/*
 * Unit tests for the Places open-path hardening logic.
 *
 * Covers the two pieces of the open path that are exercisable without a
 * display: the shell-quote round-trip used to hand paths to the external
 * helper, and the open-failure reason string built for the error dialog.
 *
 * NOTE: the item-lifetime fix and the single-dialog discipline are UI-flow
 * properties that cannot be driven by this headless harness; they are covered
 * by the manual verification steps in the feature's quickstart.
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
#include <gio/gio.h>

#include "places/places-item.h"

using WhiskerMenu::PlacesItem;

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
// FR-006: the reason string maps a GError + display name to a non-empty,
// named, legible message.
// ---------------------------------------------------------------------------

static void test_error_message_contains_name_and_reason()
{
	GError* error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			"No application is registered as handling this file");
	gchar* msg = PlacesItem::build_open_error_message(error, "report.odt");

	assert(msg && *msg);                                  // non-empty (FR-006c)
	assert(strstr(msg, "report.odt") != nullptr);         // names the item (FR-006a)
	assert(strstr(msg, "No application is registered") != nullptr); // reason (FR-006b)
	assert(g_utf8_validate(msg, -1, nullptr));
	assert(!has_control_bytes(msg));                      // legible (FR-006c)

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
// FR-007/FR-008: a path quoted with g_shell_quote round-trips through
// g_shell_parse_argv (the parse Element::spawn performs) to the exact literal
// path, so the external helper receives the right target and no embedded
// command can run.
// ---------------------------------------------------------------------------

static void assert_quote_round_trip(const char* path)
{
	gchar* quoted = g_shell_quote(path);
	// Mirror Element::spawn(): the helper command is "exo-open … <quoted>".
	gchar* command = g_strdup_printf("exo-open --launch FileManager %s", quoted);

	gchar** argv = nullptr;
	GError* error = nullptr;
	const gboolean ok = g_shell_parse_argv(command, nullptr, &argv, &error);
	assert(ok && argv);

	// argv = { "exo-open", "--launch", "FileManager", <path> } — the last token
	// must equal the original path byte-for-byte.
	guint n = g_strv_length(argv);
	assert(n == 4);
	assert(g_strcmp0(argv[n - 1], path) == 0);

	g_strfreev(argv);
	g_free(command);
	g_free(quoted);
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

int main()
{
	test_error_message_contains_name_and_reason();
	test_error_message_nonempty_without_gerror();
	test_error_message_strips_control_bytes();
	test_shell_quote_round_trip();
	return 0;
}
