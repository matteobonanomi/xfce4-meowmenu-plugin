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

#ifndef WHISKERMENU_FAVORITES_PAGE_H
#define WHISKERMENU_FAVORITES_PAGE_H

#include "page.h"

#include <functional>
#include <string>
#include <vector>

namespace WhiskerMenu
{

class FavoritesPage : public Page
{
public:
	FavoritesPage(Settings* settings, Window* window);
	~FavoritesPage();

	bool contains(Launcher* launcher) const;

	void add(Launcher* launcher);
	void remove(Launcher* launcher);
	void set_menu_items();
	void unset_menu_items();
	void move_up(Launcher* launcher);
	void move_down(Launcher* launcher);

	/* set_item_inserted_callback:
	 * @callback: observer owned by the Window that owns this page.
	 *
	 * Stores the page-lifetime insertion observer so replacing the backing model
	 * cannot detach Window behavior. The callback runs synchronously after a row
	 * is inserted and must not outlive the page owner.
	 */
	void set_item_inserted_callback(const std::function<void()>& callback)
	{
		m_item_inserted_callback = callback;
	}

private:
	void extend_context_menu(GtkWidget* menu) override;
	bool remember_launcher(Launcher* launcher) override;
	void on_rows_reordered(GtkTreeModel* model);
	std::vector<std::string> stored_ids() const;
	std::vector<std::string> available_ids() const;
	std::vector<std::string> visible_ids(GtkTreeModel* model) const;
	void apply_resolved_order(const std::vector<std::string>& order);
	std::vector<Launcher*> sort() const;
	void sort_ascending();
	void sort_descending();
	void view_created() override;

	std::function<void()> m_item_inserted_callback;
};

}

#endif // WHISKERMENU_FAVORITES_PAGE_H
