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

#include "core/sidebar-layout.h"
#include "settings.h"
#include "ui/slot.h"

#include <libxfce4panel/libxfce4panel.h>

using namespace WhiskerMenu;

namespace
{

// HACK: process-wide latch shared by every CategoryButton, toggled by the
// window keyboard handler (CategoryButton::suppress_hover_until_motion) and the
// per-button motion-notify handler below. It exists because keyboard category
// navigation lives at the window level while the hover auto-activation lives in
// these leaf widgets, and a leaf button has no back-reference to the Window.
// When set, the enter-notify timeout and the focus-in auto-activate skip
// activation so a stationary pointer cannot eject keyboard focus (FR-012); the
// first genuine motion-notify over any category button re-arms hover.
static bool s_hover_suppressed_until_motion = false;

} // namespace

void WhiskerMenu::CategoryButton::suppress_hover_until_motion()
{
	s_hover_suppressed_until_motion = true;
}

//-----------------------------------------------------------------------------

static gboolean hover_timeout(gpointer user_data)
{
	GtkToggleButton* button = GTK_TOGGLE_BUTTON(user_data);
	// NOTE: a keyboard navigation may have fired after this 150 ms timeout was
	// armed; honouring the suppression latch keeps the stale pointer from
	// activating the button it still rests over (FR-012).
	if (s_hover_suppressed_until_motion)
	{
		return GDK_EVENT_PROPAGATE;
	}
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

	// Pointer-motion events are needed to re-arm hover after a keyboard
	// navigation suppressed it (FR-012); a plain button does not request them.
	gtk_widget_add_events(GTK_WIDGET(m_button), GDK_POINTER_MOTION_MASK);

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

	// Genuine pointer motion re-arms hover activation that a keyboard
	// navigation had suppressed, so "keyboard wins over a still pointer" stays
	// deterministic: only after the user actually moves the mouse does hover
	// take over again (FR-012, C5).
	connect(m_button, "motion-notify-event",
		[](GtkWidget*, GdkEvent*) -> gboolean
		{
			s_hover_suppressed_until_motion = false;
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_button, "focus-in-event",
		[this](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			GtkToggleButton* button = GTK_TOGGLE_BUTTON(widget);
			// While hover is suppressed (a keyboard navigation just moved focus
			// here), do not auto-activate on focus-in: the window handler owns
			// the keyboard activation model (live vs Enter-to-commit) and a
			// focus-in activation here would bypass its guard (FR-012).
			if (m_settings->category_hover_activate
					&& !s_hover_suppressed_until_motion
					&& !gtk_toggle_button_get_active(button))
			{
				gtk_toggle_button_set_active(button, true);
				gtk_widget_grab_focus(widget);
			}
			return GDK_EVENT_PROPAGATE;
		});

	// NOTE: along-axis category navigation (mid-list moves, Home/End and
	// wrap-around) is owned by Window::on_key_press_event, which consumes the
	// event before GTK's default radio-group key navigation can auto-activate
	// the next radio. No per-button key handler is needed here; adding one would
	// re-introduce the double-move / double-activation this fix removes.

	m_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
	gtk_container_add(GTK_CONTAINER(m_button), GTK_WIDGET(m_box));

	m_icon = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(m_box, m_icon, false, false, 0);

	m_label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(m_label), 0.0);
	// Content-fit sidebar width (FR-023/025): only labels that exceed the cap
	// ellipsise. Capping a label with ellipsization also drops its *minimum*
	// width to ~one glyph, and a GtkSizeGroup aggregates minimums as the
	// max-of-minimums — so if every label could ellipsise, the group floor
	// collapses and the non-expanding sidebar gets squeezed to the switch's
	// width under the fixed menu width. Leaving short labels non-ellipsising
	// keeps minimum == natural, so the sidebar stays as wide as its widest
	// item; a single pathological name still ellipsises instead of widening
	// the sidebar without bound.
	if (g_utf8_strlen(text, -1) > 22)
	{
		gtk_label_set_ellipsize(GTK_LABEL(m_label), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(m_label), 22);
	}
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
	// The single shared decision (FR-015/016) ensures Apps category buttons and
	// Places section buttons — all CategoryButtons reloaded on the same trigger —
	// show or hide labels identically in both modes.
	const bool sidebar_horizontal = (g_strcmp0(m_settings->sidebar_position, "top") == 0
			|| g_strcmp0(m_settings->sidebar_position, "bottom") == 0);
	if (meow_category_label_visible(m_settings->category_show_name, sidebar_horizontal))
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
