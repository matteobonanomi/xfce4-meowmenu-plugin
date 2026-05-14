/*
 * Unit tests for milestone 005 amendment (Places Mode — recursive Home
 * search). Mirrors HomeSearchWorker's BFS walk over a temporary
 * directory tree without instantiating GTK widgets or threads,
 * matching the stand-in pattern used by the rest of tests/.
 *
 * Covers: case-folded match, shallow-first ordering, hidden-dir skipping,
 * blocklist skipping, no symlink-to-dir traversal, cap enforcement,
 * cancellation between layers, hard-budget termination.
 */

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

// ---------------------------------------------------------------------------
// Stand-in mirror of HomeSearchWorker::walk(). Synchronous; no threads.
// Returns matched basenames in dispatch order.
// ---------------------------------------------------------------------------

static const char* const BLOCKLIST[] = {
	"node_modules", "__pycache__", "target", "build", "dist", nullptr
};

static bool is_blocklisted(const char* name)
{
	for (const char* const* p = BLOCKLIST; *p; ++p)
	{
		if (g_strcmp0(name, *p) == 0) return true;
	}
	return false;
}

struct WalkOptions
{
	int cap = -1;           // -1 = no cap
	gint64 hard_budget_us = 0; // 0 = no budget
	int cancel_after = -1;  // cancel after N dispatched matches; -1 = never
};

static std::vector<std::string> walk_for_test(const std::string& root,
		const std::string& folded_query,
		const WalkOptions& opts)
{
	std::vector<std::string> out;
	std::deque<std::string> queue;
	queue.push_back(root);

	const gint64 start = g_get_monotonic_time();
	int dispatched = 0;
	bool cancelled = false;

	while (!queue.empty() && !cancelled)
	{
		if (opts.hard_budget_us > 0
				&& (g_get_monotonic_time() - start) >= opts.hard_budget_us)
		{
			break;
		}

		const std::string dir = queue.front();
		queue.pop_front();

		GError* error = nullptr;
		GFile* gdir = g_file_new_for_path(dir.c_str());
		GFileEnumerator* en = g_file_enumerate_children(gdir,
				G_FILE_ATTRIBUTE_STANDARD_NAME ","
				G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
				G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				nullptr, &error);
		if (!en)
		{
			if (error) g_error_free(error);
			g_object_unref(gdir);
			continue;
		}

		struct M { std::string sort_key; std::string path; };
		std::vector<M> matches;
		std::vector<std::string> subdirs;

		while (true)
		{
			GFileInfo* info = g_file_enumerator_next_file(en, nullptr, nullptr);
			if (!info) break;

			const char* name = g_file_info_get_name(info);
			const GFileType type = g_file_info_get_file_type(info);
			const gboolean sym = g_file_info_get_is_symlink(info);

			if (!name || name[0] == '.' || is_blocklisted(name))
			{
				g_object_unref(info);
				continue;
			}
			if (type == G_FILE_TYPE_DIRECTORY && sym)
			{
				g_object_unref(info);
				continue;
			}

			std::string child_path = dir + "/" + name;
			gchar* folded = g_utf8_casefold(name, -1);
			if (folded && strstr(folded, folded_query.c_str()) != nullptr)
			{
				matches.push_back({ folded ? folded : "", child_path });
			}
			g_free(folded);

			if (type == G_FILE_TYPE_DIRECTORY)
			{
				subdirs.push_back(child_path);
			}
			g_object_unref(info);
		}

		g_file_enumerator_close(en, nullptr, nullptr);
		g_object_unref(en);
		g_object_unref(gdir);

		std::sort(matches.begin(), matches.end(),
				[](const M& a, const M& b) { return a.sort_key < b.sort_key; });

		for (auto& m : matches)
		{
			out.push_back(g_path_get_basename(m.path.c_str()));
			++dispatched;
			if (opts.cap >= 0 && dispatched >= opts.cap)
			{
				cancelled = true;
				break;
			}
			if (opts.cancel_after >= 0 && dispatched >= opts.cancel_after)
			{
				cancelled = true;
				break;
			}
		}

		for (auto& s : subdirs)
		{
			queue.push_back(s);
		}
	}

	return out;
}

// ---------------------------------------------------------------------------
// Temp tree helpers.
// ---------------------------------------------------------------------------

static std::string make_tmp_root()
{
	gchar* tmpl = g_build_filename(g_get_tmp_dir(), "meowmenu-search-XXXXXX", nullptr);
	gchar* path = g_mkdtemp(tmpl);
	std::string result = path ? path : "";
	g_free(tmpl);
	return result;
}

static void mkdir_p(const std::string& path)
{
	g_mkdir_with_parents(path.c_str(), 0755);
}

static void touch(const std::string& path)
{
	FILE* f = std::fopen(path.c_str(), "w");
	if (f) std::fclose(f);
}

