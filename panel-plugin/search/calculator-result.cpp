/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "calculator-result.h"

#include <atk/atk.h>
#include <glib/gi18n-lib.h>
#include <pango/pango.h>

#include <algorithm>

using namespace WhiskerMenu;

namespace
{

const double kFontScales[] = {
	PANGO_SCALE_XX_SMALL,
	PANGO_SCALE_X_SMALL,
	PANGO_SCALE_SMALL,
	PANGO_SCALE_MEDIUM,
	PANGO_SCALE_LARGE,
	PANGO_SCALE_X_LARGE,
	PANGO_SCALE_XX_LARGE
};

}

/* calculator_auto_font_size:
 * @preset_id: active built-in, saved-custom, empty, or unknown preset id.
 *
 * Maps the active preset identity to the existing semantic size ladder. This
 * decision is presentation-only, so it never writes the configured Auto key.
 *
 * Returns: Normal (3), Large (4), or Larger (5).
 */
int WhiskerMenu::calculator_auto_font_size(const char* preset_id)
{
	if (g_strcmp0(preset_id, "fullscreen") == 0)
		return 5;
	if (g_strcmp0(preset_id, "modern") == 0
			|| g_strcmp0(preset_id, "classic") == 0)
		return 4;
	return 3;
}

/* calculator_result_height:
 * @item_height: ordinary list or tree row height in logical pixels.
 * @is_grid: true when the surrounding launcher view is an icon grid.
 *
 * Preserves the result view's authoritative row height in both list and grid
 * modes. The Calculator widget clips its contents separately, so typography
 * cannot increase this value.
 *
 * Returns: a positive fixed banner height in logical pixels.
 */
int WhiskerMenu::calculator_result_height(int item_height, bool is_grid)
{
	(void)is_grid;
	return std::max(1, item_height);
}

