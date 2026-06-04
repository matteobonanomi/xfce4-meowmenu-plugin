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

#include "category-button.h"

#include "settings.h"
#include "ui/slot.h"

#include <libxfce4panel/libxfce4panel.h>
#include <gdk/gdkkeysyms.h>

using namespace WhiskerMenu;

namespace
{

/* focusable_radio_siblings:
 * @parent: the GtkBox holding a GtkRadioButton group.
 *
 * Collects the visible, sensitive radio-button children in physical
 * box order. Non-radio children (separators, the mode selector box)
 * are skipped so wrap-around stays inside the category group.
 *
 * Returns: heap-allocated GList* of GtkWidget*; the caller frees it
 * with g_list_free (do NOT free the nodes themselves).
 */
GList* focusable_radio_siblings(GtkContainer* parent)
{
	GList* out = nullptr;
	GList* children = gtk_container_get_children(parent);
	for (GList* li = children; li; li = li->next)
	{
		GtkWidget* w = GTK_WIDGET(li->data);
		if (!GTK_IS_RADIO_BUTTON(w))
			continue;
		if (!gtk_widget_get_visible(w) || !gtk_widget_get_sensitive(w))
			continue;
		out = g_list_append(out, w);
	}
	g_list_free(children);
	return out;
}

/* sidebar_is_vertical:
 * @parent: the GtkBox holding the category buttons.
 *
 * Returns: true iff the parent box is oriented vertically (sidebar at
 * left or right); false when it is horizontal (sidebar at top/bottom).
 */
bool sidebar_is_vertical(GtkContainer* parent)
{
	if (!GTK_IS_ORIENTABLE(parent))
		return true;
	return gtk_orientable_get_orientation(GTK_ORIENTABLE(parent))
			== GTK_ORIENTATION_VERTICAL;
}

} // namespace

//-----------------------------------------------------------------------------

