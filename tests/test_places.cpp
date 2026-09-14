/*
 * Unit tests for Places behavior and presentation.
 *
 * The markup cases construct the production PlacesItem. The remaining small
 * helpers pin filtering behavior without constructing a complete Places page.
 */

#include "places/places-item.h"

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
// Stand-in for FavouritesSection::get_items() in MeowMenuOnly mode: remove
// missing/unreachable URIs before visible item construction.
// ---------------------------------------------------------------------------
static std::vector<std::string> prune_existing_favourite_uris(
		std::vector<std::string>& uris)
{
	std::vector<std::string> visible;
	for (auto it = uris.begin(); it != uris.end(); )
	{
		GFile* file = g_file_new_for_uri(it->c_str());
		const bool exists = file && g_file_query_exists(file, nullptr);
		if (file)
		{
			g_object_unref(file);
		}

		if (!exists)
		{
			it = uris.erase(it);
			continue;
		}

		visible.push_back(*it);
		++it;
	}
	return visible;
}

// ---------------------------------------------------------------------------
// Stand-in for FavouritesSection::get_items() in read-only/external mode:
// hide missing/unreachable URIs from MeowMenu's visible model without changing
// the externally managed source list.
// ---------------------------------------------------------------------------
static std::vector<std::string> hide_missing_external_favourite_uris(
		const std::vector<std::string>& uris)
{
	std::vector<std::string> visible;
	for (const auto& uri : uris)
	{
		GFile* file = g_file_new_for_uri(uri.c_str());
		const bool exists = file && g_file_query_exists(file, nullptr);
		if (file)
		{
			g_object_unref(file);
		}

		if (exists)
		{
			visible.push_back(uri);
		}
	}
	return visible;
}

static std::string escape_markup(const char* text)
{
	gchar* escaped = g_markup_escape_text(text ? text : "", -1);
	std::string out = escaped ? escaped : "";
	g_free(escaped);
	return out;
}

static std::string missing_markup(const char* escaped_name)
{
	gchar* markup = g_strdup_printf("<span alpha=\"55%%\">%s</span>",
			escaped_name ? escaped_name : "");
	std::string out = markup ? markup : "";
	g_free(markup);
	return out;
}

// Validate Pango span well-formedness using GLib's markup parser (no Pango
// dependency): confirm the string parses and its root element is <span> with an
// "alpha" attribute.
static bool is_valid_alpha_span(const std::string& markup)
{
	struct Captured { std::string element; bool has_alpha = false; };
	Captured cap;

	GMarkupParser parser = {};
	parser.start_element = [](GMarkupParseContext*, const gchar* name,
			const gchar** attr_names, const gchar** attr_values,
			gpointer user_data, GError**)
	{
		auto* c = static_cast<Captured*>(user_data);
		if (c->element.empty())
		{
			c->element = name ? name : "";
			for (int i = 0; attr_names && attr_names[i]; ++i)
			{
				if (g_strcmp0(attr_names[i], "alpha") == 0)
				{
					c->has_alpha = true;
				}
			}
			(void) attr_values;
		}
	};

	GMarkupParseContext* ctx = g_markup_parse_context_new(&parser,
			GMarkupParseFlags(0), &cap, nullptr);
	gboolean ok = g_markup_parse_context_parse(ctx, markup.c_str(), -1, nullptr)
			&& g_markup_parse_context_end_parse(ctx, nullptr);
	g_markup_parse_context_free(ctx);

	return ok && cap.element == "span" && cap.has_alpha;
}

// ---------------------------------------------------------------------------

/* test_places_item_markup:
 * @exists: whether the fixture target is created before constructing the item.
 * @is_favourite: whether the item represents the favourites result path.
 *
 * Constructs the real PlacesItem with a hostile literal filename and verifies
 * that both ordinary and favourite result paths expose exactly-once escaped
 * markup while retaining their distinct available/missing tooltip behavior.
 */
