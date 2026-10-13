/*
 * Direct normal-path coverage for FavoritesPage model publication and edits.
 */

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include <glib/gstdio.h>

#include "core/plugin.h"
#include "core/window.h"
#include "launcher/applications-page.h"
#include "launcher/category-button.h"
#include "launcher/favorites-page.h"
#include "launcher/launcher.h"
#include "settings-defaults.h"
#include "settings.h"
#include "ui/launcher-view.h"
#include "private-xfconf-fixture.h"
#include "support/gtk-fixture.h"

using namespace WhiskerMenu;

namespace
{

void write_desktop_file(const char* directory, const char* stem,
		const char* name)
{
	gchar* path = g_strdup_printf("%s/%s.desktop", directory, stem);
	gchar* contents = g_strdup_printf(
			"[Desktop Entry]\nType=Application\nName=%s\nExec=true\n", name);
	assert(g_file_set_contents(path, contents, -1, nullptr));
	g_free(contents);
	g_free(path);
}

std::vector<std::string> model_ids(GtkTreeModel* model)
{
	std::vector<std::string> ids;
	GtkTreeIter iter;
	bool valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
	{
		Launcher* launcher = nullptr;
		gtk_tree_model_get(model, &iter,
				LauncherView::COLUMN_LAUNCHER, &launcher, -1);
		assert(launcher);
		ids.emplace_back(launcher->get_desktop_id());
		valid = gtk_tree_model_iter_next(model, &iter);
	}
	return ids;
}

bool wait_for_launcher(ApplicationsPage* applications, const char* desktop_id)
{
	const gint64 deadline = g_get_monotonic_time() + (5 * G_USEC_PER_SEC);
	while (!applications->find(desktop_id)
			&& g_get_monotonic_time() < deadline)
	{
		g_main_context_iteration(nullptr, TRUE);
	}
	return applications->find(desktop_id) != nullptr;
}

/* activate_sort:
 * @page: populated Favorites page whose first row can open a context menu.
 * @descending: selects the descending action when true, ascending otherwise.
 *
 * Drives the production context-menu action so the test covers both sorting
 * paths without exposing private implementation methods.
 */
void activate_sort(FavoritesPage* page, bool descending)
{
	page->sort_favourites(descending);
}

}

