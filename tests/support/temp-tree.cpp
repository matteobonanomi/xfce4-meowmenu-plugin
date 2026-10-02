#include "temp-tree.h"

#include <glib.h>
#include <glib/gstdio.h>

namespace
{

void remove_tree(const gchar* path)
{
	GDir* directory = g_dir_open(path, 0, nullptr);
	if (directory)
	{
		while (const gchar* name = g_dir_read_name(directory))
		{
			gchar* child = g_build_filename(path, name, nullptr);
			if (g_file_test(child, G_FILE_TEST_IS_DIR))
				remove_tree(child);
			else
				g_remove(child);
			g_free(child);
		}
		g_dir_close(directory);
	}
	g_rmdir(path);
}

}

namespace meowmenu_test
{

TemporaryTree::TemporaryTree()
{
	gchar* path = g_dir_make_tmp("meowmenu-test-XXXXXX", nullptr);
	if (path)
	{
		root_ = path;
		g_free(path);
	}
}

TemporaryTree::~TemporaryTree()
{
	if (!root_.empty())
		remove_tree(root_.c_str());
}

std::string TemporaryTree::path(const char* name) const
{
	gchar* joined = g_build_filename(root_.c_str(), name, nullptr);
	const std::string result = joined ? joined : "";
	g_free(joined);
	return result;
}

bool TemporaryTree::mkdir(const char* name) const
{
	const std::string directory = path(name);
	const bool success = g_mkdir_with_parents(directory.c_str(), 0700) == 0;
	return success;
}

bool TemporaryTree::write(const char* name, const char* content) const
{
	const std::string filename = path(name);
	GError* error = nullptr;
	const bool success = g_file_set_contents(filename.c_str(), content, -1, &error);
	if (!success)
	{
		g_test_message("could not write temporary fixture: %s",
				error ? error->message : "unknown error");
		g_clear_error(&error);
	}
	return success;
}

}