static void test_places_item_markup(bool exists, bool is_favourite)
{
	const char* name = "Tom & Jerry <draft> 100%.txt";
	gchar* tmpdir = g_dir_make_tmp("meowmenu-markup-XXXXXX", nullptr);
	assert(tmpdir);
	gchar* path = g_build_filename(tmpdir, name, nullptr);
	assert(path);
	if (exists)
	{
		assert(g_file_set_contents(path, "fixture", -1, nullptr));
	}

	GFile* file = g_file_new_for_path(path);
	assert(file);
	WhiskerMenu::PlacesItem item(file, is_favourite);
	g_object_unref(file);

	assert(item.exists() == exists);
	assert(item.is_favourite() == is_favourite);
	assert(std::string(item.get_text()) == name);
	const std::string escaped = escape_markup(name);
	const std::string markup = item.get_display_markup();
	assert(markup == (exists ? escaped : missing_markup(escaped.c_str())));
	assert(markup.find("&amp;") != std::string::npos);
	assert(markup.find("&lt;draft&gt;") != std::string::npos);
	assert(markup.find("100%") != std::string::npos);
	assert(markup.find("&amp;amp;") == std::string::npos);
	if (exists)
	{
		assert(markup.find("<span") == std::string::npos);
	}
	else
	{
		assert(is_valid_alpha_span(markup));
	}

	const std::string escaped_path = escape_markup(path);
	const std::string tooltip = item.get_tooltip() ? item.get_tooltip() : "";
	assert(tooltip.find(escaped_path) != std::string::npos);
	assert((tooltip == escaped_path) == exists);

	if (exists)
	{
		assert(g_remove(path) == 0);
	}
	assert(g_rmdir(tmpdir) == 0);
	g_free(path);
	g_free(tmpdir);
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

static void test_meowmenu_favourites_prune_missing_uris()
{
	gchar* tmpdir = g_dir_make_tmp("meowmenu-fav-prune-XXXXXX", nullptr);
	assert(tmpdir);

	gchar* kept_path = g_build_filename(tmpdir, "kept.txt", nullptr);
	assert(g_file_set_contents(kept_path, "kept", -1, nullptr));

	gchar* kept_uri = g_filename_to_uri(kept_path, nullptr, nullptr);
	assert(kept_uri);

	std::vector<std::string> stored = {
		kept_uri,
		std::string("file://") + tmpdir + "/missing.txt",
	};

	const auto visible = prune_existing_favourite_uris(stored);
	assert(visible.size() == 1);
	assert(visible[0] == kept_uri);
	assert(stored.size() == 1);
	assert(stored[0] == kept_uri);

	g_remove(kept_path);
	g_rmdir(tmpdir);
	g_free(kept_uri);
	g_free(kept_path);
	g_free(tmpdir);
}

static void test_external_favourites_hide_missing_uris_without_pruning_source()
{
	gchar* tmpdir = g_dir_make_tmp("meowmenu-fav-hide-XXXXXX", nullptr);
	assert(tmpdir);

	gchar* kept_path = g_build_filename(tmpdir, "kept.txt", nullptr);
	assert(g_file_set_contents(kept_path, "kept", -1, nullptr));

	gchar* kept_uri = g_filename_to_uri(kept_path, nullptr, nullptr);
	assert(kept_uri);

	const std::vector<std::string> external_source = {
		kept_uri,
		std::string("file://") + tmpdir + "/missing.txt",
	};

	const auto visible = hide_missing_external_favourite_uris(external_source);
	assert(visible.size() == 1);
	assert(visible[0] == kept_uri);
	assert(external_source.size() == 2);
	assert(external_source[1].find("missing.txt") != std::string::npos);

	g_remove(kept_path);
	g_rmdir(tmpdir);
	g_free(kept_uri);
	g_free(kept_path);
	g_free(tmpdir);
}

int main()
{
	test_places_item_markup(true, false);
	test_places_item_markup(false, false);
	test_places_item_markup(true, true);
	test_places_item_markup(false, true);
	test_search_empty_filter_matches_everything();
	test_search_case_insensitive_ascii();
	test_search_utf8();
	test_search_empty_name();
	test_home_filter_existing_dirs();
	test_meowmenu_favourites_prune_missing_uris();
	test_external_favourites_hide_missing_uris_without_pruning_source();
	return 0;
}
