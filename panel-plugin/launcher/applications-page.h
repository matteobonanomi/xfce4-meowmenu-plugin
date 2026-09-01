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

#ifndef WHISKERMENU_APPLICATIONS_PAGE_H
#define WHISKERMENU_APPLICATIONS_PAGE_H

#include "page.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <garcon/garcon.h>

namespace WhiskerMenu
{

class Category;
class StringList;

class ApplicationsPage : public Page
{

public:
	ApplicationsPage(Settings* settings, Window* window);
	~ApplicationsPage();

	GtkTreeModel* create_launcher_model(const StringList& desktop_ids) const;
	Launcher* find(const std::string& desktop_id) const;
	std::vector<Launcher*> find_all() const;

	/* get_outer_widget:
	 *
	 * Returns the page container including the default-category heading. Use
	 * this (not get_widget()) when packing the page into the results stack so
	 * the heading sits above the launcher view. Borrowed; owned by the page.
	 */
	GtkWidget* get_outer_widget() const { return m_outer; }

	/* set_default_heading:
	 * @visible: TRUE to show the heading when the sidebar is disabled.
	 *
	 * Shows or hides the All Applications page heading. Other built-in pages own
	 * their corresponding heading wrappers at the Window composition boundary.
	 */
	void set_default_heading(bool visible);

	void invalidate();
	bool load();
	bool has_publication() const { return m_has_publication; }
	void reload_category_icon_size();

private:
	struct ApplicationCandidate;
	struct LoadJob;

	void show_category(GtkToggleButton* togglebutton, std::vector<Category*>::size_type index);
	void cancel_pending_load();
	void clear();
	void load_garcon_menus(ApplicationCandidate& candidate);
	void populate_candidate(ApplicationCandidate& candidate);
	void publish_candidate(ApplicationCandidate& candidate);
	std::vector<Launcher*> find_all(const ApplicationCandidate& candidate) const;
	bool load_menu(ApplicationCandidate& candidate, GarconMenu* menu,
			Category* parent_category, bool load_hierarchy);

private:
	// Outer container = [page heading, base launcher view]. The heading is hidden
	// unless the sidebar is disabled.
	GtkWidget* m_outer;
	GtkWidget* m_default_heading;

	GarconMenu* m_garcon_menu;
	GarconMenu* m_garcon_settings_menu;
	std::vector<Category*> m_categories;
	std::unordered_map<std::string, Launcher*> m_items;
	LoadJob* m_load_job;
	guint64 m_load_generation;
	bool m_has_publication;

	enum class LoadStatus
	{
		Invalid,
		Loading,
		ReloadRequired,
		Done
	}
	m_status;
};

}

#endif // WHISKERMENU_APPLICATIONS_PAGE_H
