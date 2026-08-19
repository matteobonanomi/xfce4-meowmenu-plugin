/*
 * Copyright (C) 2019 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_LAUNCHER_ICON_VIEW_H
#define WHISKERMENU_LAUNCHER_ICON_VIEW_H

#include "launcher-view.h"

#include <string>

namespace WhiskerMenu
{

class Settings;

void launcher_icon_view_set_transparent_grid_style(GtkWidget* view,
		bool enabled);
bool launcher_icon_view_complete_empty_click(GtkIconView* view,
		bool transparent_grid, const GdkEventButton* event);
GtkTreePath* launcher_icon_view_find_directional_path(GtkIconView* view,
		GtkCellRenderer* renderer, GtkTreePath* origin,
		Keyboard::PhysicalDirection direction, bool rtl);
bool launcher_icon_view_get_path_rectangle(GtkIconView* view,
		GtkCellRenderer* renderer, GtkTreePath* path,
		Keyboard::NavigationRect* rectangle);
bool launcher_icon_view_apply_keyboard_target(GtkIconView* view,
		GtkTreeModel* model, GtkTreePath* path);
void launcher_icon_view_apply_grid_width(GtkIconView* view, int icon_size,
		int viewport_width);

class LauncherIconView : public LauncherView
{
public:
	explicit LauncherIconView(Settings* settings);
	~LauncherIconView();

	GtkWidget* get_widget() const override
	{
		return GTK_WIDGET(m_view);
	}

	GtkTreePath* get_cursor() const override;
	GtkTreePath* get_path_at_pos(int x, int y) const override;
	GtkTreePath* get_selected_path() const override;
	bool is_path_selected(GtkTreePath* path) const override;
	void activate_path(GtkTreePath* path) override;
	void scroll_to_path(GtkTreePath* path) override;
	void select_path(GtkTreePath* path) override;
	void set_cursor(GtkTreePath* path) override;
	bool is_first_visual_row(GtkTreePath* path) const override;
	GtkTreePath* get_directional_path(GtkTreePath* origin,
			Keyboard::PhysicalDirection direction) const override;
	bool get_path_rectangle(GtkTreePath* path,
			Keyboard::NavigationRect* rectangle) const override;
	bool apply_keyboard_target(GtkTreePath* path) override;

	void set_fixed_height_mode(bool fixed_height) override;
	void set_selection_mode(GtkSelectionMode mode) override;

	void hide_tooltips() override;
	void show_tooltips() override;

	void clear_selection() override;
	void collapse_all() override;

	void set_model(GtkTreeModel* model) override;
	void unset_model() override;

	void set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions) override;
	void set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions) override;
	void unset_drag_source() override;
	void unset_drag_dest() override;

	void reload_icon_size() override;
	void set_viewport_width(int viewport_width) override;
	void set_interactive_resize(bool active) override;
	int get_minimum_viewport_width() const override;
	int get_item_height() const override;
	int get_icon_size() const override { return m_icon_size; }
	bool is_grid_view() const override { return true; }

private:
	void sync_transparent_grid_style();
	void schedule_grid_width_frame();
	void flush_grid_width_frame();

private:
	Settings* const m_settings;
	GtkIconView* m_view;
	GtkCellRenderer* m_icon_renderer;
	int m_icon_size;
	int m_viewport_width;
	int m_pending_viewport_width;
	guint m_resize_tick_id;
	bool m_interactive_resize;
	std::string m_grid_density;
	std::string m_layout_mode;
	bool m_transparent_grid;
};

}

#endif // WHISKERMENU_LAUNCHER_ICON_VIEW_H