static void rmtree(const std::string& root)
{
	GFile* gf = g_file_new_for_path(root.c_str());
	GFileEnumerator* en = g_file_enumerate_children(gf,
			G_FILE_ATTRIBUTE_STANDARD_NAME ","
			G_FILE_ATTRIBUTE_STANDARD_TYPE,
			G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
			nullptr, nullptr);
	if (en)
	{
		while (true)
		{
			GFileInfo* info = g_file_enumerator_next_file(en, nullptr, nullptr);
			if (!info) break;
			const char* name = g_file_info_get_name(info);
			std::string child = root + "/" + (name ? name : "");
			GFileType t = g_file_info_get_file_type(info);
			gboolean sym = g_file_info_get_is_symlink(info);
			if (t == G_FILE_TYPE_DIRECTORY && !sym)
			{
				rmtree(child);
			}
			else
			{
				g_unlink(child.c_str());
			}
			g_object_unref(info);
		}
		g_file_enumerator_close(en, nullptr, nullptr);
		g_object_unref(en);
	}
	g_object_unref(gf);
	g_rmdir(root.c_str());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_basic_match()
{
	const std::string root = make_tmp_root();
	assert(!root.empty());
	mkdir_p(root + "/projects/notes");
	touch(root + "/projects/notes/meeting.md");
	touch(root + "/projects/unrelated.txt");

	auto out = walk_for_test(root, "meeting", {});
	assert(out.size() == 1);
	assert(out[0] == "meeting.md");
	rmtree(root);
}

static void test_no_match()
{
	const std::string root = make_tmp_root();
	mkdir_p(root + "/a");
	touch(root + "/a/file.txt");
	auto out = walk_for_test(root, "zzzzzz", {});
	assert(out.empty());
	rmtree(root);
}

static void test_shallow_first_ordering()
{
	const std::string root = make_tmp_root();
	mkdir_p(root + "/deep/inner");
	touch(root + "/match-top.txt");
	touch(root + "/deep/match-mid.txt");
	touch(root + "/deep/inner/match-deep.txt");

	auto out = walk_for_test(root, "match", {});
	assert(out.size() == 3);
	// Shallow-first: top before mid before deep.
	assert(out[0] == "match-top.txt");
	assert(out[1] == "match-mid.txt");
	assert(out[2] == "match-deep.txt");
	rmtree(root);
}

static void test_hidden_and_blocklist_skipped()
{
	const std::string root = make_tmp_root();
	mkdir_p(root + "/.hidden");
	touch(root + "/.hidden/target-in-hidden.txt");
	mkdir_p(root + "/node_modules");
	touch(root + "/node_modules/target-in-block.txt");
	mkdir_p(root + "/__pycache__");
	touch(root + "/__pycache__/target-pyc.txt");
	mkdir_p(root + "/visible");
	touch(root + "/visible/target-ok.txt");

	auto out = walk_for_test(root, "target", {});
	// Only the visible match should surface.
	assert(out.size() == 1);
	assert(out[0] == "target-ok.txt");
	rmtree(root);
}

static void test_symlink_to_dir_not_followed()
{
	const std::string root = make_tmp_root();
	mkdir_p(root + "/real");
	touch(root + "/real/inside-real.txt");
	// symlink "link" -> "real"; "inside-real" must not surface via the link.
#ifdef G_OS_UNIX
	GError* err = nullptr;
	GFile* link = g_file_new_for_path((root + "/link").c_str());
	g_file_make_symbolic_link(link, "real", nullptr, &err);
	if (err) { g_error_free(err); }
	g_object_unref(link);
#endif

	auto out = walk_for_test(root, "inside", {});
	// Exactly one match (through "real"); not duplicated via "link".
	assert(out.size() == 1);
	rmtree(root);
}

static void test_cap_enforced()
{
	const std::string root = make_tmp_root();
	for (int i = 0; i < 10; ++i)
	{
		gchar* fn = g_strdup_printf("%s/cap-match-%02d.txt", root.c_str(), i);
		touch(fn);
		g_free(fn);
	}
	WalkOptions opts;
	opts.cap = 3;
	auto out = walk_for_test(root, "cap-match", opts);
	assert(out.size() == 3);
	rmtree(root);
}

static void test_cancellation_between_layers()
{
	const std::string root = make_tmp_root();
	touch(root + "/c-1.txt");
	touch(root + "/c-2.txt");
	mkdir_p(root + "/sub");
	touch(root + "/sub/c-3.txt");
	touch(root + "/sub/c-4.txt");

	WalkOptions opts;
	opts.cancel_after = 1; // cancel after 1 dispatched match
	auto out = walk_for_test(root, "c-", opts);
	assert(out.size() == 1);
	rmtree(root);
}

static void test_hard_budget_terminates()
{
	const std::string root = make_tmp_root();
	// Build a moderately bushy tree.
	for (int i = 0; i < 8; ++i)
	{
		gchar* d = g_strdup_printf("%s/d%d", root.c_str(), i);
		mkdir_p(d);
		for (int j = 0; j < 8; ++j)
		{
			gchar* f = g_strdup_printf("%s/budget-%d-%d.txt", d, i, j);
			touch(f);
			g_free(f);
		}
		g_free(d);
	}
	WalkOptions opts;
	opts.hard_budget_us = 1; // 1 microsecond — fires immediately
	auto out = walk_for_test(root, "budget", opts);
	// Budget terminates before completion; result count is bounded
	// (often 0 or small). The contract is "doesn't run to completion".
	assert(out.size() < 64);
	rmtree(root);
}

int main(int, char**)
{
	test_basic_match();
	test_no_match();
	test_shallow_first_ordering();
	test_hidden_and_blocklist_skipped();
	test_symlink_to_dir_not_followed();
	test_cap_enforced();
	test_cancellation_between_layers();
	test_hard_budget_terminates();
	return 0;
}
