/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_CALCULATOR_RESULT_H
#define WHISKERMENU_CALCULATOR_RESULT_H

#include <gtk/gtk.h>

#include <functional>
#include <string>

namespace WhiskerMenu
{

enum class CalculatorResultState
{
	Hidden,
	Pending,
	Success,
	MissingBc
};

/* calculator_auto_font_size:
 * @preset_id: active built-in, saved-custom, empty, or unknown preset id.
 *
 * Resolves Auto Calculator typography without changing its stored -1 setting.
 * Custom and unknown identities deliberately use the conservative Normal size.
 *
 * Returns: semantic font-size index in the inclusive 0..6 range.
 */
int calculator_auto_font_size(const char* preset_id);

/* calculator_result_height:
 * @item_height: ordinary list or tree row height in logical pixels.
 * @is_grid: true when the surrounding launcher view is an icon grid.
 *
 * Keeps the external Calculator banner synchronized with the active result
 * presentation. List mode supplies one list-row height; grid mode supplies one
 * grid-row height.
 *
 * Returns: the fixed Calculator banner height in logical pixels.
 */
int calculator_result_height(int item_height, bool is_grid);

class CalculatorResult
{
public:
	typedef std::function<void()> ActivateCallback;

	CalculatorResult();
	~CalculatorResult();
	GtkWidget* get_widget() const { return m_widget; }
	GtkWidget* get_focus_widget() const { return m_row; }
	void clear();
	void set_pending();
	void set_result(const char* engine, const char* icon_name,
			const char* fallback_icon_name, const std::string& value,
			int font_size);
	void set_missing_bc();
	void set_presentation_metrics(int item_height, int icon_size, bool is_grid,
			int auto_font_size = 3);
	void set_activate_callback(ActivateCallback callback);
	bool activate();
	bool is_visible() const;
	bool is_activatable() const { return m_state == CalculatorResultState::Success; }
	bool suppresses_fallbacks() const { return m_state == CalculatorResultState::Success; }
	CalculatorResultState state() const { return m_state; }
	const std::string& value() const { return m_value; }

private:
	void update_font();
	void update_icon();
	void update_vertical_margins();

	GtkWidget* m_widget;
	GtkWidget* m_row;
	GtkWidget* m_clip;
	GtkWidget* m_content;
	GtkWidget* m_icon;
	GtkWidget* m_engine;
	GtkWidget* m_value_allocation;
	GtkWidget* m_value_label;
	GIcon* m_icon_gicon;
	CalculatorResultState m_state;
	ActivateCallback m_activate_callback;
	std::string m_value;
	int m_font_size;
	int m_item_height;
	int m_icon_size;
	int m_auto_font_size;
	bool m_is_grid;
};

}

#endif // WHISKERMENU_CALCULATOR_RESULT_H