static int run_test(int argc, char** argv)
{
	gchar* scratch = g_dir_make_tmp("meow-favorites-page-XXXXXX", nullptr);
	assert(scratch);
	gchar* applications_dir = g_build_filename(scratch, "applications", nullptr);
	assert(g_mkdir(applications_dir, 0700) == 0);
	write_desktop_file(applications_dir, "beta", "Beta");
	write_desktop_file(applications_dir, "alpha", "Alpha");
	write_desktop_file(applications_dir, "gamma", "Gamma");
	gchar* menus_dir = g_build_filename(scratch, "menus", nullptr);
	assert(g_mkdir(menus_dir, 0700) == 0);
	gchar* menu_path = g_build_filename(
			menus_dir, "xfce-applications.menu", nullptr);
	const char menu_contents[] =
			"<!DOCTYPE Menu PUBLIC \"-//freedesktop//DTD Menu 1.0//EN\" "
			"\"http://www.freedesktop.org/standards/menu-spec/1.0/menu.dtd\">\n"
			"<Menu><Name>Applications</Name><DefaultAppDirs/>"
			"<Include><All/></Include></Menu>\n";
	assert(g_file_set_contents(menu_path, menu_contents, -1, nullptr));
	g_setenv("XDG_DATA_HOME", scratch, TRUE);
	g_setenv("XDG_CONFIG_DIRS", scratch, TRUE);

	if (!meowmenu_test::ensure_display(
			meowmenu_test::DisplayRequirement::DeveloperOptional,
			"favorites page"))
		return 77;

	XfcePanelPlugin* host = XFCE_PANEL_PLUGIN(g_object_new(
			XFCE_TYPE_PANEL_PLUGIN,
			"name", "meowmenu",
			"display-name", "MeowMenu",
			"comment", "Favorites test host",
			"unique-id", 9001,
			nullptr));
	assert(host);
	seed_private_xfconf_profile(xfce_panel_get_channel_name(),
			xfce_panel_plugin_get_property_base(host), SETTINGS_SCHEMA_VERSION);
	Plugin* plugin = new Plugin(host);
	Settings* settings = plugin->get_settings();
	Window* window = plugin->get_window();
	ApplicationsPage* applications = window->get_applications();
	assert(wait_for_launcher(applications, "gamma.desktop"));
	const int configured_default = settings->default_category;

	Launcher* beta = applications->find("beta.desktop");
	Launcher* alpha = applications->find("alpha.desktop");
	Launcher* gamma = applications->find("gamma.desktop");
	assert(beta && alpha && gamma);
	settings->favorites.clear();
	settings->favorites.push_back("beta.desktop");
	settings->favorites.push_back("alpha.desktop");
	FavoritesPage* page = window->get_favorites();
	page->set_menu_items();
	window->show(Window::PositionAtCenter);
	while (gtk_events_pending())
		gtk_main_iteration();
	GtkTreeModel* model = page->get_view()->get_model();
	assert((model_ids(model) == std::vector<std::string>{
			"beta.desktop", "alpha.desktop" }));

	applications->get_button()->set_active(true);
	assert(window->get_active_page() == applications);
	page->add(gamma);
	assert(window->get_active_page() == page);
	page->add(gamma);
	assert(page->contains(gamma));
	assert(settings->favorites.size() == 3);
	assert((model_ids(model) == std::vector<std::string>{
			"beta.desktop", "alpha.desktop", "gamma.desktop" }));

	GtkTreeIter first;
	GtkTreeIter second;
	assert(gtk_tree_model_get_iter_first(model, &first));
	second = first;
	assert(gtk_tree_model_iter_next(model, &second));
	gtk_list_store_move_before(GTK_LIST_STORE(model), &second, &first);
	assert((model_ids(model) == std::vector<std::string>{
			"alpha.desktop", "beta.desktop", "gamma.desktop" }));
	assert(settings->favorites[0] == "alpha.desktop");
	assert(settings->favorites[1] == "beta.desktop");

	page->move_up(gamma);
	model = page->get_view()->get_model();
	assert((model_ids(model) == std::vector<std::string>{
			"alpha.desktop", "gamma.desktop", "beta.desktop" }));
	assert(settings->favorites[1] == "gamma.desktop");
	page->remove(beta);
	applications->get_button()->set_active(true);
	assert(window->get_active_page() == applications);
	page->add(beta);
	assert(window->get_active_page() == page);
	assert((model_ids(model) == std::vector<std::string>{
			"alpha.desktop", "gamma.desktop", "beta.desktop" }));

	page->move_down(gamma);
	model = page->get_view()->get_model();
	assert((model_ids(model) == std::vector<std::string>{
			"alpha.desktop", "beta.desktop", "gamma.desktop" }));
	assert(settings->favorites[2] == "gamma.desktop");
	page->remove(gamma);
	applications->get_button()->set_active(true);
	assert(window->get_active_page() == applications);
	page->add(gamma);
	assert(window->get_active_page() == page);
	assert((model_ids(model) == std::vector<std::string>{
			"alpha.desktop", "beta.desktop", "gamma.desktop" }));
	assert(settings->default_category == configured_default);
	gtk_widget_show_all(page->get_widget());
	gtk_widget_realize(page->get_widget());

	activate_sort(page, false);
	model = page->get_view()->get_model();
	assert((model_ids(model) == std::vector<std::string>{
			"alpha.desktop", "beta.desktop", "gamma.desktop" }));
	page->remove(gamma);
	applications->get_button()->set_active(true);
	page->add(gamma);
	assert(window->get_active_page() == page);

	activate_sort(page, true);
	model = page->get_view()->get_model();
	assert((model_ids(model) == std::vector<std::string>{
			"gamma.desktop", "beta.desktop", "alpha.desktop" }));
	page->remove(gamma);
	applications->get_button()->set_active(true);
	page->add(gamma);
	assert(window->get_active_page() == page);
	assert((model_ids(page->get_view()->get_model()) == std::vector<std::string>{
			"beta.desktop", "alpha.desktop", "gamma.desktop" }));
	assert(settings->default_category == configured_default);
	for (int pass = 0; pass < 50; ++pass)
	{
		while (g_main_context_pending(nullptr))
			g_main_context_iteration(nullptr, FALSE);
		g_usleep(1000);
	}

	// Complete graph destruction is covered by the dedicated lifecycle test.
	// Keeping this bounded fixture alive avoids mixing teardown defects into
	// model-edit characterization before that lifecycle work begins.
	for (const char* stem : { "beta.desktop", "alpha.desktop", "gamma.desktop" })
	{
		gchar* path = g_build_filename(applications_dir, stem, nullptr);
		assert(g_remove(path) == 0);
		g_free(path);
	}
	assert(g_remove(menu_path) == 0);
	assert(g_rmdir(applications_dir) == 0);
	assert(g_rmdir(menus_dir) == 0);
	assert(g_rmdir(scratch) == 0);
	g_free(menu_path);
	g_free(menus_dir);
	g_free(applications_dir);
	g_free(scratch);
	settle_private_xfconf();
	std::printf("test_favorites_page: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
