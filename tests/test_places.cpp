/*
 * Unit tests for milestone 005 (Places mode) pure logic.
 *
 * Mirrors PlacesItem::search() (case-folded substring match) and
 * HomeSection::get_items()'s existence filter without instantiating GTK
 * widgets, matching the pattern of the other tests in this folder.
 *
 * NOTE: the production code paths still use GTK / GIO; this test exercises
 * the same algorithms in isolation. A future test harness that can host
 * GTK widget construction (FR-046 follow-up) will replace this stand-in.
 */

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

// ---------------------------------------------------------------------------
// Stand-in for PlacesItem::search(): case-fold the input filter, compare with
// the case-folded display name via strstr.
// ---------------------------------------------------------------------------
static bool places_search(const char* display_name, const char* filter)
{
	if (!filter || !*filter)
	{
		return true;
	}
	gchar* folded_name = g_utf8_casefold(display_name ? display_name : "", -1);
	gchar* folded_filter = g_utf8_casefold(filter, -1);
	const bool match = (folded_name && folded_filter
			&& strstr(folded_name, folded_filter) != nullptr);
	g_free(folded_name);
	g_free(folded_filter);
	return match;
}

// ---------------------------------------------------------------------------
// Stand-in for HomeSection::get_items()'s existence filter: keep only paths
// that exist on disk as directories.
// ---------------------------------------------------------------------------
static std::vector<std::string> filter_existing_dirs(
		const std::vector<std::string>& candidates)
{
	std::vector<std::string> out;
	for (const auto& p : candidates)
	{
		if (!p.empty() && g_file_test(p.c_str(), G_FILE_TEST_IS_DIR))
		{
			out.push_back(p);
		}
	}
	return out;
}

// ---------------------------------------------------------------------------

static void test_search_empty_filter_matches_everything()
{
	assert(places_search("Downloads", ""));
	assert(places_search("Downloads", nullptr));
	assert(places_search("", ""));
}

static void test_search_case_insensitive_ascii()
{
	assert(places_search("Downloads", "down"));
	assert(places_search("Downloads", "DOWN"));
	assert(places_search("Downloads", "loaDS"));
	assert(!places_search("Downloads", "music"));
}

static void test_search_utf8()
{
	// Italian "Università" — folded to lowercase, sigma rules apply.
	assert(places_search("Università", "università"));
	assert(places_search("Università", "UNIVERS"));
	// Cyrillic non-match
	assert(!places_search("Università", "школа"));
}

static void test_search_empty_name()
{
	assert(places_search("", "anything") == false);
	assert(places_search(nullptr, "anything") == false);
}

static void test_home_filter_existing_dirs()
{
	gchar* tmpdir = g_dir_make_tmp("meowmenu-test-XXXXXX", nullptr);
	assert(tmpdir);

	gchar* sub = g_build_filename(tmpdir, "exists", nullptr);
	assert(g_mkdir(sub, 0700) == 0);

	std::vector<std::string> candidates = {
		sub,
		std::string(tmpdir) + "/does-not-exist",
		"",
		"/nonexistent/path/under/root",
	};

	auto kept = filter_existing_dirs(candidates);
	assert(kept.size() == 1);
	assert(kept[0] == sub);

	g_rmdir(sub);
	g_rmdir(tmpdir);
	g_free(sub);
	g_free(tmpdir);
}

int main()
{
	test_search_empty_filter_matches_everything();
	test_search_case_insensitive_ascii();
	test_search_utf8();
	test_search_empty_name();
	test_home_filter_existing_dirs();
	return 0;
}
