/*
 * Headless tests for the pure icon-chain resolver declared in
 * panel-plugin/ui/switch-icons.h.
 *
 * A synthetic on-disk icon theme is built in a temp dir so the "present" cases
 * do not depend on whatever icons the build host happens to ship. The resolver
 * itself touches no display, only GtkIconTheme file lookups.
 */

#include "ui/switch-icons.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cstdio>
#include <cstdlib>

using namespace WhiskerMenu;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// Write a minimal icon theme under @root providing exactly the named icons in a
// single Actions directory. has_icon() only needs a matching filename present
// in a listed Directory, so empty .png files are sufficient.
void seed_theme(const char* root, const char* const* icons)
{
	gchar* actions = g_build_filename(root, "meowtest", "scalable", "actions", nullptr);
	g_mkdir_with_parents(actions, 0700);

	GString* index = g_string_new("[Icon Theme]\n");
	g_string_append(index, "Name=meowtest\n");
	g_string_append(index, "Comment=test\n");
	g_string_append(index, "Directories=scalable/actions\n\n");
	g_string_append(index, "[scalable/actions]\n");
	g_string_append(index, "Size=16\nMinSize=8\nMaxSize=512\nContext=Actions\nType=Scalable\n");

	gchar* index_path = g_build_filename(root, "meowtest", "index.theme", nullptr);
	g_file_set_contents(index_path, index->str, -1, nullptr);
	g_string_free(index, TRUE);
	g_free(index_path);

	for (const char* const* p = icons; *p; ++p)
	{
		gchar* fname = g_strconcat(*p, ".svg", nullptr);
		gchar* path = g_build_filename(actions, fname, nullptr);
		// A trivial but valid SVG so the file is a real icon, not just a stub.
		g_file_set_contents(path,
				"<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'/>\n",
				-1, nullptr);
		g_free(path);
		g_free(fname);
	}
	g_free(actions);
}

GtkIconTheme* make_theme(const char* root)
{
	GtkIconTheme* theme = gtk_icon_theme_new();
	const gchar* path[] = { root };
	gtk_icon_theme_set_search_path(theme, path, 1);
	gtk_icon_theme_set_custom_theme(theme, "meowtest");
	return theme;
}

} // namespace

int main()
{
	// GtkIconTheme lookups work without a display; gtk_init_check just primes
	// any global state and stays headless-safe if no display is present.
	gtk_init_check(nullptr, nullptr);

	gchar* root = g_dir_make_tmp("meow-switch-icons-XXXXXX", nullptr);
	if (!root)
	{
		std::printf("test_switch_icons: SKIP (no temp dir)\n");
		return EXIT_SUCCESS;
	}

	// Present icons: only "view-grid-symbolic" and "folder" exist.
	const char* present[] = { "view-grid-symbolic", "folder", nullptr };
	seed_theme(root, present);

	GtkIconTheme* theme = make_theme(root);

	// First-present wins: chain's first entry exists.
	const char* chain_first[] = { "folder", "does-not-exist", nullptr };
	CHECK(g_strcmp0(meow_resolve_icon_name(theme, chain_first), "folder") == 0);

	// Mid-chain fallback: first absent, second present.
	const char* chain_mid[] = { "absent-1", "view-grid-symbolic", "absent-2", nullptr };
	CHECK(g_strcmp0(meow_resolve_icon_name(theme, chain_mid), "view-grid-symbolic") == 0);

	// All absent: returns the last entry unconditionally (never NULL/empty).
	const char* chain_none[] = { "absent-a", "absent-b", "absent-c", nullptr };
	const char* got = meow_resolve_icon_name(theme, chain_none);
	CHECK(got != nullptr && got[0] != '\0');
	CHECK(g_strcmp0(got, "absent-c") == 0);

	// The shipped Places chain resolves to the present non-symbolic "folder".
	CHECK(g_strcmp0(meow_resolve_icon_name(theme, MEOW_SWITCH_PLACES_ICONS), "folder") == 0);
	// The shipped Apps chain resolves to the present "view-grid-symbolic".
	CHECK(g_strcmp0(meow_resolve_icon_name(theme, MEOW_SWITCH_APPS_ICONS), "view-grid-symbolic") == 0);

	g_object_unref(theme);

	// Short-vs-long label selection (FR-012/014). Text mode: the visible label
	// is the short word, the accessible name is the long descriptive name, no
	// tooltip (the label already reads it). Icon mode: no visible text, the long
	// name is both accessible name and tooltip.
	{
		ModeButtonLabels text_mode = meow_mode_button_labels(false, "Apps", "Applications");
		CHECK(g_strcmp0(text_mode.visible_text, "Apps") == 0);
		CHECK(g_strcmp0(text_mode.accessible_name, "Applications") == 0);
		CHECK(text_mode.tooltip_text == nullptr);

		ModeButtonLabels icon_mode = meow_mode_button_labels(true, "Apps", "Applications");
		CHECK(icon_mode.visible_text == nullptr);
		CHECK(g_strcmp0(icon_mode.accessible_name, "Applications") == 0);
		CHECK(g_strcmp0(icon_mode.tooltip_text, "Applications") == 0);

		// Places: short and long happen to coincide; selection still holds.
		ModeButtonLabels places_text = meow_mode_button_labels(false, "Places", "Places");
		CHECK(g_strcmp0(places_text.visible_text, "Places") == 0);
		CHECK(places_text.tooltip_text == nullptr);
	}

	// Clean up the synthetic theme tree (best effort).
	gchar* rm = g_strdup_printf("rm -rf '%s'", root);
	if (system(rm) != 0)
		std::fprintf(stderr, "warning: could not remove %s\n", root);
	g_free(rm);
	g_free(root);

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_switch_icons: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_switch_icons: ok\n");
	return EXIT_SUCCESS;
}
