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

#include "favorites-page.h"

#include "applications-page.h"
#include "favorite-projection.h"
#include "ui/image-menu-item.h"
#include "launcher.h"
#include "ui/launcher-view.h"
#include "settings.h"
#include "ui/slot.h"
#include "core/window.h"

#include <algorithm>

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

FavoritesPage::FavoritesPage(Settings* settings, Window* window) :
	Page(settings, window, "user-bookmarks", _("Favorites"))
{
	view_created();
}

//-----------------------------------------------------------------------------

FavoritesPage::~FavoritesPage()
{
	unset_menu_items();
}

//-----------------------------------------------------------------------------

bool FavoritesPage::contains(Launcher* launcher) const
{
	if (!launcher)
	{
		return false;
	}

	std::string desktop_id(launcher->get_desktop_id());
	return std::find(m_settings->favorites.begin(), m_settings->favorites.end(), desktop_id) != m_settings->favorites.end();
}

//-----------------------------------------------------------------------------

void FavoritesPage::add(Launcher* launcher)
{
	if (!launcher || contains(launcher))
	{
		return;
	}

	// Adding is an explicit user edit. Persist the exact identifier before
	// updating the visible projection; ordinary model construction never writes.
	m_settings->favorites.push_back(launcher->get_desktop_id());
	GtkListStore* store = GTK_LIST_STORE(get_view()->get_model());
	gtk_list_store_insert_with_values(
			store, nullptr, G_MAXINT,
			LauncherView::COLUMN_ICON, launcher->get_icon(),
			LauncherView::COLUMN_TEXT, launcher->get_text(),
			LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
			LauncherView::COLUMN_LAUNCHER, launcher,
			-1);
}

//-----------------------------------------------------------------------------