CalculatorResult::CalculatorResult() :
	m_widget(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0)),
	m_row(gtk_event_box_new()),
	m_clip(gtk_scrolled_window_new(nullptr, nullptr)),
	m_content(nullptr),
	m_icon(gtk_image_new()),
	m_engine(gtk_label_new(nullptr)),
	m_value_allocation(gtk_overlay_new()),
	m_value_label(gtk_label_new(nullptr)),
	m_icon_gicon(nullptr),
	m_state(CalculatorResultState::Hidden),
	m_font_size(-1),
	m_item_height(-1),
	m_icon_size(-1),
	m_auto_font_size(3),
	m_is_grid(false)
{
	gtk_style_context_add_class(gtk_widget_get_style_context(m_widget),
			"calculator-result");
	gtk_container_add(GTK_CONTAINER(m_widget), m_row);
	gtk_widget_set_can_focus(m_row, TRUE);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(m_clip), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_clip),
			GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_propagate_natural_height(
			GTK_SCROLLED_WINDOW(m_clip), FALSE);
	gtk_container_add(GTK_CONTAINER(m_row), m_clip);

	m_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(m_content, 8);
	gtk_widget_set_margin_end(m_content, 8);
	gtk_widget_set_valign(m_content, GTK_ALIGN_CENTER);
	gtk_container_add(GTK_CONTAINER(m_clip), m_content);
	gtk_box_pack_start(GTK_BOX(m_content), m_icon, false, false, 0);
	gtk_widget_set_halign(m_engine, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(m_content), m_engine, false, false, 0);

	gtk_label_set_single_line_mode(GTK_LABEL(m_value_label), TRUE);
	gtk_label_set_lines(GTK_LABEL(m_value_label), 1);
	gtk_label_set_ellipsize(GTK_LABEL(m_value_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_halign(m_value_label, GTK_ALIGN_END);
	gtk_widget_set_valign(m_value_label, GTK_ALIGN_CENTER);
	gtk_label_set_xalign(GTK_LABEL(m_value_label), 1.0f);
	GtkWidget* value_sizer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_size_request(value_sizer, 1, 1);
	gtk_container_add(GTK_CONTAINER(m_value_allocation), value_sizer);
	gtk_overlay_add_overlay(GTK_OVERLAY(m_value_allocation), m_value_label);
	// The label is deliberately an allocation-only child: its semantic Auto
	// scale must clip inside the launcher row instead of increasing its request.
	gtk_widget_set_size_request(m_value_label, 1, 1);
	gtk_widget_set_hexpand(m_value_allocation, TRUE);
	gtk_widget_set_hexpand(m_value_label, TRUE);
	gtk_widget_set_vexpand(m_value_label, TRUE);
	gtk_box_pack_end(GTK_BOX(m_content), m_value_allocation, true, true, 0);

	g_signal_connect(m_row, "button-release-event",
			G_CALLBACK(+[](GtkWidget*, GdkEventButton* event, gpointer data) -> gboolean
			{
				if (event->button == GDK_BUTTON_PRIMARY)
					static_cast<CalculatorResult*>(data)->activate();
				return GDK_EVENT_PROPAGATE;
			}), this);
	clear();
}

CalculatorResult::~CalculatorResult()
{
	g_signal_handlers_disconnect_by_data(m_widget, this);
	g_signal_handlers_disconnect_by_data(m_row, this);
	g_signal_handlers_disconnect_by_data(m_value_label, this);
	g_clear_object(&m_icon_gicon);
}

void CalculatorResult::clear()
{
	m_state = CalculatorResultState::Hidden;
	m_value.clear();
	gtk_widget_set_tooltip_text(m_widget, nullptr);
	gtk_widget_hide(m_widget);
}

void CalculatorResult::set_pending()
{
	m_state = CalculatorResultState::Pending;
	m_value.clear();
	gtk_widget_hide(m_widget);
}

/* set_result:
 * @engine: localized visible engine name.
 * @icon_name: preferred themed icon name.
 * @fallback_icon_name: calculator fallback icon name.
 * @value: complete normalized value shared with tooltip and clipboard.
	 * @font_size: -1 for preset-derived Auto or 0..6 for semantic sizes.
 *
 * Commits one successful, focusable banner without changing the ordinary
 * launcher model. Ellipsization affects only the visible label.
 */
void CalculatorResult::set_result(const char* engine, const char* icon_name,
		const char* fallback_icon_name, const std::string& value, int font_size)
{
	m_state = CalculatorResultState::Success;
	m_value = value;
	m_font_size = CLAMP(font_size, -1, 6);
	gchar* icons[2] = {
		const_cast<gchar*>(icon_name ? icon_name : fallback_icon_name),
		const_cast<gchar*>(fallback_icon_name)
	};
	GIcon* icon = g_themed_icon_new_from_names(icons,
			icon_name && g_strcmp0(icon_name, fallback_icon_name) != 0 ? 2 : 1);
	g_clear_object(&m_icon_gicon);
	m_icon_gicon = icon;
	update_icon();
	gtk_label_set_text(GTK_LABEL(m_engine), engine);
	gtk_label_set_text(GTK_LABEL(m_value_label), value.c_str());
	gtk_widget_set_tooltip_text(m_widget, value.c_str());

	std::string accessible = std::string(engine) + ": " + value;
	AtkObject* object = gtk_widget_get_accessible(m_row);
	atk_object_set_name(object, accessible.c_str());
	atk_object_set_description(object, value.c_str());
	update_font();
	gtk_widget_show_all(m_widget);
}

void CalculatorResult::set_missing_bc()
{
	m_state = CalculatorResultState::MissingBc;
	m_value.clear();
	m_font_size = -1;
	g_clear_object(&m_icon_gicon);
	m_icon_gicon = G_ICON(g_themed_icon_new("accessories-calculator"));
	update_icon();
	gtk_label_set_text(GTK_LABEL(m_engine), "bc");
	gtk_label_set_text(GTK_LABEL(m_value_label), _("bc package required"));
	gtk_widget_set_tooltip_text(m_widget, _("bc package required"));
	AtkObject* object = gtk_widget_get_accessible(m_row);
	atk_object_set_name(object, _("bc: bc package required"));
	atk_object_set_description(object, _("bc package required"));
	update_font();
	gtk_widget_show_all(m_widget);
}

//-----------------------------------------------------------------------------

/* set_presentation_metrics:
 * @item_height: height in pixels of one ordinary launcher result or grid tile.
 * @icon_size: active launcher item icon size in pixels; non-positive hides it.
 * @is_grid: whether the active launcher presentation is a grid.
 *
 * Applies the launcher-owned visual metrics to the external full-width banner.
 * The banner stays outside GtkIconView so the same fixed-height row naturally
 * spans every current grid column without a synthetic launcher-model item.
 */
void CalculatorResult::set_presentation_metrics(int item_height, int icon_size,
		bool is_grid, int auto_font_size)
{
	m_item_height = calculator_result_height(item_height, is_grid);
	m_icon_size = icon_size;
	m_is_grid = is_grid;
	m_auto_font_size = CLAMP(auto_font_size, 0, 6);
	update_icon();
	update_font();
	update_vertical_margins();
	// GtkScrolledWindow does not propagate the child's natural height. Its
	// content-height floor therefore becomes the banner's exact row request.
	gtk_scrolled_window_set_min_content_height(
			GTK_SCROLLED_WINDOW(m_clip), m_item_height);
	gtk_widget_set_size_request(m_content, -1, m_item_height);
	gtk_widget_set_size_request(m_clip, -1, m_item_height);
	gtk_widget_set_size_request(m_widget, -1, m_item_height);
	gtk_widget_set_size_request(m_row, -1, m_item_height);
}

//-----------------------------------------------------------------------------

/* update_icon:
 *
 * Renders the stored themed icon at the launcher item's exact pixel size. GTK's
 * GIcon image API ignores pixel-size overrides, so this resolves a pixbuf when
 * a metric is available and otherwise keeps the normal themed fallback.
 */
void CalculatorResult::update_icon()
{
	if (!m_icon_gicon)
		return;
	if (m_icon_size <= 1)
	{
		gtk_image_set_from_gicon(GTK_IMAGE(m_icon), m_icon_gicon, GTK_ICON_SIZE_DND);
		return;
	}
	GtkIconTheme* theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(m_icon));
	GtkIconInfo* info = gtk_icon_theme_lookup_by_gicon(theme, m_icon_gicon,
			m_icon_size, GTK_ICON_LOOKUP_FORCE_SIZE);
	GdkPixbuf* pixbuf = info ? gtk_icon_info_load_icon(info, nullptr) : nullptr;
	if (pixbuf)
	{
		gtk_image_set_from_pixbuf(GTK_IMAGE(m_icon), pixbuf);
		g_object_unref(pixbuf);
	}
	else
	{
		gtk_image_set_from_gicon(GTK_IMAGE(m_icon), m_icon_gicon, GTK_ICON_SIZE_DND);
	}
	if (info)
		g_object_unref(info);
}