static gboolean hover_timeout(gpointer user_data)
{
	GtkToggleButton* button = GTK_TOGGLE_BUTTON(user_data);
	if (gtk_widget_get_state_flags(GTK_WIDGET(button)) & GTK_STATE_FLAG_PRELIGHT)
	{
		gtk_toggle_button_set_active(button, true);
	}
	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

CategoryButton::CategoryButton(Settings* settings, GIcon* icon, const gchar* text) :
	m_settings(settings)
{
	m_button = GTK_RADIO_BUTTON(gtk_radio_button_new(nullptr));
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_button), false);
	gtk_button_set_relief(GTK_BUTTON(m_button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(GTK_WIDGET(m_button), text);
	gtk_widget_set_focus_on_click(GTK_WIDGET(m_button), false);

	connect(m_button, "enter-notify-event",
		[this](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			GtkToggleButton* button = GTK_TOGGLE_BUTTON(widget);
			if (m_settings->category_hover_activate && !gtk_toggle_button_get_active(button))
			{
				g_timeout_add(150, &hover_timeout, button);
			}
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_button, "focus-in-event",
		[this](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			GtkToggleButton* button = GTK_TOGGLE_BUTTON(widget);
			if (m_settings->category_hover_activate && !gtk_toggle_button_get_active(button))
			{
				gtk_toggle_button_set_active(button, true);
				gtk_widget_grab_focus(widget);
			}
			return GDK_EVENT_PROPAGATE;
		});

	// FR-042: ↑/↓ (or ←/→ in horizontal sidebar layouts) wrap at the
	// ends of the category group; Home/End jump to first/last. The
	// exit-to-results arrow (perpendicular to the sidebar axis) is left
	// to the window-level handler in Window::on_key_press_event which
	// checks FR-046 (no-op while Searching) and the LTR/RTL mirror
	// (FR-120).
	connect(m_button, "key-press-event",
		[](GtkWidget* widget, GdkEvent* ev) -> gboolean
		{
			GdkEventKey* key = reinterpret_cast<GdkEventKey*>(ev);
			GtkWidget* parent = gtk_widget_get_parent(widget);
			if (!parent)
				return GDK_EVENT_PROPAGATE;

			GList* siblings = focusable_radio_siblings(GTK_CONTAINER(parent));
			if (!siblings)
				return GDK_EVENT_PROPAGATE;

			const bool vertical = sidebar_is_vertical(GTK_CONTAINER(parent));
			const guint kv = key->keyval;

			GList* self_node = g_list_find(siblings, widget);
			const bool at_first = (self_node == siblings);
			const bool at_last  = (self_node && self_node->next == nullptr);

			GtkWidget* target = nullptr;

			if (kv == GDK_KEY_Home || kv == GDK_KEY_KP_Home)
			{
				target = GTK_WIDGET(siblings->data);
			}
			else if (kv == GDK_KEY_End || kv == GDK_KEY_KP_End)
			{
				target = GTK_WIDGET(g_list_last(siblings)->data);
			}
			else if (vertical
					&& (kv == GDK_KEY_Up || kv == GDK_KEY_KP_Up)
					&& at_first)
			{
				target = GTK_WIDGET(g_list_last(siblings)->data);
			}
			else if (vertical
					&& (kv == GDK_KEY_Down || kv == GDK_KEY_KP_Down)
					&& at_last)
			{
				target = GTK_WIDGET(siblings->data);
			}
			else if (!vertical
					&& (kv == GDK_KEY_Left || kv == GDK_KEY_KP_Left)
					&& at_first)
			{
				target = GTK_WIDGET(g_list_last(siblings)->data);
			}
			else if (!vertical
					&& (kv == GDK_KEY_Right || kv == GDK_KEY_KP_Right)
					&& at_last)
			{
				target = GTK_WIDGET(siblings->data);
			}

			g_list_free(siblings);

			if (target && target != widget)
			{
				gtk_widget_grab_focus(target);
				return GDK_EVENT_STOP;
			}
			return GDK_EVENT_PROPAGATE;
		});

	m_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
	gtk_container_add(GTK_CONTAINER(m_button), GTK_WIDGET(m_box));

	m_icon = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(m_box, m_icon, false, false, 0);

	m_label = gtk_label_new(text);
	// Cap the content-fit sidebar width: a pathological category name
	// ellipsises at the end instead of widening the sidebar without bound
	// (FR-025). The size group still sizes to the widest *capped* label.
	gtk_label_set_ellipsize(GTK_LABEL(m_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(m_label), 22);
	gtk_label_set_xalign(GTK_LABEL(m_label), 0.0);
	gtk_box_pack_start(m_box, m_label, false, true, 0);

	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_button)), "category-button");

	gtk_widget_show_all(GTK_WIDGET(m_button));

	reload_icon_size();
}

//-----------------------------------------------------------------------------

CategoryButton::~CategoryButton()
{
	gtk_widget_destroy(GTK_WIDGET(m_button));
}

//-----------------------------------------------------------------------------

void CategoryButton::reload_icon_size()
{
	int size = m_settings->category_icon_size.get_size();
	gtk_image_set_pixel_size(GTK_IMAGE(m_icon), size);
	gtk_widget_set_visible(m_icon, size > 1);

	// NOTE: schema v2 — categories are rendered icon-only whenever the sidebar is
	// laid out horizontally (sidebar-position ∈ {top, bottom}); the legacy
	// /position-categories-horizontal key is migrated away (see migrate_schema).
	const bool sidebar_horizontal = (g_strcmp0(m_settings->sidebar_position, "top") == 0
			|| g_strcmp0(m_settings->sidebar_position, "bottom") == 0);
	if (m_settings->category_show_name && !sidebar_horizontal)
	{
		gtk_widget_set_has_tooltip(GTK_WIDGET(m_button), false);
		gtk_box_set_child_packing(m_box, m_icon, false, false, 0, GTK_PACK_START);
		gtk_widget_show(m_label);
	}
	else
	{
		gtk_widget_set_has_tooltip(GTK_WIDGET(m_button), true);
		gtk_widget_hide(m_label);
		gtk_box_set_child_packing(m_box, m_icon, true, true, 0, GTK_PACK_START);
	}
}

//-----------------------------------------------------------------------------