void FavoritesPage::remove(Launcher* launcher)
{
	if (!launcher)
	{
		return;
	}
	const int stored_index = m_settings->favorites.find(
			launcher->get_desktop_id());
	if (stored_index < 0)
	{
		return;
	}
	m_settings->favorites.erase(stored_index);

	GtkTreeModel* model = GTK_TREE_MODEL(get_view()->get_model());
	GtkListStore* store = GTK_LIST_STORE(model);
	GtkTreeIter iter;
	Launcher* test_launcher = nullptr;

	bool valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
	{
		gtk_tree_model_get(model, &iter, LauncherView::COLUMN_LAUNCHER, &test_launcher, -1);
		if (test_launcher == launcher)
		{
			gtk_list_store_remove(store, &iter);
			break;
		}
		valid = gtk_tree_model_iter_next(model, &iter);
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::set_menu_items()
{
	GtkTreeModel* model = get_window()->get_applications()->create_launcher_model(m_settings->favorites);
	get_view()->set_model(model);

	connect(model, "rows-reordered",
		[this](GtkTreeModel* tree_model, GtkTreePath*, GtkTreeIter*, gint*)
		{
			on_rows_reordered(tree_model);
		});

	g_object_unref(model);
}

//-----------------------------------------------------------------------------

void FavoritesPage::unset_menu_items()
{
	// Clear treeview
	get_view()->unset_model();
}

//-----------------------------------------------------------------------------

void FavoritesPage::extend_context_menu(GtkWidget* menu)
{
	GtkWidget* menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	Launcher* selected_launcher = get_selected_launcher();
	if (selected_launcher)
	{
		const std::vector<std::string> order = visible_ids(get_view()->get_model());
		const auto selected = std::find(order.begin(), order.end(),
				selected_launcher->get_desktop_id());
		const int selected_index = selected == order.end()
				? -1 : static_cast<int>(selected - order.begin());

		menuitem = whiskermenu_image_menu_item_new("go-up", _("Move Up"));
		connect(menuitem, "activate",
			[this, selected_launcher](GtkMenuItem*)
			{
				move_up(selected_launcher);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		if (selected_index <= 0)
		{
			gtk_widget_set_sensitive(menuitem, false);
		}

		menuitem = whiskermenu_image_menu_item_new("go-down", _("Move Down"));
		connect(menuitem, "activate",
			[this, selected_launcher](GtkMenuItem*)
			{
				move_down(selected_launcher);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		if (selected_index < 0
				|| selected_index == static_cast<int>(order.size()) - 1)
		{
			gtk_widget_set_sensitive(menuitem, false);
		}

		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	menuitem = whiskermenu_image_menu_item_new("view-sort-ascending", _("Sort Alphabetically A-Z"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			sort_ascending();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("view-sort-descending", _("Sort Alphabetically Z-A"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			sort_descending();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

//-----------------------------------------------------------------------------

bool FavoritesPage::remember_launcher(Launcher* launcher)
{
	return m_settings->favorites_in_recent ? true : !contains(launcher);
}

//-----------------------------------------------------------------------------

std::vector<std::string> FavoritesPage::stored_ids() const
{
	return std::vector<std::string>(m_settings->favorites.begin(),
			m_settings->favorites.end());
}

//-----------------------------------------------------------------------------

std::vector<std::string> FavoritesPage::available_ids() const

{
	std::vector<std::string> ids;
	const std::vector<Launcher*> launchers =
			get_window()->get_applications()->find_all();
	ids.reserve(launchers.size());
	for (Launcher* launcher : launchers)
	{
		ids.push_back(launcher->get_desktop_id());
	}
	return ids;
}

//-----------------------------------------------------------------------------

std::vector<std::string> FavoritesPage::visible_ids(GtkTreeModel* model) const
{
	std::vector<std::string> ids;
	GtkTreeIter iter;
	bool valid = model && gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
	{
		Launcher* launcher = nullptr;
		gtk_tree_model_get(model, &iter,
				LauncherView::COLUMN_LAUNCHER, &launcher, -1);
		if (launcher)
		{
			ids.push_back(launcher->get_desktop_id());
		}
		valid = gtk_tree_model_iter_next(model, &iter);
	}
	return ids;
}

//-----------------------------------------------------------------------------

/* FavoritesPage::apply_resolved_order:
 * @order: complete visible order after a user reorder or sort.
 *
 * Merges visible identifiers back into only the resolved durable slots so a
 * temporary application absence cannot move or erase an unresolved favourite.
 */
void FavoritesPage::apply_resolved_order(const std::vector<std::string>& order)
{
	const std::vector<std::string> original = stored_ids();
	const std::vector<std::string> merged = favorite_merge_resolved_order(
			original, order, available_ids());
	for (int i = 0; i < m_settings->favorites.size(); ++i)
	{
		if (m_settings->favorites[i] != merged[i])
		{
			m_settings->favorites.set(i, merged[i]);
		}
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_rows_reordered(GtkTreeModel* model)
{
	apply_resolved_order(visible_ids(model));
}

//-----------------------------------------------------------------------------

std::vector<Launcher*> FavoritesPage::sort() const
{
	std::vector<Launcher*> items;
	items.reserve(m_settings->favorites.size());
	for (const auto& favorite : m_settings->favorites)
	{
		Launcher* launcher = get_window()->get_applications()->find(favorite);
		if (!launcher)
		{
			continue;
		}
		items.push_back(launcher);
	}
	std::sort(items.begin(), items.end(), &Element::less_than);
	return items;
}

//-----------------------------------------------------------------------------

void FavoritesPage::sort_ascending()
{
	const auto items = sort();
	std::vector<std::string> order;
	order.reserve(items.size());
	for (auto launcher : items)
	{
		order.push_back(launcher->get_desktop_id());
	}
	apply_resolved_order(order);
	set_menu_items();
}

//-----------------------------------------------------------------------------

void FavoritesPage::sort_descending()
{
	const auto items = sort();
	std::vector<std::string> order;
	order.reserve(items.size());
	for (auto i = items.rbegin(), end = items.rend(); i != end; ++i)
	{
		order.push_back((*i)->get_desktop_id());
	}
	apply_resolved_order(order);
	set_menu_items();
}

//-----------------------------------------------------------------------------

void FavoritesPage::view_created()
{
	set_reorderable(true);
}

//-----------------------------------------------------------------------------

void FavoritesPage::move_up(Launcher* launcher)
{
	if (!launcher)
	{
		return;
	}

	std::vector<std::string> order = visible_ids(get_view()->get_model());
	const auto found = std::find(order.begin(), order.end(),
			launcher->get_desktop_id());
	if (found == order.end() || found == order.begin())
	{
		return;
	}
	std::iter_swap(found, found - 1);
	apply_resolved_order(order);

	set_menu_items();
}

//-----------------------------------------------------------------------------

void FavoritesPage::move_down(Launcher* launcher)
{
	if (!launcher)
	{
		return;
	}

	std::vector<std::string> order = visible_ids(get_view()->get_model());
	const auto found = std::find(order.begin(), order.end(),
			launcher->get_desktop_id());
	if (found == order.end() || found + 1 == order.end())
	{
		return;
	}
	std::iter_swap(found, found + 1);
	apply_resolved_order(order);

	set_menu_items();
}

//-----------------------------------------------------------------------------
