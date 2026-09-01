/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "applications-page.h"

#include "launcher/application-load-generation.h"
#include "launcher/category.h"
#include "launcher/category-button.h"
#include "core/window-frame.h"
#include "launcher.h"
#include "ui/launcher-view.h"
#include "settings.h"
#include "ui/slot.h"
#include "core/window.h"

#include <algorithm>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

struct ApplicationsPage::ApplicationCandidate
{
	ApplicationCandidate() :
		menu(nullptr),
		settings_menu(nullptr),
		source_loaded(false)
	{
	}

	/* Releases a retired graph after every GTK model has detached from it. */
	~ApplicationCandidate()
	{
		for (Category* category : categories)
		{
			delete category;
		}
		for (const auto& item : items)
		{
			delete item.second;
		}
		if (menu)
		{
			g_object_unref(menu);
		}
		if (settings_menu)
		{
			g_object_unref(settings_menu);
		}
	}

	GarconMenu* menu;
	GarconMenu* settings_menu;
	bool source_loaded;
	std::vector<Category*> categories;
	std::unordered_map<std::string, Launcher*> items;
};

//-----------------------------------------------------------------------------

struct ApplicationsPage::LoadJob
{
	LoadJob(ApplicationsPage* owner, guint64 generation_id) :
		page(owner),
		generation(generation_id),
		candidate(new ApplicationCandidate()),
		cancelled(FALSE),
		worker_done(false)
	{
		g_mutex_init(&mutex);
		g_cond_init(&cond);
		generation.start();
	}

	/* Releases the isolated candidate only after its worker has stopped. */
	~LoadJob()
	{
		delete candidate;
		g_cond_clear(&cond);
		g_mutex_clear(&mutex);
	}

	void cancel()
	{
		g_atomic_int_set(&cancelled, TRUE);
		generation.cancel();
	}

	bool is_cancelled() const
	{
		return g_atomic_int_get(const_cast<gint*>(&cancelled));
	}

	/* Wakes teardown code waiting for the worker's final ownership release. */
	void mark_worker_done()
	{
		g_mutex_lock(&mutex);
		worker_done = true;
		g_cond_broadcast(&cond);
		g_mutex_unlock(&mutex);
	}

	/* Blocks destruction until no worker can access the candidate graph. */
	void wait_for_worker()
	{
		g_mutex_lock(&mutex);
		while (!worker_done)
		{
			g_cond_wait(&cond, &mutex);
		}
		g_mutex_unlock(&mutex);
	}

	ApplicationsPage* page;
	ApplicationLoadGeneration generation;
	ApplicationCandidate* candidate;
	gint cancelled;
	GMutex mutex;
	GCond cond;
	bool worker_done;
};

//-----------------------------------------------------------------------------

ApplicationsPage::ApplicationsPage(Settings* settings, Window* window) :
	Page(settings, window, "applications-other", _("All Applications")),
	m_garcon_menu(nullptr),
	m_garcon_settings_menu(nullptr),
	m_load_job(nullptr),
	m_load_generation(0),
	m_has_publication(false),
	m_status(LoadStatus::Invalid)
{
	garcon_set_environment_xdg(GARCON_ENVIRONMENT_XFCE);

	m_outer = meow::meowmenu_create_default_heading_page(get_widget(),
			_("ALL APPLICATIONS"), &m_default_heading);
	g_object_ref_sink(m_outer);

	const decltype(m_categories.size()) index = 0;
	connect(get_button()->get_widget(), "toggled",
		[this](GtkToggleButton* button)
		{
			show_category(button, index);
		});
}

//-----------------------------------------------------------------------------

ApplicationsPage::~ApplicationsPage()
{
	cancel_pending_load();
	clear();

	// Detach the base view before tearing down the wrapper so ~Page can still
	// destroy m_widget itself (it holds its own ref); destroying m_outer here
	// would otherwise take the base view down with it.
	if (m_outer)
	{
		gtk_container_remove(GTK_CONTAINER(m_outer), get_widget());
		gtk_widget_destroy(m_outer);
		g_object_unref(m_outer);
	}
}

//-----------------------------------------------------------------------------

void ApplicationsPage::set_default_heading(bool visible)
{
	if (!m_default_heading)
		return;
	gtk_widget_set_visible(m_default_heading, visible);
}

//-----------------------------------------------------------------------------