//-----------------------------------------------------------------------------

/* update_vertical_margins:
 *
 * Clears legacy height margins. Vertical centring is handled by GTK alignment
 * so margins cannot inflate the launcher-owned row or tile height.
 */
void CalculatorResult::update_vertical_margins()
{
	gtk_widget_set_margin_top(m_content, 0);
	gtk_widget_set_margin_bottom(m_content, 0);
}

//-----------------------------------------------------------------------------

void CalculatorResult::set_activate_callback(ActivateCallback callback)
{
	m_activate_callback = std::move(callback);
}

//-----------------------------------------------------------------------------

bool CalculatorResult::activate()
{
	if (!is_activatable() || !m_activate_callback)
		return false;
	m_activate_callback();
	return true;
}

//-----------------------------------------------------------------------------

bool CalculatorResult::is_visible() const
{
	return gtk_widget_get_visible(m_widget);
}

/* update_font:
 *
 * Applies an explicit semantic Pango scale or the active preset's resolved
 * Auto scale. The overlay owns clipping, so long values never alter the
 * launcher-owned row height and remain available through the full-value paths.
 */
void CalculatorResult::update_font()
{
	if (m_state == CalculatorResultState::Hidden
			|| m_state == CalculatorResultState::Pending)
		return;
	if (m_state == CalculatorResultState::MissingBc)
	{
		gtk_label_set_attributes(GTK_LABEL(m_value_label), nullptr);
		gtk_label_set_attributes(GTK_LABEL(m_engine), nullptr);
		return;
	}
	const int selected = (m_font_size < 0) ? m_auto_font_size : m_font_size;
	PangoAttrList* attrs = pango_attr_list_new();
	pango_attr_list_insert(attrs, pango_attr_scale_new(kFontScales[selected]));
	gtk_label_set_attributes(GTK_LABEL(m_value_label), attrs);
	pango_attr_list_unref(attrs);

	PangoAttrList* engine_attrs = pango_attr_list_new();
	const double engine_scale = (m_font_size < 0)
			? kFontScales[selected] * 0.85 : kFontScales[selected];
	pango_attr_list_insert(engine_attrs, pango_attr_scale_new(engine_scale));
	gtk_label_set_attributes(GTK_LABEL(m_engine), engine_attrs);
	pango_attr_list_unref(engine_attrs);
	update_vertical_margins();
	if (m_item_height > 0)
	{
		gtk_widget_set_size_request(m_widget, -1, m_item_height);
		gtk_widget_set_size_request(m_row, -1, m_item_height);
	}
}
