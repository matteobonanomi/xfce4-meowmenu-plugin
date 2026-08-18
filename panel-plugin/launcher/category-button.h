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

#ifndef WHISKERMENU_CATEGORY_BUTTON_H
#define WHISKERMENU_CATEGORY_BUTTON_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class Settings;

class CategoryButton
{
public:
	CategoryButton(Settings* settings, GIcon* icon, const gchar* text);
	~CategoryButton();

	CategoryButton(const CategoryButton&) = delete;
	CategoryButton(CategoryButton&&) = delete;
	CategoryButton& operator=(const CategoryButton&) = delete;
	CategoryButton& operator=(CategoryButton&&) = delete;

	GtkWidget* get_widget() const
	{
		return GTK_WIDGET(m_button);
	}

	bool get_active() const
	{
		return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_button));
	}

	void set_active(bool active)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button), active);
	}

	void join_group(CategoryButton* button)
	{
		gtk_radio_button_join_group(m_button, button->m_button);
	}

	void reload_icon_size();

	/* reload_icon_size:
	 * @render_size: icon artwork size in logical pixels; 1 hides the icon.
	 * @slot_size: aligned icon allocation in logical pixels.
	 *
	 * Applies an optical artwork size without moving the shared sidebar label
	 * column. The no-argument overload uses the configured category size for
	 * both values.
	 */
	void reload_icon_size(int render_size, int slot_size);

	/* measure_label_width:
	 *
	 * Measure the label's natural width in pixels in its current font, capped to
	 * MEOW_SIDEBAR_LABEL_MAX_CHARS characters so an over-long label contributes
	 * only the cap (it ellipsises on screen). The window takes the maximum across
	 * every sidebar button to size the shared width floor. Independent of widget
	 * visibility, so a hidden off-mode button still reports its true width.
	 *
	 * Returns: the (capped) natural label width in pixels.
	 */
	int measure_label_width() const;

	/* set_min_label_width:
	 * @width: the shared minimum label width in pixels.
	 *
	 * Pin the label's minimum width so this button never requests less than the
	 * widest sidebar label across both modes. See the implementation for why the
	 * floor lives on the buttons rather than on the width size group.
	 */
	void set_min_label_width(int width);

	/* suppress_hover_until_motion:
	 *
	 * Inhibit pointer-hover auto-activation for every category button until the
	 * next genuine pointer motion. Called by the window keyboard handler when a
	 * keyboard-driven category navigation occurs so a stationary pointer resting
	 * over the sidebar cannot re-activate a hovered button and steal focus back.
	 * Process-wide (one shared latch across all buttons); re-armed on the first
	 * motion-notify over any category button.
	 */
	static void suppress_hover_until_motion();

private:
	Settings* const m_settings;
	GtkRadioButton* m_button;
	GtkBox* m_box;
	GtkWidget* m_icon;
	GtkWidget* m_label;
	long m_label_chars;
};

}

#endif // WHISKERMENU_CATEGORY_BUTTON_H