GtkTreeModel* ApplicationsPage::create_launcher_model(
		const StringList& desktop_ids) const
{
	// Create new model for treeview
	GtkListStore* store = gtk_list_store_new(
			LauncherView::N_COLUMNS,
			G_TYPE_ICON,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);

	// Availability is a presentation concern. Unresolved identifiers remain in
	// Xfconf and are reconsidered after every successful application load.
	for (int i = 0; i < desktop_ids.size(); ++i)
	{
		const std::string& desktop_id = desktop_ids[i];
		if (desktop_id.empty())
		{
			continue;
		}

		Launcher* launcher = find(desktop_id);
		if (launcher)
		{
			gtk_list_store_insert_with_values(
					store, nullptr, G_MAXINT,
					LauncherView::COLUMN_ICON, launcher->get_icon(),
					LauncherView::COLUMN_TEXT, launcher->get_text(),
					LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
					LauncherView::COLUMN_LAUNCHER, launcher,
					-1);
		}
	}

	return GTK_TREE_MODEL(store);
}

//-----------------------------------------------------------------------------

Launcher* ApplicationsPage::find(const std::string& desktop_id) const
{
	auto i = m_items.find(desktop_id);
	return (i != m_items.end()) ? i->second : nullptr;
}

//-----------------------------------------------------------------------------

std::vector<Launcher*> ApplicationsPage::find_all() const
{
	std::vector<Launcher*> launchers;
	launchers.reserve(m_items.size());
	for (const auto& i : m_items)
	{
		launchers.push_back(i.second);
	}
	std::sort(launchers.begin(), launchers.end(), &Element::less_than);
	return launchers;
}

//-----------------------------------------------------------------------------

void ApplicationsPage::show_category(GtkToggleButton* togglebutton, std::vector<Category*>::size_type index)
{
	// Only apply filter for active button and valid category
	if (!gtk_toggle_button_get_active(togglebutton) || m_categories.empty())
	{
		return;
	}

	// Apply filter
	Category* category = m_categories[index];
	get_view()->unset_model();
	get_view()->set_fixed_height_mode(!category->has_separators());
	get_view()->set_model(category->get_model());
}

//-----------------------------------------------------------------------------

void ApplicationsPage::invalidate()
{
	if (m_status == LoadStatus::Done)
	{
		m_status = LoadStatus::Invalid;
	}
	else if (m_status == LoadStatus::Loading)
	{
		m_status = LoadStatus::ReloadRequired;
		if (m_load_job)
		{
			m_load_job->generation.invalidate();
		}
	}
}

//-----------------------------------------------------------------------------

bool ApplicationsPage::load()
{
	// Check if already loaded
	if (m_status == LoadStatus::Done)
	{
		return true;
	}
	// Check if currently loading
	else if (m_status != LoadStatus::Invalid)
	{
		return false;
	}
	m_status = LoadStatus::Loading;

	// A candidate owns every replacement object. The current publication stays
	// attached while discovery runs, so reload never exposes a partial graph.
	LoadJob* job = new LoadJob(this, ++m_load_generation);
	m_load_job = job;
	GTask* task = g_task_new(nullptr, nullptr,
		+[](GObject*, GAsyncResult*, gpointer user_data)
		{
			LoadJob* load_job = static_cast<LoadJob*>(user_data);
			ApplicationsPage* page = load_job->page;
			if (page && page->m_load_job == load_job
					&& !load_job->is_cancelled()
					&& page->m_load_generation == load_job->generation.id())
			{
				page->m_load_job = nullptr;
				page->populate_candidate(*load_job->candidate);
				const bool coherent = load_job->candidate->source_loaded
						|| !page->m_has_publication;
				const bool ready = load_job->generation.candidate_ready(coherent);
				if (ready && load_job->generation.commit(
						page->m_load_generation, true))
				{
					page->publish_candidate(*load_job->candidate);
				}
				else
				{
					page->m_status = LoadStatus::Invalid;
				}

				// A reload-required signal during discovery invalidates the
				// candidate and immediately queues the newer generation.
				if (load_job->generation.follow_up_required())
				{
					page->m_status = LoadStatus::Invalid;
					page->load();
				}
			}
			delete load_job;
		},
		job);
	g_task_set_task_data(task, job, nullptr);
	g_task_run_in_thread(task,
		+[](GTask* thread_task, gpointer, gpointer task_data, GCancellable*)
		{
			LoadJob* load_job = static_cast<LoadJob*>(task_data);
			if (!load_job->is_cancelled() && load_job->page)
			{
				// Garcon file discovery/loading is worker-safe. Signal wiring,
				// Launcher construction, and UI publication remain in the main
				// context completion callback.
				load_job->page->load_garcon_menus(*load_job->candidate);
			}
			load_job->mark_worker_done();
			g_task_return_boolean(thread_task, true);
		});
	g_object_unref(task);

	return false;
}

//-----------------------------------------------------------------------------

void ApplicationsPage::reload_category_icon_size()
{
	for (auto category : m_categories)
	{
		category->get_button()->reload_icon_size();
	}
}

