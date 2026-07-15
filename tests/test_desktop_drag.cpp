/*
 * Headless tests for desktop-drag policy and payload rules.
 *
 * GTK drag callbacks are still validated manually, but these tests pin the
 * pure decisions that keep Apps and Places source wiring consistent.
 */

#include "panel-plugin/core/desktop-drag.h"
#include "panel-plugin/ui/icon-size.h"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include <glib/gstdio.h>

using namespace WhiskerMenu;

// ---------------------------------------------------------------------------

static std::vector<std::string> external_uri_payload(const char* layout_mode,
		const char* uri)
{
	if (!desktop_drag_application_uri_available(layout_mode, uri))
	{
		return {};
	}
	return { uri };
}

static std::vector<std::string> places_uri_payload(const char* layout_mode,
		bool exists, const char* uri)
{
	if (!desktop_drag_places_uri_available(layout_mode, exists, uri))
	{
		return {};
	}
	return { uri };
}

// ---------------------------------------------------------------------------

static void test_icon_size_small_alias()
{
	assert(IconSize::pixels_for(IconSize::Small) == 32);

	IconSize configured(nullptr, "/launcher-icon-size", IconSize::Small);
	assert(configured.get_size() == IconSize::pixels_for(IconSize::Small));
	assert(desktop_drag_preview_size() == IconSize::pixels_for(IconSize::Small));
}

static void test_layout_gate()
{
	assert(desktop_drag_external_uri_enabled("docked"));
	assert(desktop_drag_external_uri_enabled("centered"));
	assert(!desktop_drag_external_uri_enabled("fullscreen"));

	// Unknown or missing values follow the existing layout parser's Docked
	// fallback, so older configs keep the windowed drag path available.
	assert(desktop_drag_external_uri_enabled(nullptr));
	assert(desktop_drag_external_uri_enabled("unexpected"));
}

static void test_application_uri_payload()
{
	const auto payload = external_uri_payload("docked",
			"file:///usr/share/applications/org.example.App.desktop");
	assert((payload == std::vector<std::string>{
			"file:///usr/share/applications/org.example.App.desktop" }));

	assert(external_uri_payload("centered",
			"file:///usr/share/applications/org.example.App.desktop").size() == 1);
	assert(external_uri_payload("fullscreen",
			"file:///usr/share/applications/org.example.App.desktop").empty());
	assert(external_uri_payload("docked", "").empty());
	assert(external_uri_payload("docked", nullptr).empty());
}

static void test_places_uri_payload()
{
	assert((places_uri_payload("docked", true,
			"file:///home/u/report.txt") == std::vector<std::string>{
			"file:///home/u/report.txt" }));
	assert((places_uri_payload("centered", true,
			"file:///home/u/Documents") == std::vector<std::string>{
			"file:///home/u/Documents" }));

	assert(places_uri_payload("fullscreen", true,
			"file:///home/u/report.txt").empty());
	assert(places_uri_payload("docked", false,
			"file:///home/u/missing.txt").empty());
	assert(places_uri_payload("docked", true, "").empty());
}

static void test_places_folder_launcher_payload()
{
	gchar* tmpdir = g_dir_make_tmp("meowmenu-test-folder-drag-XXXXXX", nullptr);
	assert(tmpdir);

	GFile* folder = g_file_new_for_path(tmpdir);
	gchar* source_uri = g_file_get_uri(folder);
	gchar* artifact_uri = desktop_drag_create_folder_launcher_uri(folder,
			"Project Folder");

	assert(artifact_uri);
	assert(std::strcmp(artifact_uri, source_uri) != 0);
	assert(g_str_has_prefix(artifact_uri, "file://"));

	GFile* artifact = g_file_new_for_uri(artifact_uri);
	assert(g_file_query_exists(artifact, nullptr));

	gchar* artifact_path = g_file_get_path(artifact);
	gchar* artifact_dir = g_path_get_dirname(artifact_path);
	gchar* contents = nullptr;
	assert(g_file_get_contents(artifact_path, &contents, nullptr, nullptr));
	assert(std::strstr(contents, "Type=Link"));
	assert(std::strstr(contents, "Name=Project Folder"));
	assert(std::strstr(contents, source_uri));

	desktop_drag_cleanup_folder_launcher_uri(artifact_uri);
	assert(!g_file_query_exists(artifact, nullptr));
	assert(!g_file_test(artifact_dir, G_FILE_TEST_EXISTS));
	desktop_drag_cleanup_folder_launcher_uri(artifact_uri);
	desktop_drag_cleanup_folder_launcher_uri(nullptr);

	g_free(contents);
	g_free(artifact_dir);
	g_free(artifact_path);
	g_object_unref(artifact);
	g_free(artifact_uri);
	g_free(source_uri);
	g_object_unref(folder);
	g_rmdir(tmpdir);
	g_free(tmpdir);
}

static void test_places_folder_launcher_rejects_files()
{
	gchar* tmpdir = g_dir_make_tmp("meowmenu-test-file-drag-XXXXXX", nullptr);
	assert(tmpdir);
	gchar* path = g_build_filename(tmpdir, "note.txt", nullptr);
	assert(g_file_set_contents(path, "hello", -1, nullptr));

	GFile* file = g_file_new_for_path(path);
	gchar* artifact_uri = desktop_drag_create_folder_launcher_uri(file,
			"note.txt");
	assert(!artifact_uri);

	g_object_unref(file);
	g_remove(path);
	g_free(path);
	g_rmdir(tmpdir);
	g_free(tmpdir);
}

static void test_places_folder_launcher_scheduled_cleanup()
{
	gchar* tmpdir = g_dir_make_tmp("meowmenu-test-folder-drag-XXXXXX", nullptr);
	assert(tmpdir);

	GFile* folder = g_file_new_for_path(tmpdir);
	gchar* artifact_uri = desktop_drag_create_folder_launcher_uri(folder,
			"Delayed Cleanup");
	assert(artifact_uri);

	GFile* artifact = g_file_new_for_uri(artifact_uri);
	assert(g_file_query_exists(artifact, nullptr));
	assert(desktop_drag_schedule_folder_launcher_cleanup(artifact_uri, 20) != 0);
	assert(g_file_query_exists(artifact, nullptr));

	const gint64 deadline = g_get_monotonic_time() + (2 * G_USEC_PER_SEC);
	while (g_file_query_exists(artifact, nullptr)
			&& g_get_monotonic_time() < deadline)
	{
		while (g_main_context_iteration(nullptr, FALSE))
		{
		}
		g_usleep(1000);
	}
	assert(!g_file_query_exists(artifact, nullptr));

	g_free(artifact_uri);
	g_object_unref(artifact);
	g_object_unref(folder);
	g_rmdir(tmpdir);
	g_free(tmpdir);
}

static void test_drag_end_never_hides_menu()
{
	assert(!desktop_drag_should_hide_menu_after_end());
}

static void test_context_menu_fallback_is_no_overwrite()
{
	assert(!desktop_drag_context_menu_allows_overwrite());
}

// ---------------------------------------------------------------------------

int main()
{
	test_icon_size_small_alias();
	test_layout_gate();
	test_application_uri_payload();
	test_places_uri_payload();
	test_places_folder_launcher_payload();
	test_places_folder_launcher_rejects_files();
	test_places_folder_launcher_scheduled_cleanup();
	test_drag_end_never_hides_menu();
	test_context_menu_fallback_is_no_overwrite();
	return 0;
}