//-----------------------------------------------------------------------------

/* ApplicationsPage::cancel_pending_load:
 *
 * Invalidates the active async application-load generation and waits until its
 * worker no longer touches this page before teardown continues. The completion
 * callback owns the LoadJob memory and will discard cancelled/stale jobs.
 */
void ApplicationsPage::cancel_pending_load()
{
	LoadJob* job = m_load_job;
	if (!job)
	{
		return;
	}

	job->cancel();
	job->wait_for_worker();
	if (m_load_job == job)
	{
		m_load_job = nullptr;
	}
	if (job->page == this)
	{
		job->page = nullptr;
	}
	if (m_status == LoadStatus::Loading || m_status == LoadStatus::ReloadRequired)
	{
		m_status = LoadStatus::Invalid;
	}
}

//-----------------------------------------------------------------------------

void ApplicationsPage::clear()
{
	cancel_pending_load();

	// End Window's borrow epoch before the owned Category/CategoryButton graph is
	// deleted. After this point dynamic category widgets are no longer reachable
	// through layout, mode-switch, focus, or width-measurement code.
	get_window()->detach_categories();

	// Free categories
	for (auto category : m_categories)
	{
		delete category;
	}
	m_categories.clear();

	// Free menu items
	get_window()->unset_items();
	get_view()->unset_model();

	for (const auto& i : m_items)
	{
		delete i.second;
	}
	m_items.clear();

	// Free menu
	if (G_LIKELY(m_garcon_menu))
	{
		g_object_unref(m_garcon_menu);
		m_garcon_menu = nullptr;
	}

	// Free settings menu
	if (G_LIKELY(m_garcon_settings_menu))
	{
		g_object_unref(m_garcon_settings_menu);
		m_garcon_settings_menu = nullptr;
	}
	m_has_publication = false;
}

//-----------------------------------------------------------------------------

/* ApplicationsPage::load_garcon_menus:
 *
 * Performs only Garcon menu creation and file loading on the worker thread.
 * The owner waits for this state transition during teardown, and the guarded
 * main-context completion owns all signal wiring and object publication.
 */
void ApplicationsPage::load_garcon_menus(ApplicationCandidate& candidate)
{
	// Create menu
	if (m_settings->custom_menu_file.empty())
	{
		candidate.menu = garcon_menu_new_applications();
	}
	else
	{
		candidate.menu = garcon_menu_new_for_path(m_settings->custom_menu_file);
	}

	// Load menu
	if (candidate.menu && !garcon_menu_load(candidate.menu, nullptr, nullptr))
	{
		g_object_unref(candidate.menu);
		candidate.menu = nullptr;
	}

	if (!candidate.menu)
	{
		return;
	}
	candidate.source_loaded = true;

	// Create settings menu
	gchar* path = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, "menus/xfce-settings-manager.menu");
	candidate.settings_menu = garcon_menu_new_for_path(
			path ? path : SETTINGS_MENUFILE);
	g_free(path);

	// Load settings menu
	if (candidate.settings_menu
			&& !garcon_menu_load(candidate.settings_menu, nullptr, nullptr))
	{
		g_object_unref(candidate.settings_menu);
		candidate.settings_menu = nullptr;
	}
}

//-----------------------------------------------------------------------------

/* ApplicationsPage::populate_candidate:
 * @candidate: isolated load-owned replacement graph.
 *
 * Connects Garcon change signals and constructs the Launcher/category object
 * graph on the main context. Published members remain untouched until the
 * complete candidate reaches publish_candidate().
 */
void ApplicationsPage::populate_candidate(ApplicationCandidate& candidate)
{
	if (candidate.menu)
	{
		connect(candidate.menu, "reload-required",
			[this](GarconMenu*)
			{
				invalidate();
			});

		load_menu(candidate, candidate.menu, nullptr,
				m_settings->view_mode == Settings::ViewAsTree);

		if (candidate.settings_menu)
		{
			connect(candidate.settings_menu, "reload-required",
				[this](GarconMenu*)
				{
					invalidate();
				});

			Category* category = new Category(m_settings, nullptr);
			load_menu(candidate, candidate.settings_menu, category, false);
			delete category;
		}
	}

	// Sort items and categories
	if (m_settings->view_mode != Settings::ViewAsTree)
	{
		for (auto category : candidate.categories)
		{
			category->sort();
		}
	}
	if (m_settings->sort_categories)
	{
		std::sort(candidate.categories.begin(), candidate.categories.end(),
				&Element::less_than);
	}

	// A failed or empty Garcon discovery still produces one coherent empty All
	// Applications model; cold start never publishes a half-built navigation.
	Category* all = new Category(m_settings, nullptr);
	all->append_items(find_all(candidate));
	candidate.categories.insert(candidate.categories.begin(), all);
}

//-----------------------------------------------------------------------------

std::vector<Launcher*> ApplicationsPage::find_all(
		const ApplicationCandidate& candidate) const
{
	std::vector<Launcher*> launchers;
	launchers.reserve(candidate.items.size());
	for (const auto& item : candidate.items)
	{
		launchers.push_back(item.second);
	}
	std::sort(launchers.begin(), launchers.end(), &Element::less_than);
	return launchers;
}

//-----------------------------------------------------------------------------

/* ApplicationsPage::publish_candidate:
 * @candidate: complete candidate whose generation was accepted for commit.
 *
 * Detaches every model that borrows launchers, swaps graph ownership, and
 * reattaches navigation/models before returning to the GTK main loop. The
 * retired graph remains owned by @candidate until all borrowers are detached.
 */
void ApplicationsPage::publish_candidate(ApplicationCandidate& candidate)
{
	g_assert(!candidate.categories.empty());

	get_window()->unset_items();
	get_view()->unset_model();
	get_window()->detach_categories();

	std::swap(m_garcon_menu, candidate.menu);
	std::swap(m_garcon_settings_menu, candidate.settings_menu);
	m_categories.swap(candidate.categories);
	m_items.swap(candidate.items);

	m_categories.front()->set_button(get_button());

	// Set all applications category
	get_view()->set_fixed_height_mode(true);
	get_view()->set_model(m_categories.front()->get_model());

	// Add buttons for categories
	std::vector<CategoryButton*> category_buttons;
	const auto size = m_categories.size();
	for (decltype(m_categories.size()) i = 1; i < size; ++i)
	{
		CategoryButton* category_button = m_categories[i]->get_button();
		connect(category_button->get_widget(), "toggled",
			[this, i](GtkToggleButton* button)
			{
				show_category(button, i);
			});
		category_buttons.push_back(category_button);
	}

	// Attach every model before category activation can expose its target.
	get_window()->set_items();

	// Adding categories resolves and activates the publication target.
	get_window()->set_categories(category_buttons);
	get_window()->set_loaded();

	m_has_publication = true;
	m_status = LoadStatus::Done;
}

//-----------------------------------------------------------------------------

bool ApplicationsPage::load_menu(ApplicationCandidate& candidate,
		GarconMenu* menu, Category* parent_category, bool load_hierarchy)
{
	bool has_children = false;

	// Add menu elements
	GList* elements = garcon_menu_get_elements(menu);
	for (GList* li = elements; li; li = li->next)
	{
		// Add menu item
		if (GARCON_IS_MENU_ITEM(li->data))
		{
			GarconMenuItem* menuitem = GARCON_MENU_ITEM(li->data);

			// Listen for changes
			connect(menuitem, "changed",
				[this](GarconMenuItem*)
				{
					invalidate();
				});

			// Skip hidden items
			if (!garcon_menu_element_get_visible(GARCON_MENU_ELEMENT(menuitem)))
			{
				continue;
			}

			// Create launcher
			std::string desktop_id(garcon_menu_item_get_desktop_id(menuitem));
			auto iter = candidate.items.find(desktop_id);
			if (iter == candidate.items.end())
			{
				iter = candidate.items.emplace(std::move(desktop_id),
						new Launcher(m_settings, menuitem)).first;
			}

			// Add launcher to current category
			if (parent_category)
			{
				parent_category->append_item(iter->second);
			}

			has_children = true;
		}
		// Add separator
		else if (GARCON_IS_MENU_SEPARATOR(li->data) && load_hierarchy && parent_category)
		{
			parent_category->append_separator();
		}
		// Add submenu
		else if (GARCON_IS_MENU(li->data))
		{
			GarconMenu* submenu = GARCON_MENU(li->data);

			// Skip hidden categories
			GarconMenuDirectory* directory = garcon_menu_get_directory(submenu);
			if (directory && !garcon_menu_directory_get_visible(directory))
			{
				continue;
			}

			// Create category
			Category* category = nullptr;
			if (!load_hierarchy && parent_category)
			{
				category = parent_category;
			}
			else
			{
				category = new Category(m_settings, submenu);
			}

			// Populate category
			if (load_menu(candidate, submenu, category, load_hierarchy))
			{
				if (!parent_category)
				{
					candidate.categories.push_back(category);
				}
				else if (category != parent_category)
				{
					parent_category->append_category(category);
				}

				has_children = true;
			}
			// Remove empty categories
			else if (category != parent_category)
			{
				delete category;
			}
		}
	}
	g_list_free(elements);

	return has_children;
}

//-----------------------------------------------------------------------------
