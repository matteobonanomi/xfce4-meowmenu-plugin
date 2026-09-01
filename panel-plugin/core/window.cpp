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

#include "window.h"

#include "category-lifetime.h"
#include "drag-to-favourites.h"
#include "launcher/applications-page.h"
#include "launcher/category-button.h"
#include "launcher/command.h"
#include "launcher/favorites-page.h"
#include "launcher/launcher.h"
#include "launcher/page.h"
#include "places/favourites-section.h"
#include "places/history-section.h"
#include "places/home-section.h"
#include "places/places-item.h"
#include "ui/launcher-view.h"
#include "ui/grid-presentation.h"
#include "places/places-page.h"
#include "plugin.h"
#include "profile.h"
#include "launcher/recent-page.h"
#include "menu-composition.h"
#include "resizer.h"
#include "opacity-model.h"
#include "search/search-page.h"
#include "settings.h"
#include "sidebar-layout.h"
#include "theme-fallback.h"
#include "user-session-relayout.h"
#include "window-frame.h"
#include "ui/slot.h"
#include "ui/switch-icons.h"
#include "window-keyboard.h"

#include <libxfce4ui/libxfce4ui.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

#include <algorithm>
#include <ctime>
#include <iterator>
#include <string>
#include <vector>

using namespace WhiskerMenu;

namespace
{

/* selection_data_to_string:
 * @data: GTK selection data from an in-process favourite drop.
 *
 * Copies the payload bytes into a std::string without assuming GTK supplied a
 * trailing NUL. The custom drag targets send either a desktop ID or a URI, both
 * treated as opaque UTF-8 identifiers by the drop handler.
 *
 * Returns: copied payload text, or an empty string when no payload is present.
 */
static std::string selection_data_to_string(GtkSelectionData* data)
{
	if (!data || gtk_selection_data_get_length(data) <= 0)
	{
		return {};
	}

	const guchar* raw = gtk_selection_data_get_data(data);
	if (!raw)
	{
		return {};
	}

	return std::string(reinterpret_cast<const char*>(raw),
			gtk_selection_data_get_length(data));
}

/* scroller_add_child:
 * @scroller: a GtkScrolledWindow that wraps non-scrollable content in a viewport.
 * @child: the widget to (re-)insert.
 *
 * GTK3's gtk_scrolled_window_add() asserts the scrolled window is empty
 * (g_return_if_fail child == NULL). After a child has been removed, the
 * implicitly-created GtkViewport stays behind as the scrolled window's child,
 * so a second gtk_container_add() silently no-ops — and any caller-held ref
 * dropped afterwards would then destroy @child. Reinsert into the surviving
 * viewport when one is present; otherwise let the scrolled window build one.
 */
static void scroller_add_child(GtkScrolledWindow* scroller, GtkWidget* child)
{
	GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(scroller));
	if (GTK_IS_VIEWPORT(viewport))
		gtk_container_add(GTK_CONTAINER(viewport), child);
	else
		gtk_container_add(GTK_CONTAINER(scroller), child);
}

/* focusable_category_siblings:
 * @parent: the GtkBox holding a category GtkRadioButton group.
 *
 * Collects the visible, sensitive radio-button children in physical box
 * order. Non-radio children (the mode selector box, separators, the leading
 * spacer) are skipped, and the mode toggle keeps along-axis navigation inside
 * the category group. In each mode only the active group's buttons are visible,
 * so the result already excludes the hidden sibling group (Apps vs Places).
 *
 * Returns: heap-allocated GList* of GtkWidget*; the caller frees it with
 * g_list_free (the nodes point at borrowed widgets — do not free those).
 */
static GList* focusable_category_siblings(GtkContainer* parent)
{
	GList* out = nullptr;
	GList* children = gtk_container_get_children(parent);
	for (GList* li = children; li; li = li->next)
	{
		GtkWidget* w = GTK_WIDGET(li->data);
		if (!GTK_IS_RADIO_BUTTON(w))
			continue;
		if (!gtk_widget_get_visible(w) || !gtk_widget_get_sensitive(w)
				|| !gtk_widget_get_can_focus(w))
			continue;
		out = g_list_append(out, w);
	}
	g_list_free(children);
	return out;
}

/* widget_navigation_rect:
 * @widget: live widget whose allocated bounds are requested.
 * @toplevel: coordinate space receiving the translated rectangle.
 * @rectangle: output integer geometry; unchanged when translation fails.
 *
 * Reads visibility, mapping, allocation, and hierarchy coordinates at the
 * moment of one arrow event. No result is cached between events.
 *
 * Returns: true when the widget has a usable positive rectangle.
 */
bool widget_navigation_rect(GtkWidget* widget, GtkWidget* toplevel,
		Keyboard::NavigationRect* rectangle)
{
	if (!widget || !rectangle || !gtk_widget_get_visible(widget)
			|| !gtk_widget_get_mapped(widget))
		return false;
	GtkAllocation allocation = {};
	gtk_widget_get_allocation(widget, &allocation);
	if (allocation.width <= 0 || allocation.height <= 0)
		return false;
	int x = allocation.x;
	int y = allocation.y;
	if (toplevel && toplevel != widget)
	{
		int translated_x = 0;
		int translated_y = 0;
		if (!gtk_widget_translate_coordinates(widget, toplevel,
				0, 0, &translated_x, &translated_y))
			return false;
		x = translated_x;
		y = translated_y;
	}
	*rectangle = Keyboard::NavigationRect(x, y,
			allocation.width, allocation.height);
	return rectangle->is_valid();
}

} // namespace

WhiskerMenu::Window::Window(Settings* settings, Plugin* plugin) :
	m_settings(settings),
	m_plugin(plugin),
	m_window(nullptr),
	m_css_provider(nullptr),
	m_position(PositionAtButton),
	m_monitor{0,0,0,0},
	m_workarea{0,0,0,0},
	m_favorites_heading(nullptr),
	m_recent_heading(nullptr),
	m_places(nullptr),
	m_mode_selector_box(nullptr),
	m_mode_btn_apps(nullptr),
	m_mode_btn_places(nullptr),
	m_places_home_btn(nullptr),
	m_places_history_btn(nullptr),
	m_places_fav_btn(nullptr),
	m_places_active(false),
	m_mode_switch_in_progress(false),
	m_keyboard_category_nav(false),
	m_places_property_slot(0),
	m_live_settings_property_slot(0),
	m_mode_selector_separator(nullptr),
	m_category_group_separator(nullptr),
	m_strip_scroll(nullptr),
	m_strip_lead_spacer(nullptr),
	m_strip_trail_spacer(nullptr),
	m_sidebar_struct(1),
	m_category_width_group(nullptr),
	m_mode_button_size_group(nullptr),
	m_geometry{0,0,1,1},
	m_layout_ltr(true),
	m_layout_categories_horizontal(false),
	m_layout_sidebar_position(CompositionSidebar::Left),
	m_layout_sidebar_enabled(true),
	m_layout_available_session_actions(0),
	m_composition{},
	m_layout_snapshot{},
	m_surface_palette{},
	m_layout_metrics{6, ThemeMetricsSource::SafeFallback},
	m_style_refresh_source(0),
	m_style_refresh_running(false),
	m_profile_shape(0),
	m_supports_alpha(false),
	m_child_has_focus(false),
	m_resizing(false),
	m_resize_tick_id(0),
	m_resize_monitor(nullptr),
	m_resize_monitor_notify_slot(0),
	m_resize_monitor_removed_slot(0)
{
	// Create the window
	m_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	m_frame = nullptr;
	gtk_widget_set_name(GTK_WIDGET(m_window), "meowmenu-window");
	// Untranslated window title to allow window managers to identify it; not visible to users.
	gtk_window_set_title(m_window, "Meow Menu");
#ifdef HAVE_GTK_LAYER_SHELL
	if (!gtk_layer_is_supported())
#endif
	{
		gtk_window_set_modal(m_window, true);
	}
	gtk_window_set_decorated(m_window, false);
	gtk_window_set_skip_taskbar_hint(m_window, true);
	gtk_window_set_skip_pager_hint(m_window, true);
	gtk_window_set_type_hint(m_window, GDK_WINDOW_TYPE_HINT_MENU);
	gtk_window_stick(m_window);
	gtk_widget_add_events(GTK_WIDGET(m_window), GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK);

#ifdef HAVE_GTK_LAYER_SHELL
	if (gtk_layer_is_supported())
	{
		gtk_layer_init_for_window(m_window);

		// Position from top left, and excludes other windows
		gtk_layer_set_exclusive_zone(m_window, -1);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_TOP, true);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_BOTTOM, false);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_LEFT, true);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_RIGHT, false);

		// Grab keyboard focus when shown
		gtk_layer_set_keyboard_mode(m_window, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

		// Position menu above other windows
		gtk_layer_set_layer(m_window, GTK_LAYER_SHELL_LAYER_OVERLAY);
	}
#endif

	connect(m_window, "enter-notify-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			m_child_has_focus = false;
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_window, "focus-in-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			m_child_has_focus = false;
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_window, "focus-out-event",
		[this](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			if (m_resizing)
				interactive_resize_cancel();

			if (m_settings->stay_on_focus_out || m_child_has_focus || !gtk_widget_get_visible(widget))
			{
				return GDK_EVENT_PROPAGATE;
			}

			// Needed to make focus out event happen after button press event,
			// otherwise it is impossible to toggle panel button.
			g_idle_add(
				+[](gpointer user_data) -> gboolean
				{
					static_cast<Window*>(user_data)->hide(true);
					return G_SOURCE_REMOVE;
				},
			this);

			return GDK_EVENT_PROPAGATE;
		});

	connect(m_window, "key-press-event",
		[this](GtkWidget* widget, GdkEvent* event) -> gboolean
		{
			return on_key_press_event(widget, reinterpret_cast<GdkEventKey*>(event));
		});

	connect(m_window, "key-press-event",
		[this](GtkWidget* widget, GdkEvent* event) -> gboolean
		{
			return on_key_press_event_after(widget, reinterpret_cast<GdkEventKey*>(event));
		},
		Connect::After);

	connect(m_window, "map-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			return on_map_event();
		});

	connect(m_window, "unmap-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			if (m_resizing)
				interactive_resize_cancel();
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_window, "state-flags-changed",
		[this](GtkWidget* widget, GtkStateFlags)
		{
			on_state_flags_changed(widget);
		});

	g_signal_connect(G_OBJECT(m_window), "delete-event", G_CALLBACK(&gtk_widget_hide_on_delete), nullptr);

	// Structural container only — the frame no longer carries a visible border.
	// The single intentional window border is stroked along the rounded clip path
	// in on_draw_event; a frame shadow here would draw a second, square outline
	// that ignores the corner radius (the doubled/ghost line defect).
	m_frame = GTK_FRAME(gtk_frame_new(nullptr));
	gtk_frame_set_shadow_type(m_frame, GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_frame));

	// Create window contents stack
	m_window_stack = GTK_STACK(gtk_stack_new());
	gtk_container_add(GTK_CONTAINER(m_frame), GTK_WIDGET(m_window_stack));

	// Create loading message
	m_window_load_spinner = GTK_SPINNER(gtk_spinner_new());
	gtk_widget_set_halign(GTK_WIDGET(m_window_load_spinner), GTK_ALIGN_CENTER);
	gtk_widget_set_valign(GTK_WIDGET(m_window_load_spinner), GTK_ALIGN_CENTER);
	gtk_stack_add_named(m_window_stack, GTK_WIDGET(m_window_load_spinner), "load");

	// Create resizers
	m_resize[Resizer::TopLeft] = new Resizer(Resizer::TopLeft, this);
	m_resize[Resizer::Top] = new Resizer(Resizer::Top, this);
	m_resize[Resizer::TopRight] = new Resizer(Resizer::TopRight, this);
	m_resize[Resizer::Left] = new Resizer(Resizer::Left, this);
	m_resize[Resizer::Right] = new Resizer(Resizer::Right, this);
	m_resize[Resizer::BottomLeft] = new Resizer(Resizer::BottomLeft, this);
	m_resize[Resizer::Bottom] = new Resizer(Resizer::Bottom, this);
	m_resize[Resizer::BottomRight] = new Resizer(Resizer::BottomRight, this);

	// Create the profile picture and username label
	m_profile = new Profile(m_settings, this);

	// Create action buttons. supported behavior / supported behavior: a 250 ms monotonic-clock
	// debounce absorbs held-Enter key-repeat bursts so a single press
	// (or burst) launches exactly once. The state is process-global
	// across the nine session buttons because the user can only target
	// one button per keypress, and a stuck key must not fan out across
	// adjacent buttons.
	static Keyboard::ActivationDebounce s_command_debounce;
	for (int i = 0; i < 9; ++i)
	{
		m_commands_button[i] = m_settings->command[i]->get_button();
		// supported behavior: session buttons participate in the keyboard focus chain.
		gtk_widget_set_can_focus(m_commands_button[i], TRUE);
		gtk_style_context_add_class(gtk_widget_get_style_context(m_commands_button[i]),
				"meow-focus-ring");
		m_command_slots[i] = connect(m_commands_button[i], "clicked",
			[this](GtkButton*)
			{
				if (!s_command_debounce.accept(g_get_monotonic_time()))
				{
					return;
				}
				hide();
			});
	}

	// The avatar event-box and username label are decorative only; keep
	// them out of the focus chain so Tab into the Profile bar lands on
	// the first visible session button (data-model "Entry widget mapping").
	if (m_profile)
	{
		gtk_widget_set_can_focus(m_profile->get_picture(), FALSE);
		gtk_widget_set_can_focus(m_profile->get_username(), FALSE);
	}

	// Create search entry
	m_search_entry = GTK_ENTRY(gtk_search_entry_new());
	gtk_window_set_focus(m_window, GTK_WIDGET(m_search_entry));

	connect(m_search_entry, "changed",
		[this](GtkEditable*)
		{
			search();
		});

	connect(m_search_entry, "button-press-event",
		[this](GtkWidget*, GdkEventButton*) -> gboolean
		{
			if (m_places_active)
				m_places->note_deliberate_navigation();
			return GDK_EVENT_PROPAGATE;
		});

	// supported behavior: Enter on the search entry activates the first match (or
	// is a silent no-op when the current query has zero results — see
	// clarification Q4). The debounce inside Page::launcher_activated
	// covers held-Enter key-repeat (supported behavior).
	connect(m_search_entry, "activate",
		[this](GtkEntry*)
		{
			const gchar* text = gtk_entry_get_text(m_search_entry);
			if (xfce_str_is_empty(text))
			{
				return;
			}
			m_search_results->activate_first();
		});

	connect(m_search_entry, "populate-popup",
		[this](GtkEntry*, GtkWidget*)
		{
			set_child_has_focus();
		});

	// Create favorites
	m_favorites = new FavoritesPage(m_settings, this);

	CategoryButton* favorites_button = m_favorites->get_button();
	connect(favorites_button->get_widget(), "toggled",
		[this](GtkToggleButton* button)
		{
			favorites_toggled(button);
		});
	connect(favorites_button->get_widget(), "drag-motion",
		[this](GtkWidget*, GdkDragContext* context, gint, gint, guint time) -> gboolean
		{
			return on_application_favourites_drag_motion(context, time);
		});
	connect(favorites_button->get_widget(), "drag-leave",
		[this](GtkWidget* widget, GdkDragContext*, guint)
		{
			on_favourite_drag_leave(widget);
		});
	connect(favorites_button->get_widget(), "drag-data-received",
		[this](GtkWidget*, GdkDragContext* context, gint, gint, GtkSelectionData* data,
				guint info, guint time)
		{
			on_application_favourites_drag_data_received(context, data, info, time);
		});

	// Create recent
	m_recent = new RecentPage(m_settings, this);

	CategoryButton* recent_button = m_recent->get_button();
	recent_button->join_group(favorites_button);
	connect(recent_button->get_widget(), "toggled",
		[this](GtkToggleButton* button)
		{
			recent_toggled(button);
		});

	// Create applications
	m_applications = new ApplicationsPage(m_settings, this);

	CategoryButton* applications_button = m_applications->get_button();
	applications_button->join_group(recent_button);
	connect(applications_button->get_widget(), "toggled",
		[this](GtkToggleButton* button)
		{
			category_toggled(button);
		});

	// Places mode (current behavior) — built unconditionally; visibility is gated
	// on m_settings->places_enabled in update_layout().
	m_places = new PlacesPage(m_settings, this);

	{
		GIcon* home_icon = g_themed_icon_new(m_places->get_home_section()->get_icon_name());
		m_places_home_btn = new CategoryButton(m_settings, home_icon,
				m_places->get_home_section()->get_display_name());
		g_object_unref(home_icon);

		GIcon* hist_icon = g_themed_icon_new(m_places->get_history_section()->get_icon_name());
		m_places_history_btn = new CategoryButton(m_settings, hist_icon,
				m_places->get_history_section()->get_display_name());
		g_object_unref(hist_icon);
		m_places_history_btn->join_group(m_places_home_btn);

		GIcon* fav_icon = g_themed_icon_new(m_places->get_favourites_section()->get_icon_name());
		m_places_fav_btn = new CategoryButton(m_settings, fav_icon,
				m_places->get_favourites_section()->get_display_name());
		g_object_unref(fav_icon);
		m_places_fav_btn->join_group(m_places_history_btn);
	}

	// The three Places section toggles mirror the Applications handlers: pointer
	// selection hands focus to the search entry so the user can type, but a
	// keyboard-driven activation (m_keyboard_category_nav set) keeps focus on the
	// active section button so arrow navigation can continue (supported behavior; C1/C2).
	connect(m_places_home_btn->get_widget(), "toggled",
		[this](GtkToggleButton* b)
		{
			if (!gtk_toggle_button_get_active(b))
				return;
			m_places->set_active_section(m_places->get_home_section());
			gtk_stack_set_visible_child_name(m_panels_stack, "places");
			m_places->present();
			if (!m_keyboard_category_nav)
				gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		});
	connect(m_places_history_btn->get_widget(), "toggled",
		[this](GtkToggleButton* b)
		{
			if (!gtk_toggle_button_get_active(b))
				return;
			m_places->set_active_section(m_places->get_history_section());
			gtk_stack_set_visible_child_name(m_panels_stack, "places");
			m_places->present();
			if (!m_keyboard_category_nav)
				gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		});
	connect(m_places_fav_btn->get_widget(), "toggled",
		[this](GtkToggleButton* b)
		{
			if (!gtk_toggle_button_get_active(b))
				return;
			m_places->set_active_section(m_places->get_favourites_section());
			gtk_stack_set_visible_child_name(m_panels_stack, "places");
			m_places->present();
			if (!m_keyboard_category_nav)
				gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		});
	connect(m_places_fav_btn->get_widget(), "drag-motion",
		[this](GtkWidget*, GdkDragContext* context, gint, gint, guint time) -> gboolean
		{
			return on_places_favourites_drag_motion(context, time);
		});
	connect(m_places_fav_btn->get_widget(), "drag-leave",
		[this](GtkWidget* widget, GdkDragContext*, guint)
		{
			on_favourite_drag_leave(widget);
		});
	connect(m_places_fav_btn->get_widget(), "drag-data-received",
		[this](GtkWidget*, GdkDragContext* context, gint, gint, GtkSelectionData* data,
				guint info, guint time)
		{
			on_places_favourites_drag_data_received(context, data, info, time);
		});

	// Mode selector: two toggle buttons forming a manual radio group.
	m_mode_selector_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	g_object_ref_sink(m_mode_selector_box);
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_mode_selector_box)),
			"places-mode-selector");
	m_mode_btn_apps = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(_("Apps")));
	m_mode_btn_places = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(_("Places")));
	gtk_button_set_relief(GTK_BUTTON(m_mode_btn_apps), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(m_mode_btn_places), GTK_RELIEF_NONE);
	// supported behavior: no Alt-mnemonic activation on the mode toggles — the labels
	// "Apps"/"Places" must render verbatim, not as "_Apps"/"_Places".
	gtk_button_set_use_underline(GTK_BUTTON(m_mode_btn_apps),   FALSE);
	gtk_button_set_use_underline(GTK_BUTTON(m_mode_btn_places), FALSE);
	// supported behavior / supported behavior: shared focus-indicator class. The rule lives in
	// the CSS provider built by update_background_css() so themes can
	// override the ring colour/thickness by re-styling .meow-focus-ring.
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_mode_btn_apps)),
			"meow-focus-ring");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_mode_btn_places)),
			"meow-focus-ring");
	gtk_toggle_button_set_active(m_mode_btn_apps, true);
	gtk_box_pack_start(m_mode_selector_box, GTK_WIDGET(m_mode_btn_apps), true, true, 0);
	gtk_box_pack_start(m_mode_selector_box, GTK_WIDGET(m_mode_btn_places), true, true, 0);
	// Equal-width Apps/Places buttons in every layout/preset (supported behavior). A size
	// group forces both to the larger natural width regardless of label length
	// or the icon↔text child swap — more robust than box homogeneity, which the
	// strip relocation can defeat.
	m_mode_button_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget(m_mode_button_size_group, GTK_WIDGET(m_mode_btn_apps));
	gtk_size_group_add_widget(m_mode_button_size_group, GTK_WIDGET(m_mode_btn_places));

	connect(m_mode_btn_apps, "toggled",
		[this](GtkToggleButton* b)
		{
			if (m_mode_switch_in_progress) return;
			if (gtk_toggle_button_get_active(b))
				switch_mode(false);
			else if (!gtk_toggle_button_get_active(m_mode_btn_places))
				gtk_toggle_button_set_active(b, true);
		});
	connect(m_mode_btn_places, "toggled",
		[this](GtkToggleButton* b)
		{
			if (m_mode_switch_in_progress) return;
			if (gtk_toggle_button_get_active(b))
				switch_mode(true);
			else if (!gtk_toggle_button_get_active(m_mode_btn_apps))
				gtk_toggle_button_set_active(b, true);
		});

	// The Apps/Places mode is now operated solely by Tab/Shift+Tab and is
	// never a keyboard-focus stop (supported behavior). Make both toggle buttons
	// non-focusable so GTK's own Tab traversal can never land on them and
	// so no arrow-key mode flip is possible from them; mouse activation
	// via the "toggled" handlers above is unaffected.
	gtk_widget_set_can_focus(GTK_WIDGET(m_mode_btn_apps),   FALSE);
	gtk_widget_set_can_focus(GTK_WIDGET(m_mode_btn_places), FALSE);

	// Create search results
	m_search_results = new SearchPage(m_settings, this);

	GtkBox* search_results = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(search_results, m_search_results->get_calculator_result(), false, false, 0);
	gtk_box_pack_start(search_results, m_search_results->get_message(), false, false, 0);
	gtk_box_pack_start(search_results, m_search_results->get_widget(), true, true, 0);
	gtk_container_set_border_width(GTK_CONTAINER(search_results), 0);

	// Create grid for packing resizers
	GtkGrid* grid = GTK_GRID(gtk_grid_new());
	gtk_grid_attach(grid, m_resize[Resizer::TopLeft]->get_widget(), 0, 0, 1, 1);
	gtk_grid_attach(grid, m_resize[Resizer::Top]->get_widget(), 1, 0, 1, 1);
	gtk_grid_attach(grid, m_resize[Resizer::TopRight]->get_widget(), 2, 0, 1, 1);
	gtk_grid_attach(grid, m_resize[Resizer::Left]->get_widget(), 0, 1, 1, 1);
	gtk_grid_attach(grid, m_resize[Resizer::Right]->get_widget(), 2, 1, 1, 1);
	gtk_grid_attach(grid, m_resize[Resizer::BottomLeft]->get_widget(), 0, 2, 1, 1);
	gtk_grid_attach(grid, m_resize[Resizer::Bottom]->get_widget(), 1, 2, 1, 1);
	gtk_grid_attach(grid, m_resize[Resizer::BottomRight]->get_widget(), 2, 2, 1, 1);
	gtk_stack_add_named(m_window_stack, GTK_WIDGET(grid), "contents");

	// Create box for packing children
	m_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	gtk_container_set_border_width(GTK_CONTAINER(m_vbox), 0);
	gtk_grid_attach(grid, GTK_WIDGET(m_vbox), 1, 1, 1, 1);

	// Create box for packing commands
	m_commands_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	m_commands_spacer = gtk_label_new(nullptr);
	gtk_box_pack_start(m_commands_box, m_commands_spacer, true, true, 0);
	for (auto command : m_commands_button)
	{
		gtk_box_pack_start(m_commands_box, command, false, false, 0);
	}

	// Create box for packing username and commands
	m_title_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
	m_primary_row = m_title_box;
	m_primary_middle = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
	m_sidebar_header_reserve = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_can_focus(m_sidebar_header_reserve, FALSE);
	g_object_ref_sink(m_primary_middle);
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_title_box), false, false, 0);
	gtk_box_pack_start(m_title_box, m_profile->get_widget(), false, false, 0);
	gtk_box_pack_start(m_title_box, m_sidebar_header_reserve,
			false, false, 0);
	gtk_box_pack_start(m_title_box, GTK_WIDGET(m_commands_box), false, false, 0);

	// Add search to layout
	m_search_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
	m_secondary_row = m_search_box;
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_search_box), false, true, 0);
	gtk_box_pack_start(m_search_box, GTK_WIDGET(m_search_entry), true, true, 0);

	// Create box for packing launcher pages and sidebar
	m_contents_stack = GTK_STACK(gtk_stack_new());
	m_contents_box = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(m_contents_box, 6);
	gtk_grid_set_row_spacing(m_contents_box, 0);
	gtk_stack_add_named(m_contents_stack, GTK_WIDGET(m_contents_box), "contents");
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_contents_stack), true, true, 0);

	// Create box for packing categories horizontally
	m_categories_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_categories_box), 0, 0, 2, 1);

	// Create box for packing launcher pages
	m_panels_stack = GTK_STACK(gtk_stack_new());
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_panels_stack)),
		"applications-area");
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 0, 1, 1, 1);
	gtk_widget_set_hexpand(GTK_WIDGET(m_panels_stack), true);
	gtk_widget_set_vexpand(GTK_WIDGET(m_panels_stack), true);
	GtkWidget* favorites_outer = meow::meowmenu_create_default_heading_page(
			m_favorites->get_widget(), _("FAVORITES"), &m_favorites_heading);
	GtkWidget* recent_outer = meow::meowmenu_create_default_heading_page(
			m_recent->get_widget(), _("RECENTLY USED"), &m_recent_heading);
	gtk_stack_add_named(m_panels_stack, favorites_outer, "favorites");
	gtk_stack_add_named(m_panels_stack, recent_outer, "recent");
	// Use the outer wrapper so the default-category heading (sidebar-disabled
	// case, supported behavior) sits above the applications launcher view.
	gtk_stack_add_named(m_panels_stack, m_applications->get_outer_widget(), "applications");
	// Search results live inside the applications area so the sidebar remains visible
	// while the user types. This applies to all layout modes, not just fullscreen.
	gtk_stack_add_named(m_panels_stack, GTK_WIDGET(search_results), "search");

	// Wrap PlacesPage with its empty-state label, same pattern as search_results.
	GtkBox* places_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(places_box, m_places->get_message(), false, false, 0);
	gtk_box_pack_start(places_box, m_places->get_widget(), true, true, 0);
	gtk_stack_add_named(m_panels_stack, GTK_WIDGET(places_box), "places");

	// Create box for packing sidebar
	m_category_buttons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(m_category_buttons, GTK_WIDGET(m_mode_selector_box), false, false, 0);
	m_mode_selector_separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(m_category_buttons, m_mode_selector_separator, false, false, 4);
	gtk_box_pack_start(m_category_buttons, favorites_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, recent_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, applications_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, m_places_home_btn->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, m_places_history_btn->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, m_places_fav_btn->get_widget(), false, false, 0);
	m_category_group_separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(m_category_buttons, m_category_group_separator,
			false, false, 4);

	// Equalise the width of all category buttons within a mode so the icons and
	// labels line up. NOTE: this size group does NOT by itself keep the sidebar
	// width stable across an Apps<->Places switch — its hidden-widget
	// contribution is unreliable across re-layouts. The cross-mode width floor
	// is held instead by sync_category_label_width(), which pins every button's
	// minimum label width to the widest label in either mode.
	// HACK: gtk_size_group_set_ignore_hidden() is deprecated since GTK 3.22
	// (the behaviour it controls became the default in GTK 4), but GTK 3 still
	// requires the explicit call.  Suppress the deprecation warning locally.
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	m_category_width_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_set_ignore_hidden(m_category_width_group, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
	// Both the Apps-mode buttons and the Places-section buttons join the width
	// group, so switching Apps↔Places (which only toggles visibility) never
	// resizes the sidebar (supported behavior) — the group already sizes to the widest
	// member, hidden or not.
	gtk_size_group_add_widget(m_category_width_group, favorites_button->get_widget());
	gtk_size_group_add_widget(m_category_width_group, recent_button->get_widget());
	gtk_size_group_add_widget(m_category_width_group, applications_button->get_widget());
	gtk_size_group_add_widget(m_category_width_group, m_places_home_btn->get_widget());
	gtk_size_group_add_widget(m_category_width_group, m_places_history_btn->get_widget());
	gtk_size_group_add_widget(m_category_width_group, m_places_fav_btn->get_widget());

	// Establish the shared minimum label width from the built-in buttons; it is
	// recomputed once the application categories load (see set_categories).
	sync_category_label_width();

	// Re-measure the floor when the font or theme changes. NOTE: the box's own
	// style-updated fires on theme/scale changes only — not on child-button
	// hover — so this stays off the hot path. Pinning the label size requests
	// queues a resize, never another style-updated, so there is no feedback loop.
	connect(GTK_WIDGET(m_category_buttons), "style-updated",
		[this](GtkWidget*)
		{
			sync_category_label_width();
			schedule_style_refresh();
		});

	m_sidebar = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 1, 1, 1, 1);
	gtk_scrolled_window_set_propagate_natural_height(m_sidebar, true);
	gtk_scrolled_window_set_shadow_type(m_sidebar, GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_overlay_scrolling(m_sidebar, TRUE);
	gtk_container_add(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));

	// Construct the alternate Horizontal host up front. Layout reconciliation
	// only reparents existing widgets, so opening a new layout never rebuilds the
	// selector or category widget tree after the opening snapshot is captured.
	m_strip_scroll = GTK_SCROLLED_WINDOW(
			gtk_scrolled_window_new(nullptr, nullptr));
	gtk_scrolled_window_set_shadow_type(m_strip_scroll, GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(m_strip_scroll,
			GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_overlay_scrolling(m_strip_scroll, TRUE);
	gtk_scrolled_window_set_propagate_natural_width(m_strip_scroll, FALSE);
	gtk_box_pack_start(m_categories_box, GTK_WIDGET(m_strip_scroll),
			true, true, 0);
	m_strip_lead_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	m_strip_trail_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(m_strip_lead_spacer, TRUE);
	gtk_widget_set_hexpand(m_strip_trail_spacer, TRUE);
	gtk_box_pack_start(m_category_buttons, m_strip_lead_spacer, true, true, 0);
	gtk_box_pack_start(m_category_buttons, m_strip_trail_spacer, true, true, 0);
	gtk_widget_hide(m_strip_lead_spacer);
	gtk_widget_hide(m_strip_trail_spacer);

	bind_category_focus_adjustments();
	connect(GTK_WIDGET(m_category_buttons), "size-allocate",
		[this](GtkWidget*, GtkAllocation*)
		{
			reveal_active_category();
		});

	// Handle default page
	reset_default_button();

	// Add CSS classes
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_window)), "meowmenu");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_search_box)), "search-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_title_box)), "title-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_primary_row)), "primary-row");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_primary_middle)), "primary-middle");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_secondary_row)), "secondary-row");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_commands_box)), "commands-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_contents_stack)), "contents");

	m_css_provider = gtk_css_provider_new();
	GdkScreen* css_screen = gtk_widget_get_screen(GTK_WIDGET(m_window));
	gtk_style_context_add_provider_for_screen(
		css_screen ? css_screen : gdk_screen_get_default(),
		GTK_STYLE_PROVIDER(m_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	schedule_style_refresh();

	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(m_category_buttons));
	gtk_style_context_add_class(context, "categories");
	gtk_style_context_add_class(context, "right");

	// Show widgets
	gtk_widget_show_all(GTK_WIDGET(m_frame));

	// NOTE: gtk_widget_show_all above unconditionally reveals the Apps/Places
	// selector. Keep this temporary visibility aligned with Places until the
	// authoritative layout pass at the start of the first opening.
	if (m_mode_selector_box)
	{
		gtk_widget_set_visible(GTK_WIDGET(m_mode_selector_box),
			m_settings->places_enabled);
	}
	if (m_mode_selector_separator)
	{
		gtk_widget_set_visible(m_mode_selector_separator,
			m_settings->places_enabled);
	}
	m_default_button->set_active(true);

	// Handle transparency
	gtk_widget_set_app_paintable(GTK_WIDGET(m_window), true);

	connect(m_window, "draw",
		[this](GtkWidget* widget, cairo_t* cr) -> gboolean
		{
			return on_draw_event(widget, cr);
		});

	connect(m_window, "screen-changed",
		[this](GtkWidget* widget, GdkScreen*)
		{
			on_screen_changed(widget);
		});
	on_screen_changed(GTK_WIDGET(m_window));

	// Places mode property-change subscriptions (current behavior).
	if (m_settings->channel)
	{
		m_places_property_slot = g_signal_connect(m_settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue*, gpointer user_data) -> void
			{
				auto* self = static_cast<Window*>(user_data);
				if (g_strcmp0(property, "/places/enabled") == 0)
				{
					self->update_layout();
					self->update_favourite_drop_targets();
				}
				else if (g_strcmp0(property, "/places/history-enabled") == 0
						|| g_strcmp0(property, "/places/favourites-enabled") == 0
						|| g_strcmp0(property, "/recent-items-max") == 0)
				{
					self->update_layout();
					self->update_favourite_drop_targets();
				}
				else if (g_strcmp0(property, "/places/favourite-sync") == 0)
				{
					self->m_places->get_favourites_section()->refresh_mode();
					if (self->m_places_active)
						self->m_places->refresh_active();
					self->update_favourite_drop_targets();
				}
			}), this);
	}

	// Re-evaluate RGBA visual and redraw when corner-radius changes so that
	// the rounded-rect clip path is activated/deactivated without reopening the menu.
	if (m_settings->channel)
	{
		m_live_settings_property_slot = g_signal_connect(m_settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue*, gpointer user_data) -> void
			{
				const bool transparent_grid =
						g_strcmp0(property, "/transparent-grid") == 0;
				if (g_strcmp0(property, "/corner-radius") != 0
						&& g_strcmp0(property, "/menu-opacity") != 0
						&& !transparent_grid)
					return;
				// The corner radius is applied entirely by re-clipping and
				// re-stroking in on_draw_event; a menu-opacity change re-runs the
				// CSS so the single shell alpha updates live. The redraw queued
				// below picks up either change without reopening the menu.
				auto* self = static_cast<Window*>(user_data);
				if (!transparent_grid)
				{
					self->update_background_css();
				}
				else
				{
					self->m_search_results->get_view()->reload_icon_size();
					self->m_favorites->get_view()->reload_icon_size();
					self->m_recent->get_view()->reload_icon_size();
					self->m_applications->get_view()->reload_icon_size();
					self->m_places->get_view()->reload_icon_size();
				}
				self->update_view_redraw_safeguards();
				self->on_screen_changed(GTK_WIDGET(self->m_window));
				gtk_widget_queue_draw(GTK_WIDGET(self->m_window));
			}), this);
	}

	// Load applications
	m_applications->load();
	update_favourite_drop_targets();

	g_object_ref_sink(m_window);
}

//-----------------------------------------------------------------------------

/* update_favourite_drop_targets:
 *
 * Reconciles the transient GTK drag destinations with the currently visible
 * sidebar state. Hidden or read-only favourite buttons expose no destination,
 * so wrong-mode and disabled-item drags never discover an invisible target.
 */
void WhiskerMenu::Window::update_favourite_drop_targets()
{
	GtkWidget* app_target = m_favorites->get_button()->get_widget();
	if (application_favourites_drop_available())
	{
		const GtkTargetEntry targets[] = {
			{ g_strdup(WHISKERMENU_APPLICATION_FAVOURITE_DND_TARGET),
				GTK_TARGET_SAME_APP, WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO }
		};
		gtk_drag_dest_set(app_target, GTK_DEST_DEFAULT_ALL, targets, 1,
				GDK_ACTION_COPY);
		g_free(targets[0].target);
	}
	else
	{
		gtk_drag_dest_unset(app_target);
		gtk_drag_unhighlight(app_target);
	}

	GtkWidget* places_target = m_places_fav_btn->get_widget();
	if (places_favourites_drop_available())
	{
		const GtkTargetEntry targets[] = {
			{ g_strdup(WHISKERMENU_PLACES_FAVOURITE_DND_TARGET),
				GTK_TARGET_SAME_APP, WHISKERMENU_PLACES_FAVOURITE_DND_INFO }
		};
		gtk_drag_dest_set(places_target, GTK_DEST_DEFAULT_ALL, targets, 1,
				GDK_ACTION_COPY);
		g_free(targets[0].target);
	}
	else
	{
		gtk_drag_dest_unset(places_target);
		gtk_drag_unhighlight(places_target);
	}
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::Window::application_favourites_drop_available() const
{
	GtkWidget* target = m_favorites->get_button()->get_widget();
	return WhiskerMenu::application_favourites_drop_available(
			m_settings->sidebar_enabled,
			m_places_active,
			gtk_widget_get_visible(target));
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::Window::places_favourites_drop_available() const
{
	GtkWidget* target = m_places_fav_btn->get_widget();
	FavouritesSection* favourites = m_places->get_favourites_section();
	return WhiskerMenu::places_favourites_drop_available(
			m_settings->sidebar_enabled,
			m_settings->places_enabled,
			m_places_active,
			gtk_widget_get_visible(target),
			favourites && (favourites->get_mode() == FavouritesSection::MeowMenuOnly));
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_application_favourites_drag_motion(
		GdkDragContext* context, guint time)
{
	if (!application_favourites_drop_available())
	{
		gdk_drag_status(context, GdkDragAction(0), time);
		return GDK_EVENT_STOP;
	}

	gtk_drag_highlight(m_favorites->get_button()->get_widget());
	gdk_drag_status(context, GDK_ACTION_COPY, time);
	return GDK_EVENT_STOP;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_places_favourites_drag_motion(
		GdkDragContext* context, guint time)
{
	if (!places_favourites_drop_available())
	{
		gdk_drag_status(context, GdkDragAction(0), time);
		return GDK_EVENT_STOP;
	}

	gtk_drag_highlight(m_places_fav_btn->get_widget());
	gdk_drag_status(context, GDK_ACTION_COPY, time);
	return GDK_EVENT_STOP;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::on_favourite_drag_leave(GtkWidget* widget)
{
	gtk_drag_unhighlight(widget);
}

//-----------------------------------------------------------------------------

/* on_application_favourites_drag_data_received:
 * @context: active GTK drag context to finish.
 * @data: selected desktop ID payload.
 * @info: target info emitted by the source.
 * @time: GTK event timestamp for gtk_drag_finish().
 *
 * Resolves the dropped desktop ID through the current application model and
 * appends it via FavoritesPage so duplicate prevention and Xfconf sync stay on
 * the existing favourite path.
 */
void WhiskerMenu::Window::on_application_favourites_drag_data_received(
		GdkDragContext* context, GtkSelectionData* data, guint info, guint time)
{
	GtkWidget* target = m_favorites->get_button()->get_widget();
	gtk_drag_unhighlight(target);

	bool success = false;
	if ((info == WHISKERMENU_APPLICATION_FAVOURITE_DND_INFO)
			&& application_favourites_drop_available()
			&& favourite_drop_accepts(FavouriteDragPayload::Application,
				FavouriteDropTarget::ApplicationFavorites))
	{
		const std::string desktop_id = selection_data_to_string(data);
		Launcher* launcher = m_applications->find(desktop_id);
		if (launcher)
		{
			m_favorites->add(launcher);
			success = true;
		}
	}

	gtk_drag_finish(context, success, FALSE, time);
}

//-----------------------------------------------------------------------------

/* on_places_favourites_drag_data_received:
 * @context: active GTK drag context to finish.
 * @data: selected file/folder URI payload.
 * @info: target info emitted by the source.
 * @time: GTK event timestamp for gtk_drag_finish().
 *
 * Revalidates the dropped URI before appending it to MeowMenu-owned Places
 * favourites. Thunar-synced bookmarks never reach this path as writable
 * destinations because target registration is disabled for that mode.
 */
void WhiskerMenu::Window::on_places_favourites_drag_data_received(
		GdkDragContext* context, GtkSelectionData* data, guint info, guint time)
{
	GtkWidget* target = m_places_fav_btn->get_widget();
	gtk_drag_unhighlight(target);

	bool success = false;
	if ((info == WHISKERMENU_PLACES_FAVOURITE_DND_INFO)
			&& places_favourites_drop_available()
			&& favourite_drop_accepts(FavouriteDragPayload::Places,
				FavouriteDropTarget::PlacesFavourites))
	{
		const std::string uri = selection_data_to_string(data);
		GFile* file = uri.empty() ? nullptr : g_file_new_for_uri(uri.c_str());
		const bool exists = file && g_file_query_exists(file, nullptr);
		if (exists)
		{
			m_places->get_favourites_section()->add_favourite(uri.c_str());
			m_places->refresh_active();
			success = true;
		}
		if (file)
		{
			g_object_unref(file);
		}
	}

	gtk_drag_finish(context, success, FALSE, time);
}

//-----------------------------------------------------------------------------

WhiskerMenu::Window::~Window()
{
	if (m_resizing)
		interactive_resize_cancel();
	if (m_style_refresh_source != 0)
	{
		g_source_remove(m_style_refresh_source);
		m_style_refresh_source = 0;
	}

	for (int i = 0; i < 9; ++i)
	{
		g_signal_handler_disconnect(m_commands_button[i], m_command_slots[i]);
		gtk_container_remove(GTK_CONTAINER(m_commands_box), m_commands_button[i]);
	}

	if (m_settings && m_settings->channel)
	{
		disconnect_signal(m_settings->channel, m_places_property_slot);
		disconnect_signal(m_settings->channel, m_live_settings_property_slot);
	}

	delete m_applications;
	delete m_places;
	delete m_places_home_btn;
	delete m_places_history_btn;
	delete m_places_fav_btn;
	delete m_search_results;
	delete m_recent;
	delete m_favorites;

	delete m_profile;

	for (Resizer* resizer : m_resize)
	{
		delete resizer;
	}

	if (m_category_width_group)
	{
		g_object_unref(m_category_width_group);
		m_category_width_group = nullptr;
	}


	if (m_css_provider)
	{
		GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(m_window));
		if (screen)
		{
			gtk_style_context_remove_provider_for_screen(
				screen, GTK_STYLE_PROVIDER(m_css_provider));
		}
		g_object_unref(m_css_provider);
	}
	gtk_widget_destroy(GTK_WIDGET(m_window));
	g_object_unref(m_mode_selector_box);
	g_object_unref(m_primary_middle);
	g_object_unref(m_window);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::clear_resize_handles()
{
	for (Resizer* resizer : m_resize)
		resizer->cancel();
}

//-----------------------------------------------------------------------------

/* Window::resize_display_signature:
 * @monitor: active monitor retained for the transaction; never NULL.
 *
 * Captures every display property that defines the transaction's coordinate
 * system and bounds. Any later difference invalidates the gesture.
 *
 * Returns: the current immutable comparison value for @monitor.
 */
InteractiveResize::DisplaySignature
WhiskerMenu::Window::resize_display_signature(GdkMonitor* monitor) const
{
	GdkRectangle geometry = {};
	GdkRectangle workarea = {};
	gdk_monitor_get_geometry(monitor, &geometry);
	gdk_monitor_get_workarea(monitor, &workarea);
	return {
		reinterpret_cast<std::uintptr_t>(monitor),
		{geometry.x, geometry.y, geometry.width, geometry.height},
		{workarea.x, workarea.y, workarea.width, workarea.height},
		gdk_monitor_get_scale_factor(monitor),
		reinterpret_cast<std::uintptr_t>(
				gtk_widget_get_screen(GTK_WIDGET(m_window)))
	};
}

//-----------------------------------------------------------------------------

/* Window::start_resize_display_watch:
 * @monitor: monitor captured by the new active transaction; never NULL.
 *
 * Retains the monitor and observes its properties plus display removal for
 * exactly the transaction lifetime.
 */
void WhiskerMenu::Window::start_resize_display_watch(GdkMonitor* monitor)
{
	stop_resize_display_watch();
	m_resize_monitor = GDK_MONITOR(g_object_ref(monitor));
	m_resize_monitor_notify_slot = connect(
			m_resize_monitor,
			"notify",
			[this](GObject*, GParamSpec*)
			{
				validate_resize_display();
			});

	GdkDisplay* display = gdk_monitor_get_display(m_resize_monitor);
	m_resize_monitor_removed_slot = connect(
			display,
			"monitor-removed",
			[this](GdkDisplay*, GdkMonitor* removed)
			{
				if (removed == m_resize_monitor)
					interactive_resize_cancel();
			});
}

//-----------------------------------------------------------------------------

/* Window::stop_resize_display_watch:
 *
 * Disconnects both active display subscriptions before releasing the retained
 * monitor. Repeated calls are safe.
 */
void WhiskerMenu::Window::stop_resize_display_watch()
{
	if (!m_resize_monitor)
		return;

	GdkDisplay* display = gdk_monitor_get_display(m_resize_monitor);
	disconnect_signal(display, m_resize_monitor_removed_slot);
	disconnect_signal(m_resize_monitor, m_resize_monitor_notify_slot);
	g_object_unref(m_resize_monitor);
	m_resize_monitor = nullptr;
}

//-----------------------------------------------------------------------------

/* Window::validate_resize_display:
 *
 * Compares the retained monitor with the frozen transaction signature and
 * cancels before more geometry can be accepted when any property changed.
 */
void WhiskerMenu::Window::validate_resize_display()
{
	if (!m_resizing || !m_resize_monitor)
		return;

	if (!m_resize_transaction.display_matches(
			resize_display_signature(m_resize_monitor)))
	{
		interactive_resize_cancel();
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::hide(bool lost_focus)
{
	if (m_resizing)
		interactive_resize_cancel();

	// Persist only while remembering is enabled. A disabled preference leaves
	// the saved value intact so it can be ignored without destructive writes.
	const char* mode = mode_to_persist(m_settings->places_enabled,
			m_settings->places_remember_last_mode,
			m_places_active ? MenuMode::Places : MenuMode::Applications);
	if (mode)
	{
		m_settings->places_last_mode = mode;
	}

	// Save settings
	m_settings->favorites.save();
	m_settings->places_favourites.save();
	m_settings->recent.save();

	gtk_drag_unhighlight(m_favorites->get_button()->get_widget());
	gtk_drag_unhighlight(m_places_fav_btn->get_widget());

	// Scroll categories to top
	GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(m_sidebar);
	gtk_adjustment_set_value(adjustment, gtk_adjustment_get_lower(adjustment));

	// Hide command buttons to remove active border
	for (auto command : m_commands_button)
	{
		gtk_widget_set_visible(command, false);
	}

	// Hide window
	gtk_widget_hide(GTK_WIDGET(m_window));

	// Switch back to default page
	show_default_page();

	// Inform plugin that window is hidden
	if (!lost_focus)
	{
		m_plugin->menu_hidden();
	}
}

//-----------------------------------------------------------------------------

int WhiskerMenu::Window::get_result_toplevel_width_authority() const
{
	if (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0)
		return m_workarea.width;
	int requested_width = -1;
	gtk_widget_get_size_request(GTK_WIDGET(m_window), &requested_width, nullptr);
	return requested_width;
}

//-----------------------------------------------------------------------------

int WhiskerMenu::Window::get_result_viewport_width_cap() const
{
	if (g_strcmp0(m_settings->layout_mode, "fullscreen") != 0
			|| m_workarea.width <= 0)
	{
		return -1;
	}
	return meow_fullscreen_main_column(m_workarea.width).width;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show(const Position position)
{
	m_position = position;

	// Reset tracked size so set_size always applies the correct dimensions.
	// Without this, stale geometry from a previous mode (e.g. fullscreen) could
	// prevent set_size from triggering when switching back to a normal preset.
	m_geometry.width = 0;
	m_geometry.height = 0;

	// Handle switching view types
	m_search_results->update_view();
	m_favorites->update_view();
	m_recent->update_view();
	m_applications->update_view();
	m_places->reload_view();
	update_view_redraw_safeguards();

	// Handle showing tooltips
	if (m_settings->launcher_show_tooltip)
	{
		m_search_results->get_view()->show_tooltips();
		m_favorites->get_view()->show_tooltips();
		m_recent->get_view()->show_tooltips();
		m_applications->get_view()->show_tooltips();
		m_places->get_view()->show_tooltips();
	}
	else
	{
		m_search_results->get_view()->hide_tooltips();
		m_favorites->get_view()->hide_tooltips();
		m_recent->get_view()->hide_tooltips();
		m_applications->get_view()->hide_tooltips();
		m_places->get_view()->hide_tooltips();
	}
	m_profile->reset_tooltip();

	// Make sure recent item count is within max
	m_recent->enforce_item_count();

	// Make sure applications list is current; does nothing unless list has changed
	if (m_applications->load())
	{
		set_loaded();
	}
	else if (!m_applications->has_publication())
	{
		m_plugin->set_loaded(false);
		gtk_stack_set_visible_child_name(m_window_stack, "load");
		gtk_spinner_start(m_window_load_spinner);
	}
	else
	{
		// Reload retains the complete published graph until its replacement is
		// ready; only a true cold load presents the spinner.
		m_plugin->set_loaded(true);
		gtk_stack_set_visible_child_name(m_window_stack, "contents");
	}

	// Update default page
	reset_default_button();
	// Commit the opening mode only after view recreation, category preparation,
	// and default-button ordering. No later opening step may select content from
	// the other top-level mode.
	apply_menu_mode(resolve_opening_mode(m_settings->places_enabled,
			m_settings->places_remember_last_mode,
			m_settings->places_last_mode),
			MenuModeTransition::Enter);

	// Clear any previous selection
	m_favorites->reset_selection();
	m_recent->reset_selection();
	m_applications->reset_selection();

	// Make sure icon sizes are correct. The Places section buttons are reloaded
	// on the SAME trigger as the Apps category buttons so sidebar label
	// visibility (names vs icon-only) is identical in both modes — both consult
	// the one shared decision in CategoryButton::reload_icon_size (supported behavior).
	m_favorites->get_button()->reload_icon_size();
	m_recent->get_button()->reload_icon_size();
	m_applications->get_button()->reload_icon_size();
	m_places_home_btn->reload_icon_size();
	m_places_history_btn->reload_icon_size();
	const int category_icon_px = m_settings->category_icon_size.get_size();
	m_places_fav_btn->reload_icon_size(
			meow_favourites_icon_render_size(category_icon_px),
			category_icon_px);

	m_applications->reload_category_icon_size();

	m_search_results->get_view()->reload_icon_size();
	m_favorites->get_view()->reload_icon_size();
	m_recent->get_view()->reload_icon_size();
	m_applications->get_view()->reload_icon_size();
	m_places->get_view()->reload_icon_size();

	GdkMonitor* monitor_gdk = nullptr;
	// Centered always references the panel button's monitor (supported behavior), so it takes
	// the button-detection branch even when launched by a keyboard shortcut
	// (which would otherwise pick the cursor's monitor).
	if (position == PositionAtButton || centered_layout())
	{
		// Wait up to half a second for auto-hidden panels to be shown
		int parent_x = 0, parent_y = 0;
		clock_t end = clock() + (CLOCKS_PER_SEC / 2);
		GtkWidget* parent = m_plugin->get_button();
		GtkWindow* parent_window = GTK_WINDOW(gtk_widget_get_toplevel(parent));
		gtk_window_get_position(parent_window, &parent_x, &parent_y);
		while ((parent_x == -9999) && (parent_y == -9999) && (clock() < end))
		{
			while (gtk_events_pending())
			{
				gtk_main_iteration();
			}
			gtk_window_get_position(parent_window, &parent_x, &parent_y);
		}

		// Fetch position
		m_plugin->get_menu_position(&m_geometry.x, &m_geometry.y);
		monitor_gdk = gdk_display_get_monitor_at_window(gtk_widget_get_display(parent), gtk_widget_get_window(parent));
	}
	else
	{
		// Fetch cursor position
		GdkDisplay* display = gdk_display_get_default();
		GdkSeat* seat = gdk_display_get_default_seat(display);
		GdkDevice* device = gdk_seat_get_pointer(seat);
		gdk_device_get_position(device, nullptr, &m_geometry.x, &m_geometry.y);
		monitor_gdk = gdk_display_get_monitor_at_point(display, m_geometry.x, m_geometry.y);
	}

	// Fall back to a usable monitor if detection failed — e.g. the panel
	// button's monitor was just disconnected — so geometry/centering never
	// dereferences a null monitor or lands off-screen.
	if (monitor_gdk == nullptr)
	{
		GdkDisplay* display = gdk_display_get_default();
		monitor_gdk = gdk_display_get_primary_monitor(display);
		if (monitor_gdk == nullptr)
			monitor_gdk = gdk_display_get_monitor(display, 0);
	}

	// Resize window if necessary, and also prevent it from being larger than screen
#ifdef HAVE_GTK_LAYER_SHELL
	if (gtk_layer_is_supported())
	{
		gtk_layer_set_monitor(m_window, monitor_gdk);
	}
#endif
	gdk_monitor_get_geometry(monitor_gdk, &m_monitor);
	gdk_monitor_get_workarea(monitor_gdk, &m_workarea);
	bool resized = false;  // set below after fullscreen check

	// Center window if requested
	if (position == PositionAtCenter)
	{
		center_window();
	}

	// Move window
	move_window();

	// Relayout window if necessary.
	const bool layout_ltr = gtk_widget_get_default_direction() != GTK_TEXT_DIR_RTL;

	const char* sidebar_pos  = m_settings->sidebar_position;
	CompositionSidebar sidebar_layout = CompositionSidebar::Hidden;
	if (m_settings->sidebar_enabled)
	{
		if (g_strcmp0(sidebar_pos, "right") == 0)
			sidebar_layout = CompositionSidebar::Right;
		else if (g_strcmp0(sidebar_pos, "horizontal") == 0)
			sidebar_layout = CompositionSidebar::Horizontal;
		else
			sidebar_layout = CompositionSidebar::Left;
	}
	const bool cats_horizontal = sidebar_layout
			== CompositionSidebar::Horizontal;
	const bool sidebar_enabled = m_settings->sidebar_enabled;
	const bool is_fullscreen = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);
	// Reconcile once before every presentation. The transaction is bounded and
	// idempotent, so no parallel cache predicate can omit a new layout input.
	m_layout_ltr = layout_ltr;
	m_layout_categories_horizontal = cats_horizontal;
	m_layout_sidebar_position = sidebar_layout;
	m_layout_sidebar_enabled = sidebar_enabled;
	if (m_profile_shape != m_settings->profile_shape)
	{
		m_profile->update_picture();
		m_profile_shape = m_settings->profile_shape;
	}
	update_layout();
	// Reset after mode visibility and fixed-button ordering settle. Resetting
	// only while closing can be invalidated by the next opening's reparent and
	// size negotiation, leaving Favourites/Recent/All Applications above the
	// first visible viewport.
	if (!is_fullscreen && sidebar_enabled && !cats_horizontal)
		meow_reset_vertical_sidebar_scroll(m_sidebar);

	// Sidebar visibility now follows the Enable-sidebar switch (supported behavior);
	// update_layout() owns the in-strip/relocated cases, this is the docked
	// vertical-sidebar show/hide. (Legacy "hidden" position migrated away.)
	gtk_widget_set_visible(GTK_WIDGET(m_sidebar),
			sidebar_enabled && !cats_horizontal);

	// Apply mode-dependent child size requests *before* resizing the toplevel.
	// This prevents stale fullscreen requests from forcing docked presets wider
	// than their configured menu-width when switching back from FullScreen.
	if (is_fullscreen)
	{
		// Centre the search bar in the shared Full Screen column. FILL plus
		// symmetric side margins pin the column to that exact width regardless
		// of the search entry's natural width or whether the Apps/Places switch
		// shares the row, so entry + switch together never grow past the width the
		// entry alone occupies with Places off. A bare halign CENTER would instead
		// grow to the children's natural width and let the switch widen the row.
		const FullscreenMainColumn column =
				meow_fullscreen_main_column(m_workarea.width);
		gtk_widget_set_halign(GTK_WIDGET(m_primary_middle), GTK_ALIGN_FILL);
		gtk_widget_set_size_request(GTK_WIDGET(m_primary_middle), column.width, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_primary_middle), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_primary_middle), 0);
		// Full-screen strip parity: the Horizontal category strip shares the
		// results/application column. Identical FILL + margins put the category
		// icons on the same edges as the results box. Set on every relayout so
		// docked/full-screen and orientation flips reflow with no stale margins.
		gtk_widget_set_halign(GTK_WIDGET(m_categories_box), GTK_ALIGN_FILL);
		gtk_widget_set_margin_start(GTK_WIDGET(m_categories_box), column.margin);
		gtk_widget_set_margin_end(GTK_WIDGET(m_categories_box), column.margin);
		// Keep sidebar width meaningful, and compensate on the opposite side so
		// the applications grid stays centered.
		// NOTE (supported behavior): the symmetry margin is a fixed fraction of the work
		// area and is deliberately independent of the icon/category-name toggles
		// and the relocated switch. With no sidebar, the switch belongs to the
		// fixed primary row rather than the results grid, so it cannot perturb
		// this space. Keep both invariants when editing.
		const int sidebar_width = m_workarea.width / 6;
		const bool sidebar_at_logical_start = m_layout_ltr
				== (m_layout_sidebar_position == CompositionSidebar::Left);
		gtk_widget_set_size_request(GTK_WIDGET(m_sidebar), sidebar_width, -1);
		// The one-sided compensating margin is correct *only* when a vertical
		// sidebar actually occupies one side. The sidebar is shown only when it is
		// enabled and not laid out as a Horizontal strip; in every other case
		// (sidebar disabled, or a horizontal strip) no widget fills that side, so a
		// one-sided margin would shove the results box off-centre — exactly the
		// "no void on the left" symptom. Mirror the margin on both sides there to
		// keep the grid centred with symmetric voids, aligned with the centred
		// search bar (supported behavior).
		const bool vertical_sidebar_visible = sidebar_enabled && !cats_horizontal;
		if (vertical_sidebar_visible)
		{
			gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack),
					sidebar_at_logical_start ? 0 : sidebar_width);
			gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack),
					sidebar_at_logical_start ? sidebar_width : 0);
		}
		else
		{
			gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack), sidebar_width);
			gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack), sidebar_width);
		}
	}
	else
	{
		gtk_widget_set_halign(GTK_WIDGET(m_primary_middle), GTK_ALIGN_FILL);
		gtk_widget_set_size_request(GTK_WIDGET(m_primary_middle), -1, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_primary_middle), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_primary_middle), 0);
		gtk_widget_set_size_request(GTK_WIDGET(m_sidebar), -1, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack), 0);
		// Docked and Centered: the Horizontal category strip fills the menu width;
		// symmetric spacers center its category group.
		gtk_widget_set_halign(GTK_WIDGET(m_categories_box), GTK_ALIGN_FILL);
		gtk_widget_set_margin_start(GTK_WIDGET(m_categories_box), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_categories_box), 0);
	}

	refresh_theme_metrics();
	update_background_css();

	// Resize window according to current layout mode
	if (is_fullscreen)
	{
#ifdef HAVE_GTK_LAYER_SHELL
		if (gtk_layer_is_supported())
		{
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_TOP,    true);
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_BOTTOM, true);
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_LEFT,   true);
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_RIGHT,  true);
			gtk_layer_set_exclusive_zone(m_window, -1);
			for (int edge = 0; edge < 4; ++edge)
				gtk_layer_set_margin(m_window, static_cast<GtkLayerShellEdge>(edge), 0);
		}
		else
#endif
		{
			resized = set_size(m_workarea.width, m_workarea.height);
			m_geometry.x = m_workarea.x;
			m_geometry.y = m_workarea.y;
		}
	}
	else
	{
#ifdef HAVE_GTK_LAYER_SHELL
		if (gtk_layer_is_supported())
		{
			// Restore to default anchors
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_TOP,    true);
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_BOTTOM, false);
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_LEFT,   true);
			gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_RIGHT,  false);
		}
#endif
		resized = set_size(m_settings->menu_width, m_settings->menu_height);
	}

	// Show window
	gtk_window_present(m_window);
	// Presenting can map a model that was populated while the window was hidden.
	// One complete frame makes the first visible result independent of hover or
	// category damage; GTK coalesces this with the normal map invalidation.
	meow::meowmenu_queue_complete_window_frame(GTK_WIDGET(m_window));

	if (resized)
	{
		check_scrollbar_needed();
	}

	// Fetch position again to make sure window does not overlap panel.
	// Centered overrides the launch trigger: it always re-centres on the target
	// monitor using the now-final size (full monitor geometry, gap suppressed in
	// move_window()).
	if (centered_layout())
	{
		center_window();
	}
	else if (position == PositionAtButton)
	{
		m_plugin->get_menu_position(&m_geometry.x, &m_geometry.y);
	}
	else if (position == PositionAtCenter)
	{
		center_window();
	}

	// Move window
	move_window();
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_child_has_focus()
{
	m_child_has_focus = true;
}

//-----------------------------------------------------------------------------

/* Window::detach_categories:
 *
 * Ends the dynamic application-category borrow epoch before ApplicationsPage
 * deletes or replaces the owning Category objects. The window only borrows
 * these widgets/buttons, so it removes them from GTK containers and size groups
 * but never destroys them; CategoryButton remains responsible for destruction.
 */
void WhiskerMenu::Window::detach_categories()
{
	detach_category_widgets(m_category_width_group, m_app_category_widgets);
	m_app_categories.clear();

	sync_category_label_width();
}

//-----------------------------------------------------------------------------

/* Window::refresh_layout:
 *
 * Applies current layout and presentation settings to the existing menu window
 * without changing the application/category content epoch. A visible menu
 * reconciles its existing hierarchy directly; a hidden menu waits until show.
 */
void WhiskerMenu::Window::refresh_layout()
{
	// Layout and presentation define the transaction's anchors and bounds.
	// Changing either invalidates the active gesture rather than mixing states.
	if (m_resizing)
		interactive_resize_cancel();

	schedule_style_refresh();
	m_search_results->refresh_calculator_presentation();

	if (gtk_widget_get_visible(GTK_WIDGET(m_window)))
	{
		update_layout();
		gtk_widget_queue_resize(GTK_WIDGET(m_window));
		gtk_widget_queue_draw(GTK_WIDGET(m_window));
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_categories(const std::vector<CategoryButton*>& categories)
{
	const bool favorites_active = m_favorites->get_button()->get_active();
	const bool recent_active = m_recent->get_button()->get_active();
	const bool all_active = m_applications->get_button()->get_active();
	detach_categories();

	CategoryButton* last_button = m_applications->get_button();
	m_app_categories = categories;
	for (auto button : categories)
	{
		button->join_group(last_button);
		last_button = button;
		gtk_box_pack_start(m_category_buttons, button->get_widget(), false, false, 0);
		m_app_category_widgets.push_back(button->get_widget());
		if (m_category_width_group)
		{
			gtk_size_group_add_widget(m_category_width_group, button->get_widget());
		}
		connect(button->get_widget(), "toggled",
			[this](GtkToggleButton* active_button)
			{
				category_toggled(active_button);
			});
	}
	if (m_layout_categories_horizontal && m_strip_trail_spacer)
	{
		// Category discovery appends new buttons after the trailing spacer;
		// restore the centered strip order before the next allocation pass.
		gtk_box_reorder_child(m_category_buttons, m_strip_trail_spacer, -1);
	}
	const bool windowed_vertical_sidebar = m_layout_sidebar_enabled
			&& !m_layout_categories_horizontal
			&& g_strcmp0(m_settings->layout_mode, "fullscreen") != 0;
	meow_configure_vertical_sidebar_content(
			GTK_WIDGET(m_category_buttons), windowed_vertical_sidebar);

	// Now that the application categories are known, recompute the shared
	// minimum label width so the sidebar floor accounts for them too.
	sync_category_label_width();

	// During the first asynchronous publication, detach_categories() places the
	// stack on the temporary Applications page while dynamic categories are
	// replaced. Re-enter the configured mode after that publication so the
	// initial Favorites/Recent/All page is not lost. Later reloads retain the
	// current page as before.
	const bool publishing_initial_load =
			g_strcmp0(gtk_stack_get_visible_child_name(m_window_stack), "load") == 0;
	if (!publishing_initial_load && !m_places_active)
	{
		// Built-in targets survive a reload. A retired dynamic category has no
		// stable identity, so its unavailable replacement falls back to All.
		if (favorites_active)
			m_favorites->get_button()->set_active(true);
		else if (recent_active)
			m_recent->get_button()->set_active(true);
		else if (all_active)
			m_applications->get_button()->set_active(true);
		else
			m_applications->get_button()->set_active(true);
	}
	apply_menu_mode(m_places_active ? MenuMode::Places
			: MenuMode::Applications,
			publishing_initial_load ? MenuModeTransition::Enter
					: MenuModeTransition::Reevaluate);
	if (gtk_widget_get_visible(GTK_WIDGET(m_window)) && !m_places_active
			&& g_strcmp0(m_settings->layout_mode, "fullscreen") != 0)
	{
		meow_reset_vertical_sidebar_scroll(m_sidebar);
		meow::meowmenu_queue_complete_window_frame(GTK_WIDGET(m_window));
	}
	bind_category_focus_adjustments();
	reveal_active_category();
}

//-----------------------------------------------------------------------------

/* Window::bind_category_focus_adjustments:
 *
 * Rebinds the category focus chain after vertical/horizontal reparenting. The
 * inactive scroller is deliberately left without adjustment ownership.
 */
void WhiskerMenu::Window::bind_category_focus_adjustments()
{
	if (!m_layout_sidebar_enabled)
	{
		gtk_container_set_focus_hadjustment(
				GTK_CONTAINER(m_category_buttons), nullptr);
		gtk_container_set_focus_vadjustment(
				GTK_CONTAINER(m_category_buttons), nullptr);
		return;
	}
	GtkScrolledWindow* presented = m_layout_categories_horizontal
			? m_strip_scroll : m_sidebar;
	meow_bind_navigation_scroller(GTK_CONTAINER(m_category_buttons), presented,
			m_layout_categories_horizontal
					? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::reveal_active_category()
{
	if (!m_layout_sidebar_enabled)
		return;
	GtkWidget* target = gtk_window_get_focus(m_window);
	if (!target || !gtk_widget_is_ancestor(target,
			GTK_WIDGET(m_category_buttons)))
	{
		target = get_active_category_button();
	}
	GtkScrolledWindow* presented = m_layout_categories_horizontal
			? m_strip_scroll : m_sidebar;
	meow_reveal_navigation_widget(presented, target);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::sync_category_label_width()
{
	// Every sidebar button, in both modes. The Places sections and the three
	// built-in Apps buttons always exist; the application categories are added
	// once garcon has loaded them.
	std::vector<CategoryButton*> buttons = {
		m_favorites->get_button(),
		m_recent->get_button(),
		m_applications->get_button(),
		m_places_home_btn,
		m_places_history_btn,
		m_places_fav_btn,
	};
	buttons.insert(buttons.end(), m_app_categories.begin(), m_app_categories.end());

	std::vector<int> widths;
	widths.reserve(buttons.size());
	for (CategoryButton* button : buttons)
	{
		if (button)
		{
			widths.push_back(button->measure_label_width());
		}
	}

	const int floor = meow_sidebar_max_label_width(
			widths.data(), static_cast<int>(widths.size()));

	for (CategoryButton* button : buttons)
	{
		if (button)
		{
			button->set_min_label_width(floor);
		}
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_items()
{
	m_search_results->set_menu_items();
	m_favorites->set_menu_items();
	m_recent->set_menu_items();

	// Handle switching to favorites are added
	connect(m_favorites->get_view()->get_model(), "row-inserted",
		[this](GtkTreeModel*, GtkTreePath*, GtkTreeIter*)
		{
			show_favorites();
		});
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_loaded()
{
	// Hide loading spinner
	gtk_spinner_stop(m_window_load_spinner);
	gtk_stack_set_visible_child_name(m_window_stack, "contents");

	// Focus search entry
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));

	// Show panel button
	m_plugin->set_loaded(true);

	// Check in case of plugin reload
	check_scrollbar_needed();

	// All application-backed models are attached before this point. Invalidate
	// the concrete active result and its composed owner after the stack switch so
	// asynchronous loading cannot publish an empty first frame.
	if (m_places_active)
		m_places->present();
	else if (Page* page = get_active_page())
		page->present();
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::unset_items()
{
	m_search_results->unset_menu_items();
	m_favorites->unset_menu_items();
	m_recent->unset_menu_items();
}

//-----------------------------------------------------------------------------

Keyboard::MenuState WhiskerMenu::Window::current_menu_state() const
{
	const gchar* text = gtk_entry_get_text(m_search_entry);
	return xfce_str_is_empty(text)
			? Keyboard::MenuState::Browsing
			: Keyboard::MenuState::Searching;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::keyboard_navigate_category(GtkWidget* target)
{
	// Keyboard origin: raise the guard so the category `toggled` handlers keep
	// focus on the sidebar button instead of handing off to the search entry,
	// and suppress pointer-hover auto-activation until the next real motion so a
	// stationary pointer resting over the sidebar cannot steal focus back.
	m_keyboard_category_nav = true;
	CategoryButton::suppress_hover_until_motion();

	gtk_widget_grab_focus(target);

	// Activation model derived solely from the existing hover-activation setting
	// (no new preference): hover ON → activate now so results follow the
	// highlight live; hover OFF → move highlight only and leave the committed
	// category untouched until the user presses Enter/Space.
	if (m_settings->category_hover_activate)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(target), true);
	}

	m_keyboard_category_nav = false;
	reveal_active_category();
}

/* dispatch_directional_navigation:
 * @direction: one normalized, unmodified physical arrow.
 *
 * Gives the focused region its internal move first, then scores the current
 * live rectangles of every other eligible region. The bindings are event
 * local so scrolling, layout changes, and asynchronous content cannot leave
 * a stale navigation graph behind.
 *
 * Returns: false when a modal/unknown child should retain normal GTK input;
 * true when the arrow was consumed, including an explicit no-op.
 */
bool WhiskerMenu::Window::dispatch_directional_navigation(
		Keyboard::PhysicalDirection direction)
{
	auto note_places_departure = [this]()
	{
		if (m_places_active)
			m_places->note_deliberate_navigation();
	};
	GtkWidget* focused = gtk_window_get_focus(m_window);
	if (!focused)
	{
		/* Stable row reparenting can briefly leave GtkWindow without a focus
		 * child. Search is the documented neutral origin, so recover it before
		 * classifying the arrow instead of consuming a dead first keypress. */
		gtk_entry_grab_focus_without_selecting(m_search_entry);
		focused = gtk_window_get_focus(m_window);
		if (!focused)
			return true;
	}
	for (GtkWidget* ancestor = focused; ancestor;
			ancestor = gtk_widget_get_parent(ancestor))
	{
		if (GTK_IS_MENU(ancestor))
			return false;
		if (GTK_IS_WINDOW(ancestor)
				&& ancestor != GTK_WIDGET(m_window)
				&& gtk_window_get_modal(GTK_WINDOW(ancestor)))
			return false;
	}

	Keyboard::NavigationRegion origin_region;
	GtkWidget* origin_widget = nullptr;
	Page* active_page = m_places_active ? nullptr : get_active_page();
	LauncherView* active_view = m_places_active
		? m_places->get_view() : (active_page ? active_page->get_view() : nullptr);
	GtkTreePath* origin_path = nullptr;
	Keyboard::NavigationRect origin_rect;
	bool origin_found = false;
	if (focused == GTK_WIDGET(m_search_entry))
	{
		origin_region = Keyboard::NavigationRegion::Search;
		origin_widget = GTK_WIDGET(m_search_entry);
		origin_found = widget_navigation_rect(origin_widget,
				GTK_WIDGET(m_window), &origin_rect);
	}
	else
	{
		if (!m_places_active && active_page == m_search_results
				&& m_search_results->has_calculator_result())
		{
			GtkWidget* calculator_focus =
					m_search_results->get_preferred_focus_widget();
			bool in_calculator = false;
			for (GtkWidget* ancestor = focused; ancestor;
					ancestor = gtk_widget_get_parent(ancestor))
			{
				if (ancestor == calculator_focus)
				{
					in_calculator = true;
					break;
				}
			}
			if (in_calculator)
			{
				origin_region = Keyboard::NavigationRegion::Results;
				origin_widget = calculator_focus;
				origin_found = widget_navigation_rect(calculator_focus,
						GTK_WIDGET(m_window), &origin_rect);
			}
		}
		if (origin_found)
		{
			/* Calculator is a separate first-result banner, so it has no
			 * model path. Its vertical neighbours are Search and the first
			 * ordinary result; horizontal arrows use live region geometry. */
		}
		else
		{
		for (GtkWidget* ancestor = focused; ancestor;
				ancestor = gtk_widget_get_parent(ancestor))
		{
			if ((m_sidebar && ancestor == GTK_WIDGET(m_sidebar))
					|| (m_categories_box
						&& ancestor == GTK_WIDGET(m_categories_box))
					|| (m_category_buttons
						&& ancestor == GTK_WIDGET(m_category_buttons)))
			{
				origin_region = Keyboard::NavigationRegion::Sidebar;
				origin_widget = focused;
				origin_found = widget_navigation_rect(focused,
						GTK_WIDGET(m_window), &origin_rect);
				break;
			}
			if (m_commands_box && ancestor == GTK_WIDGET(m_commands_box))
			{
				origin_region = Keyboard::NavigationRegion::SessionControls;
				origin_widget = focused;
				origin_found = widget_navigation_rect(focused,
						GTK_WIDGET(m_window), &origin_rect);
				break;
			}
			if (m_panels_stack && ancestor == GTK_WIDGET(m_panels_stack))
			{
				origin_region = Keyboard::NavigationRegion::Results;
				origin_widget = focused;
				if (active_view)
				{
					origin_path = active_view->get_selected_path();
					if (!origin_path)
						origin_path = active_view->get_cursor();
					if (origin_path)
						origin_found = active_view->get_path_rectangle(
								origin_path, &origin_rect);
					if (!origin_found)
						origin_found = widget_navigation_rect(
								active_view->get_widget(), GTK_WIDGET(m_window),
								&origin_rect);
				}
				break;
			}
		}
		}
	}

	if (!origin_found)
	{
		if (origin_path)
			gtk_tree_path_free(origin_path);
		return false;
	}

	/* Search's horizontal text editing is the first internal owner. */
	if (origin_region == Keyboard::NavigationRegion::Search
			&& (direction == Keyboard::PhysicalDirection::Left
					|| direction == Keyboard::PhysicalDirection::Right))
	{
		gint start = 0;
		gint end = 0;
		const bool has_selection = gtk_editable_get_selection_bounds(
			GTK_EDITABLE(m_search_entry), &start, &end);
		const gint position = gtk_editable_get_position(
			GTK_EDITABLE(m_search_entry));
		const gint length = gtk_entry_get_text_length(m_search_entry);
		const bool rtl = gtk_widget_get_direction(GTK_WIDGET(m_search_entry))
				== GTK_TEXT_DIR_RTL;
		const bool can_move = has_selection
			|| (direction == Keyboard::PhysicalDirection::Left
					? (rtl ? position < length : position > 0)
					: (rtl ? position > 0 : position < length));
		if (can_move)
		{
			gtk_tree_path_free(origin_path);
			return false;
		}
	}

	if (origin_region == Keyboard::NavigationRegion::Results && active_view)
	{
		bool moved = false;
		const bool calculator_origin =
				Keyboard::is_calculator_navigation_origin(
						m_search_results->has_calculator_result(),
						origin_widget
								== m_search_results->get_preferred_focus_widget());
		if (calculator_origin && direction == Keyboard::PhysicalDirection::Up)
		{
			gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
			moved = true;
		}
		else if (calculator_origin
				&& direction == Keyboard::PhysicalDirection::Down)
		{
			moved = active_page && active_page->focus_first_result();
		}
		else if (active_page && origin_path)
		{
			const bool first_row = active_page == m_search_results
					&& m_search_results->has_calculator_result()
					&& active_view->is_first_visual_row(origin_path);
			if (first_row && direction == Keyboard::PhysicalDirection::Up)
			{
				gtk_widget_grab_focus(
						m_search_results->get_preferred_focus_widget());
				moved = true;
			}
			else
				moved = active_page->keyboard_move(direction);
		}
		else if (active_page && !calculator_origin)
			moved = active_page->keyboard_move(direction);
		else if (origin_path)
		{
			GtkTreePath* target = active_view->get_directional_path(
					origin_path, direction);
			if (target)
			{
				moved = active_view->apply_keyboard_target(target);
				gtk_tree_path_free(target);
			}
		}
		if (moved)
		{
			note_places_departure();
			gtk_tree_path_free(origin_path);
			return true;
		}
	}

	/* Sidebar and Session controls have an internal physical order. */
	if (origin_region == Keyboard::NavigationRegion::Sidebar
			&& GTK_IS_RADIO_BUTTON(origin_widget))
	{
		GtkWidget* parent = gtk_widget_get_parent(origin_widget);
		GList* siblings = parent && GTK_IS_CONTAINER(parent)
			? focusable_category_siblings(GTK_CONTAINER(parent)) : nullptr;
		if (siblings)
		{
			const bool vertical = !GTK_IS_ORIENTABLE(parent)
					|| gtk_orientable_get_orientation(GTK_ORIENTABLE(parent))
							== GTK_ORIENTATION_VERTICAL;
			const bool along = vertical
					? (direction == Keyboard::PhysicalDirection::Up
							|| direction == Keyboard::PhysicalDirection::Down)
					: (direction == Keyboard::PhysicalDirection::Left
							|| direction == Keyboard::PhysicalDirection::Right);
			if (along)
			{
				std::vector<GtkWidget*> category_widgets;
				std::vector<Keyboard::FocusTarget> category_targets;
				for (GList* sibling = siblings; sibling; sibling = sibling->next)
				{
					GtkWidget* category = GTK_WIDGET(sibling->data);
					Keyboard::NavigationRect rectangle;
					if (!widget_navigation_rect(category,
							GTK_WIDGET(m_window), &rectangle))
						continue;
					Keyboard::FocusTarget target;
					target.target_id = category_widgets.size();
					target.kind = Keyboard::FocusTargetKind::CategoryButton;
					target.region = Keyboard::NavigationRegion::Sidebar;
					target.rectangle = rectangle;
					target.visual_ordinal = category_widgets.size();
					target.usable = true;
					category_widgets.push_back(category);
					category_targets.push_back(target);
				}
				const std::size_t selected = Keyboard::choose_spatial_target(
						origin_rect, direction, category_targets,
						gtk_widget_get_direction(GTK_WIDGET(m_window))
								== GTK_TEXT_DIR_RTL,
						current_menu_state());
				if (selected != Keyboard::NO_TARGET)
				{
					keyboard_navigate_category(category_widgets[selected]);
					note_places_departure();
				}
				g_list_free(siblings);
				gtk_tree_path_free(origin_path);
				return true;
			}
			g_list_free(siblings);
		}
	}

	if (origin_region == Keyboard::NavigationRegion::SessionControls)
	{
		std::vector<GtkWidget*> buttons;
		GList* children = gtk_container_get_children(
				GTK_CONTAINER(m_commands_box));
		for (GList* child = children; child; child = child->next)
		{
			GtkWidget* button = GTK_WIDGET(child->data);
			if (button && gtk_widget_get_visible(button)
					&& gtk_widget_get_sensitive(button)
					&& gtk_widget_get_can_focus(button))
				buttons.push_back(button);
		}
		g_list_free(children);
		const auto current = std::find(buttons.begin(), buttons.end(),
				origin_widget);
		const bool horizontal = direction == Keyboard::PhysicalDirection::Left
				|| direction == Keyboard::PhysicalDirection::Right;
		if (horizontal && current != buttons.end())
		{
			std::vector<Keyboard::FocusTarget> button_targets;
			button_targets.reserve(buttons.size());
			for (std::size_t i = 0; i < buttons.size(); ++i)
			{
				Keyboard::NavigationRect rectangle;
				if (!widget_navigation_rect(buttons[i], GTK_WIDGET(m_window),
						&rectangle))
					continue;
				Keyboard::FocusTarget target;
				target.target_id = i;
				target.kind = Keyboard::FocusTargetKind::SessionButton;
				target.region = Keyboard::NavigationRegion::SessionControls;
				target.rectangle = rectangle;
				target.visual_ordinal = i;
				target.usable = true;
				button_targets.push_back(target);
			}
			const std::size_t selected = Keyboard::choose_spatial_target(
					origin_rect, direction, button_targets,
					gtk_widget_get_direction(GTK_WIDGET(m_window))
							== GTK_TEXT_DIR_RTL,
					current_menu_state());
			if (selected != Keyboard::NO_TARGET
					&& button_targets[selected].target_id < buttons.size())
			{
				gtk_widget_grab_focus(
						buttons[button_targets[selected].target_id]);
				note_places_departure();
			}
			gtk_tree_path_free(origin_path);
			return true;
		}
	}

	struct Binding
	{
		Keyboard::FocusTarget target;
		GtkWidget* widget = nullptr;
		LauncherView* view = nullptr;
		GtkTreePath* path = nullptr;
	};
	std::vector<Binding> bindings;
	const Keyboard::MenuState state = current_menu_state();
	const bool sidebar_targets_allowed =
			Keyboard::allows_results_sidebar_exit(state, origin_region, direction);
	auto add_widget = [&](Keyboard::NavigationRegion region,
			Keyboard::FocusTargetKind kind, GtkWidget* target_widget)
	{
		Keyboard::NavigationRect rectangle;
		if (!target_widget || !widget_navigation_rect(target_widget,
				GTK_WIDGET(m_window), &rectangle)
			|| !gtk_widget_get_sensitive(target_widget)
			|| !gtk_widget_get_can_focus(target_widget))
			return;
		Binding binding;
		binding.target.target_id = bindings.size();
		binding.target.region = region;
		binding.target.kind = kind;
		binding.target.rectangle = rectangle;
		binding.target.visual_ordinal = bindings.size();
		binding.target.usable = true;
		binding.widget = target_widget;
		bindings.push_back(binding);
	};
	auto add_result = [&](LauncherView* view, GtkTreePath* path)
	{
		if (!view || !path)
			return;
		Keyboard::NavigationRect rectangle;
		if (!view->get_path_rectangle(path, &rectangle))
			return;
		Binding binding;
		binding.target.target_id = bindings.size();
		binding.target.region = Keyboard::NavigationRegion::Results;
		binding.target.kind = Keyboard::FocusTargetKind::ResultItem;
		binding.target.rectangle = rectangle;
		binding.target.visual_ordinal = bindings.size();
		binding.target.usable = true;
		binding.view = view;
		binding.path = gtk_tree_path_copy(path);
		bindings.push_back(binding);
	};

	if (origin_region != Keyboard::NavigationRegion::Search)
		add_widget(Keyboard::NavigationRegion::Search,
				Keyboard::FocusTargetKind::SearchEntry, GTK_WIDGET(m_search_entry));
	if (origin_region != Keyboard::NavigationRegion::Sidebar
			&& sidebar_targets_allowed && m_category_buttons)
	{
		GList* children = gtk_container_get_children(
				GTK_CONTAINER(m_category_buttons));
		for (GList* child = children; child; child = child->next)
		{
			GtkWidget* category = GTK_WIDGET(child->data);
			if (GTK_IS_RADIO_BUTTON(category))
				add_widget(Keyboard::NavigationRegion::Sidebar,
						Keyboard::FocusTargetKind::CategoryButton, category);
		}
		g_list_free(children);
	}
	if (origin_region != Keyboard::NavigationRegion::SessionControls)
	{
		GList* children = gtk_container_get_children(
				GTK_CONTAINER(m_commands_box));
		for (GList* child = children; child; child = child->next)
			add_widget(Keyboard::NavigationRegion::SessionControls,
					Keyboard::FocusTargetKind::SessionButton,
					GTK_WIDGET(child->data));
		g_list_free(children);
	}
	if (origin_region != Keyboard::NavigationRegion::Results && active_view)
	{
		struct ResultPaths
		{
			std::vector<GtkTreePath*> paths;
		};
		ResultPaths result_paths;
		GtkTreeModel* model = active_view->get_model();
		if (model)
		{
			gtk_tree_model_foreach(model,
					[](GtkTreeModel*, GtkTreePath* path, GtkTreeIter*,
							gpointer data) -> gboolean
					{
						static_cast<ResultPaths*>(data)->paths.push_back(
								gtk_tree_path_copy(path));
						return FALSE;
					}, &result_paths);
		}
		for (GtkTreePath* path : result_paths.paths)
		{
			add_result(active_view, path);
			gtk_tree_path_free(path);
		}
	}
	if (origin_region != Keyboard::NavigationRegion::Results
			&& !m_places_active && m_search_results->has_calculator_result())
		add_widget(Keyboard::NavigationRegion::Results,
				Keyboard::FocusTargetKind::CalculatorBanner,
				m_search_results->get_preferred_focus_widget());

	std::vector<Keyboard::FocusTarget> targets;
	for (const Binding& binding : bindings)
		targets.push_back(binding.target);
	const std::size_t selected = Keyboard::choose_spatial_target(origin_rect,
		direction, targets,
		gtk_widget_get_direction(GTK_WIDGET(m_window)) == GTK_TEXT_DIR_RTL,
		sidebar_targets_allowed ? Keyboard::MenuState::Browsing : state);
	if (selected != Keyboard::NO_TARGET)
	{
		Binding& binding = bindings[selected];
		if (binding.view && binding.path)
			binding.view->apply_keyboard_target(binding.path);
		else if (binding.widget)
		{
			if (binding.target.kind == Keyboard::FocusTargetKind::CategoryButton)
				keyboard_navigate_category(binding.widget);
			else
				gtk_widget_grab_focus(binding.widget);
		}
		note_places_departure();
	}
	for (Binding& binding : bindings)
		gtk_tree_path_free(binding.path);
	gtk_tree_path_free(origin_path);
	return true;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_key_press_event(GtkWidget* widget, GdkEventKey* key_event)
{
	// Type-to-search catch-all (supported behavior, supported behavior, supported behavior, supported behavior). This
	// runs BEFORE GTK's default key chain so that:
	//   - Printable keys typed while focus is on the results view do
	//     not reach the view's default Space-activates-row handler.
	//   - Each character is delivered to the entry exactly once (the
	//     post-default signal must not also re-route, otherwise every
	//     keystroke would be inserted twice once Window::search()
	//     moves focus to the view).
	// Backspace gets a dedicated branch (supported behavior): it must remove a
	// character from the query regardless of which zone holds focus,
	// but it is not classified Printable (control codepoint) and
	// therefore cannot ride the generic printable redirect.
	GtkWidget* entry = GTK_WIDGET(m_search_entry);
	GtkWidget* focused = gtk_window_get_focus(m_window);
	bool child_has_input_priority = false;
	for (GtkWidget* ancestor = focused; ancestor;
			ancestor = gtk_widget_get_parent(ancestor))
	{
		if (GTK_IS_MENU(ancestor)
				|| (GTK_IS_WINDOW(ancestor)
					&& ancestor != GTK_WIDGET(m_window)
					&& gtk_window_get_modal(GTK_WINDOW(ancestor))))
		{
			child_has_input_priority = true;
			break;
		}
	}
	if (Keyboard::should_recover_search_focus(focused != nullptr,
			child_has_input_priority))
	{
		/* GTK may clear the focus child between a stable-row reparent and map.
		 * Recover Search early so the current event follows its ordinary entry
		 * path and is neither lost nor delivered twice. */
		gtk_entry_grab_focus_without_selecting(m_search_entry);
		focused = gtk_window_get_focus(m_window);
	}
	if (focused && !child_has_input_priority
			&& Keyboard::is_query_space_key(key_event->keyval)
			&& !(key_event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK
					| GDK_SUPER_MASK | GDK_META_MASK | GDK_HYPER_MASK)))
	{
		/* Space is a query character even when a result or button owns focus.
		 * Insert it through the editable API so no focused control sees its
		 * activation binding and no input-method candidate is advanced. */
		if (focused != entry)
			gtk_entry_grab_focus_without_selecting(m_search_entry);
		GtkEditable* editable = GTK_EDITABLE(m_search_entry);
		gint position = gtk_editable_get_position(editable);
		gtk_editable_delete_selection(editable);
		gtk_editable_insert_text(editable, " ", 1, &position);
		gtk_editable_set_position(editable, position);
		return GDK_EVENT_STOP;
	}
	if (focused && !child_has_input_priority && focused != entry)
	{
		if (Keyboard::is_search_text_event(key_event))
		{
			gtk_search_entry_handle_event(GTK_SEARCH_ENTRY(m_search_entry),
					reinterpret_cast<GdkEvent*>(key_event));
			return GDK_EVENT_STOP;
		}
		if (key_event->keyval == GDK_KEY_BackSpace)
		{
			if (xfce_str_is_empty(gtk_entry_get_text(m_search_entry)))
				return GDK_EVENT_STOP;
			gtk_search_entry_handle_event(GTK_SEARCH_ENTRY(m_search_entry),
					reinterpret_cast<GdkEvent*>(key_event));
			return GDK_EVENT_STOP;
		}
	}

	// Tab family is intercepted before GTK's default focus-chain and split
	// on the Control modifier:
	//   - bare Tab / Shift+Tab        → Applications⇄Places mode toggle
	//                                    (supported behavior, supported behavior, supported behavior);
	//   - Ctrl+Tab / Ctrl+Shift+Tab   → consumed no-op
	// The two are mutually exclusive on GDK_CONTROL_MASK and both always
	// consume the event so GTK's own Tab traversal never also fires.
	if (key_event->keyval == GDK_KEY_Tab
			|| key_event->keyval == GDK_KEY_ISO_Left_Tab
			|| key_event->keyval == GDK_KEY_KP_Tab)
	{
		if (child_has_input_priority)
			return GDK_EVENT_PROPAGATE;
		if (key_event->state & GDK_CONTROL_MASK)
		{
			/* Ctrl+Tab is deliberately consumed. Mode switching is the only
			 * direct Tab transaction; GTK must not revive focus traversal. */
			return GDK_EVENT_STOP;
		}

		// Bare Tab/Shift+Tab toggles the mode (or is inert when Places is
		// unavailable). Tab never falls through to toolkit traversal, so the
		// Inert arm consumes the event and changes nothing.
		switch (Keyboard::tab_action(m_settings->places_enabled))
		{
		case Keyboard::TabAction::ToggleMode:
			switch_mode(!m_places_active);
			if (m_places_active)
			{
				if (!m_places->focus_first_result())
					gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
			}
			else
			{
				Page* destination = get_active_page();
				if (destination)
				{
					if (destination == m_search_results)
						m_search_results->focus_first_visual_result();
					else if (!destination->focus_first_result())
						gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
				}
			}
			break;
		case Keyboard::TabAction::Inert:
			break;
		}
		return GDK_EVENT_STOP;
	}

	if (key_event->keyval == GDK_KEY_Escape)
	{
		// Esc ladder (supported behavior, supported behavior). Strict priority:
		//   ContextMenuOpen > ResizeInProgress > QueryNonEmpty > MenuOpen.
		// Each press peels one layer; Backspace/Delete MUST NOT close the
		// menu (the explicit BackSpace branch above only redirects to the
		// entry; the Esc ladder is the sole keyboard close path).
		GtkWidget* esc_focused = gtk_window_get_focus(m_window);
		bool context_menu_open = false;
		for (GtkWidget* w = esc_focused; w; w = gtk_widget_get_parent(w))
		{
			if (GTK_IS_MENU(w))
			{
				context_menu_open = true;
				break;
			}
		}
		const bool query_non_empty
				= !xfce_str_is_empty(gtk_entry_get_text(m_search_entry));

		const Keyboard::EscState st = Keyboard::classify_esc_state(
				context_menu_open, m_resizing, query_non_empty);
		switch (Keyboard::esc_action(st))
		{
		case Keyboard::EscAction::CloseContextMenu:
			// Forward the Esc into the active menu shell so it closes
			// just that layer and returns focus to the originating
			// launcher (supported behavior).
			if (esc_focused)
			{
				for (GtkWidget* w = esc_focused; w; w = gtk_widget_get_parent(w))
				{
					if (GTK_IS_MENU(w))
					{
						gtk_menu_shell_cancel(GTK_MENU_SHELL(w));
						break;
					}
				}
			}
			break;

		case Keyboard::EscAction::CancelResize:
			interactive_resize_cancel();
			break;

		case Keyboard::EscAction::ClearQuery:
			gtk_entry_set_text(m_search_entry, "");
			// supported behavior follow-up: focus returns to the entry so the user is
			// back in Browsing. Window::search() also grabs focus to the
			// entry when the text becomes empty, but make it explicit
			// here so the ladder behaves the same regardless of where
			// focus sat at Esc time.
			gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
			break;

		case Keyboard::EscAction::CloseMenu:
			hide();
			break;
		}
		return GDK_EVENT_STOP;
	}

	Keyboard::PhysicalDirection direction;
	if (Keyboard::normalize_direction(key_event->keyval, &direction))
	{
		/* Modified arrows belong to Search text selection, child menus, or
		 * the toolkit. Only the unmodified event enters the menu router. */
		if (!Keyboard::is_directional_key(key_event))
			return GDK_EVENT_PROPAGATE;
		return dispatch_directional_navigation(direction)
				? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
	}

	Page* page = get_active_page();
	// NOTE: in Places mode get_active_page() returns the hidden applications
	// page; use m_places directly so focus and selection target the visible view.
	GtkWidget* view = m_places_active
		? m_places->get_view()->get_widget()
		: page->get_view()->get_widget();
	GtkWidget* search = GTK_WIDGET(m_search_entry);


	switch (key_event->keyval)
	{
	// Pass PageUp and PageDown keys to current view. supported behavior: when focus
	// was on the search entry and no row was selected yet, also select
	// the first row so the user sees a visual anchor for the subsequent
	// Page motion.
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
		if ((widget == search) || (gtk_window_get_focus(m_window) == search))
		{
			gtk_widget_grab_focus(view);
			LauncherView* results_view = m_places_active
				? m_places->get_view() : page->get_view();
			GtkTreePath* selected = results_view->get_selected_path();
			if (!selected)
			{
				m_places_active ? m_places->select_first() : page->select_first();
			}
			else
			{
				gtk_tree_path_free(selected);
			}
		}
		break;

	default:
		break;
	}

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_key_press_event_after(GtkWidget* /*widget*/, GdkEventKey* /*key_event*/)
{
	// NOTE: type-to-search routing moved to the pre-default handler
	// above so it can pre-empt GtkTreeView/GtkIconView's "Space
	// activates the selected row" default binding (supported behavior). Keeping
	// a duplicate redirect here would cause every keystroke to be
	// inserted twice once Window::search() moves focus to the view
	// per supported behavior. The post-default slot remains attached as a hook
	// point for future expansion; today it is a no-op.
	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

/* Window::on_map_event:
 *
 * Reasserts the complete mode visibility matrix after final GTK mapping and
 * invalidates the composed toplevel once. This keeps the fixed Applications
 * controls independent of pre-map damage and later pointer hover.
 *
 * Returns: GDK_EVENT_PROPAGATE so normal map processing continues.
 */
gboolean WhiskerMenu::Window::on_map_event()
{
	gtk_window_set_keep_above(m_window, true);
	apply_menu_mode(m_places_active ? MenuMode::Places
			: MenuMode::Applications, MenuModeTransition::Reevaluate);
	meow::meowmenu_queue_complete_window_frame(GTK_WIDGET(m_window));

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::on_state_flags_changed(GtkWidget* widget)
{
	// Refocus and raise window if visible; skip when stay-on-focus-out is active
	// so that the menu remains visible without stealing focus from other windows.
	if (gtk_widget_get_visible(widget) && !m_settings->stay_on_focus_out)
	{
		gtk_window_present(m_window);
	}

	schedule_style_refresh();
}

//-----------------------------------------------------------------------------

/* update_view_redraw_safeguards:
 *
 * Pushes the current transparent-surface decision into every Applications and
 * Places result view. Call after view recreation and after live opacity or
 * Transparent grid changes.
 */
void WhiskerMenu::Window::update_view_redraw_safeguards()
{
	LauncherView* views[] = {
		m_search_results->get_view(),
		m_favorites->get_view(),
		m_recent->get_view(),
		m_applications->get_view(),
		m_places->get_view()
	};
	for (LauncherView* view : views)
	{
		view->set_full_redraw_safeguard(full_redraw_safeguard_required(
				m_settings->menu_opacity,
				view->is_grid_view()
						? LauncherViewKind::IconGrid
						: LauncherViewKind::Tree,
				m_settings->transparent_grid));
	}
}

//-----------------------------------------------------------------------------

/* Window::prepare_results_width_resize:
 * @current_width: launcher width before the resize step.
 * @requested_width: launcher width requested by the resize step.
 *
 * Supplies only the visible result page with the horizontal delta before GTK
 * allocates the new window. Hidden GtkIconViews synchronize from their owning
 * scroller when shown and must not perform redundant relayouts during a drag.
 */
void WhiskerMenu::Window::prepare_results_width_resize(int current_width,
		int requested_width)
{
	if (m_places_active)
	{
		m_places->prepare_viewport_resize(current_width,
				requested_width);
	}
	else if (Page* page = get_active_page())
	{
		page->prepare_viewport_resize(current_width,
				requested_width);
	}
}

//-----------------------------------------------------------------------------

/* Window::schedule_style_refresh:
 *
 * Coalesces GTK style invalidations into one idle transaction. Reloading the
 * application CSS provider can itself emit style-updated, so callbacks raised
 * while the transaction is active are deliberately ignored.
 */
void WhiskerMenu::Window::schedule_style_refresh()
{
	if (!meow_style_refresh_should_schedule(m_style_refresh_source,
			m_style_refresh_running))
		return;

	m_style_refresh_source = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
			+[](gpointer data) -> gboolean
			{
				auto* self = static_cast<Window*>(data);
				self->m_style_refresh_source = 0;
				self->m_style_refresh_running = true;
				self->refresh_theme_metrics();
				self->update_background_css();
				self->update_layout();
				gtk_widget_queue_resize(GTK_WIDGET(self->m_window));
				gtk_widget_queue_draw(GTK_WIDGET(self->m_window));
				self->m_style_refresh_running = false;
				return G_SOURCE_REMOVE;
			}, this, nullptr);
}

/* Window::refresh_theme_metrics:
 *
 * Samples normal-state padding from the long-lived Search entry and first
 * Session button. The pure resolver discards unusable values and supplies one
 * bounded rhythm for every major-region boundary.
 */
void WhiskerMenu::Window::refresh_theme_metrics()
{
	int values[8];
	std::size_t count = 0;
	GtkWidget* widgets[] = {
		GTK_WIDGET(m_search_entry),
		m_commands_button[0]
	};
	for (GtkWidget* widget : widgets)
	{
		if (!GTK_IS_WIDGET(widget))
			continue;
		GtkBorder padding = {};
		gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
				GTK_STATE_FLAG_NORMAL, &padding);
		values[count++] = padding.left;
		values[count++] = padding.right;
		values[count++] = padding.top;
		values[count++] = padding.bottom;
	}
	m_layout_metrics = meow_resolve_layout_metrics(values, count);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::update_background_css()
{
	if (!m_css_provider || !m_window)
	{
		return;
	}

	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(m_window));
	// NOTE: the fallback below is recomputed from scratch on every call and
	// never cached, so a live light<->dark theme switch is tracked by the
	// existing callers of this method (on_screen_changed,
	// on_state_flags_changed, the xfconf property-changed handler, and the
	// initial setup) without introducing any new refresh signal.
	GdkRGBA bg = { 0.12, 0.12, 0.12, 1.0 };
	gboolean bg_found = gtk_style_context_lookup_color(context, "theme_bg_color", &bg);
	if (!bg_found)
	{
		bg_found = gtk_style_context_lookup_color(context, "bg_color", &bg);
	}

	if (!bg_found)
	{
		// Both background lookups failed. The old code kept the unconditional
		// dark seed here, so a light theme that omits the key painted a dark
		// menu — the reported defect. Instead infer the theme's lightness from
		// its foreground/text colour, and failing that the prefer-dark setting,
		// then substitute a matching neutral grey for the seed.
		GdkRGBA fg;
		gboolean have_fg = gtk_style_context_lookup_color(context, "theme_fg_color", &fg);
		if (!have_fg)
		{
			have_fg = gtk_style_context_lookup_color(context, "fg_color", &fg);
		}

		gboolean prefer_dark = FALSE;
		g_object_get(gtk_settings_get_default(),
			"gtk-application-prefer-dark-theme", &prefer_dark, nullptr);

		bg = meow_choose_background_fallback(have_fg, &fg, prefer_dark);
	}
	GdkRGBA base = {};
	gboolean base_found = gtk_style_context_lookup_color(
			context, "theme_base_color", &base);
	if (!base_found)
		base_found = gtk_style_context_lookup_color(context, "base_color", &base);
	m_surface_palette = meow_resolve_surface_palette(
			bg_found, &bg, base_found, &base, bg);

	const GdkRGBA& css_background = m_surface_palette.content;
	const int red = CLAMP(static_cast<int>(css_background.red * 255.0 + 0.5), 0, 255);
	const int green = CLAMP(static_cast<int>(css_background.green * 255.0 + 0.5), 0, 255);
	const int blue = CLAMP(static_cast<int>(css_background.blue * 255.0 + 0.5), 0, 255);
	// Separator colour for the single menu border and the secondary-row divider.
	// Seed it from the theme background and nudge it toward or away from black so
	// both lines read on dark and light themes without a literal fixed colour.
	const double sep_luma = 0.299 * red + 0.587 * green + 0.114 * blue;
	const int sep_step = 45;
	const int sep_r = (sep_luma < 128.0) ? CLAMP(red   + sep_step, 0, 255) : CLAMP(red   - sep_step, 0, 255);
	const int sep_g = (sep_luma < 128.0) ? CLAMP(green + sep_step, 0, 255) : CLAMP(green - sep_step, 0, 255);
	const int sep_b = (sep_luma < 128.0) ? CLAMP(blue  + sep_step, 0, 255) : CLAMP(blue  - sep_step, 0, 255);
	// Publish the seed for on_draw_event so the outer silhouette and the one
	// auxiliary-row boundary share the same theme-derived line colour.
	m_separator_rgba.red   = sep_r / 255.0;
	m_separator_rgba.green = sep_g / 255.0;
	m_separator_rgba.blue  = sep_b / 255.0;
	m_separator_rgba.alpha = 1.0;
	// The whole menu reads at exactly one alpha: the single /menu-opacity value
	// maps to a CSS alpha that paints the .meowmenu window shell, and every region
	// inside it stays transparent. There is no per-region model and nothing
	// compounds, so the background fades uniformly in docked, centered, and
	// full-screen modes while all foreground content stays fully opaque.
	const double menu_alpha = meowmenu_effective_background_alpha(
			m_settings->menu_opacity,
			gdk_screen_is_composited(
					gtk_widget_get_screen(GTK_WIDGET(m_window)))
					&& m_supports_alpha);

	// NOTE: the results area no longer emits its own 1 px outline. The single
	// intentional menu border is drawn once in on_draw_event along the rounded
	// clip path (docked only); a per-region CSS border here would double that
	// line and ignore the corner radius. Full-screen continues to show no
	// outline at all (one seamless surface).

	// NOTE: the alpha MUST be serialised locale-independently. A plain "%.3f"
	// honours the process LC_NUMERIC, so under a comma-decimal locale (it_IT,
	// de_DE, …) it would emit "0,600", producing the invalid declaration
	// rgba(31, 31, 31, 0,600) that GTK's CSS parser rejects — the menu's opacity
	// control would then silently do nothing. Format the single alpha with
	// meowmenu_format_css_alpha (always a '.' separator, no global state) and
	// substitute a %s token so the generated CSS is byte-for-byte identical in
	// every language.
	char alpha_shell[MEOWMENU_CSS_ALPHA_BUFSZ];
	meowmenu_format_css_alpha(menu_alpha, alpha_shell);

	gchar* css = g_strdup_printf(
		".meowmenu { background-image: none; background-color: rgba(%d, %d, %d, %s); }"
		// NOTE: GTK wraps an undecorated, client-side-decorated toplevel in a
		// `decoration` subnode that the active theme styles with a drop shadow
		// (and often its own border-radius/border/margin). That node is painted
		// outside everything on_draw_event controls, so it surfaces as a faint
		// translucent halo sitting just beyond the single custom border — the
		// ghost outline supported behavior forbids. Neutralise it so the only thing framing
		// the menu is the one border stroked along the rounded clip path.
		"window.meowmenu decoration,"
		".meowmenu decoration"
		"{ box-shadow: none; border: none; background: transparent;"
		"  margin: 0; padding: 0; border-radius: 0; }"
		// NOTE: the GtkFrame is a structural container only, but GTK3 still
		// renders its themed `border` subnode and reserves that border/padding
		// as an inset. That inset used to hide under the opaque pre-026 shell;
		// the docked shell is now transparent, so the reserved ring shows the
		// desktop through it as a band between the content and the single drawn
		// border. Collapse the frame border/padding so the regions sit flush to
		// the window edge where the border is stroked.
		".meowmenu frame,"
		".meowmenu frame > border"
		"{ border: none; padding: 0; margin: 0;"
		"  min-width: 0; min-height: 0; }"
		// NOTE: every region and descendant stays transparent on purpose. Only the
		// .meowmenu window shell paints a background (at the single menu alpha
		// above); the categories sidebar, the search/title/commands chrome strips,
		// and the applications area never paint their own background, so the whole
		// menu reads at exactly one alpha and nothing compounds over the shell.
		".meowmenu > *,"
		".meowmenu frame,"
		".meowmenu frame > *,"
		".meowmenu stack,"
		".meowmenu stack > *,"
		".meowmenu scrolledwindow,"
		".meowmenu scrolledwindow > *,"
		".meowmenu grid,"
		".meowmenu grid > *,"
		".meowmenu .contents,"
		".meowmenu .contents > *,"
		".meowmenu .categories,"
		".meowmenu .search-area,"
		".meowmenu .title-area,"
		".meowmenu .commands-area,"
		".meowmenu .applications-area,"
		".meowmenu .applications-area > *,"
		".meowmenu treeview,"
		".meowmenu flowbox,"
		".meowmenu flowboxchild,"
		".meowmenu list,"
		".meowmenu row"
		"{ background-color: transparent; background-image: none; }"
		// HACK: enforce the single-highlight invariant. The result lists carry
		// two independent visual highlights — the plugin's GTK single selection
		// AND GTK's own pointer-driven row prelight (:hover /
		// GTK_CELL_RENDERER_PRELIT). A theme that paints both leaves a trail of
		// highlighted rows under keyboard and mouse navigation. Neutralise
		// prelight on the .launchers tree/icon views so the GTK selection stays
		// the ONLY painted highlight. :selected styling is preserved via
		// :not(:selected), and the .meow-focus-ring focus outline below is
		// untouched. These rules are plugin-scoped (class .launchers), not
		// theme-scoped, so the invariant holds identically across every preset
		// and theme (supported behavior).
		".meowmenu .launchers:hover:not(:selected),"
		".meowmenu .launchers cell:hover:not(:selected),"
		".meowmenu .launchers row:hover:not(:selected)"
		"{ background-color: transparent; background-image: none; }"
		// Transparent grid is opt-in and view-scoped. It removes only the
		// resting icon-grid cell box; selected, hover, active, and focus states
		// stay theme-visible so navigation and drag feedback remain legible.
		".meowmenu iconview.launchers.transparent-grid.view.cell:not(:selected):not(:hover):not(:active),"
		".meowmenu .launchers.transparent-grid.cell:not(:selected):not(:hover):not(:active),"
		".meowmenu iconview.launchers.transparent-grid cell:not(:selected):not(:hover):not(:active),"
		".meowmenu .launchers.transparent-grid cell:not(:selected):not(:hover):not(:active)"
		"{ background: none; background-color: transparent;"
		"  background-image: none; border: none; border-color: transparent;"
		"  box-shadow: none; }"
		// Launcher content has no persistent internal frame. The scoped rule also
		// clears theme chrome on the scrollbar container; its slider and every GTK
		// interaction state remain outside the selector.
		"%s"
		// GtkTreeView themes vary widely in how strongly they paint an inactive
		// selection. Pin only selected list rows to the theme's selected tokens so
		// list and grid results have equivalent keyboard/pointer emphasis.
		"%s"
		".meowmenu .category-button,"
		".meowmenu .category-button *,"
		".meowmenu .category-button image,"
		".meowmenu .category-button label"
		"{ opacity: 1; }"
		// Default-category heading shown when the sidebar is disabled
		// (supported behavior). Uppercase text comes from the catalog strings; the
		// letter-spacing/weight here are theme-overridable.
		".meowmenu .meow-default-heading"
		"{ font-weight: bold; letter-spacing: 1px; opacity: 0.65;"
		"  margin: 6px 6px 2px 6px; }"
		// supported behavior / supported behavior: keyboard focus indicator. The 1 px inset
		// outline sits inside the widget border (outline-offset: -1px)
		// so the widget does not change size on focus and the
		// surrounding layout does not shift. :focus-visible restricts
		// the ring to keyboard focus, never mouse click. Themes can
		// override .meow-focus-ring to recolour or thicken the ring.
		".meowmenu .meow-focus-ring:focus-visible,"
		".meowmenu .category-button:focus-visible,"
		".meowmenu entry:focus-visible,"
		".meowmenu treeview:focus-visible,"
		".meowmenu iconview:focus-visible"
		"{ outline: 1px solid @theme_selected_bg_color; outline-offset: -1px; }"
		// The selector has no enclosing track. Idle inactive choices are flat;
		// checked, hover, active, disabled, and focus states stay theme-owned.
		".meowmenu .places-mode-selector"
		"{ background: transparent; border: none; box-shadow: none; }"
		".meowmenu .places-mode-selector button:not(:checked):not(:hover):not(:active):not(:disabled):not(:focus)"
		"{ background: transparent; border-color: transparent; box-shadow: none; }",
		red, green, blue, alpha_shell,
		meow::meowmenu_frameless_launcher_css(),
		meow::meowmenu_list_selection_css());

	// Capture the parse error from our own generated stylesheet. Passing nullptr
	// here discards all diagnostics, which is why a malformed declaration (such
	// as the comma-decimal alpha bug) previously failed silently. The warning is
	// emitted unconditionally — not gated behind any debug flag — so a future
	// "opacity looks broken, no error" report becomes a one-line diagnosis. The
	// success path stays silent, so normal operation adds no log noise.
	GError* error = nullptr;
	gtk_css_provider_load_from_data(m_css_provider, css, -1, &error);
	if (error != nullptr)
	{
		g_warning("meowmenu: failed to apply menu background styling: %s",
		          error->message);
		g_clear_error(&error);
	}
	g_free(css);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::on_screen_changed(GtkWidget* widget)
{
	if (m_resizing)
		interactive_resize_cancel();

	GdkScreen* screen = gtk_widget_get_screen(widget);
	GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
	// Always request an RGBA visual when the compositor provides one so that
	// themed RGBA backgrounds and rounded-corner clipping can be composited.
	if (!visual)
	{
		visual = gdk_screen_get_system_visual(screen);
		m_supports_alpha = false;
	}
	else
	{
		m_supports_alpha = true;
	}
	gtk_widget_set_visual(widget, visual);
	schedule_style_refresh();
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::apply_window_shape(int width, int height, int radius, bool composited)
{
	// Re-mask only when the silhouette actually changes; on_draw_event runs
	// often and re-applying the shape every frame would thrash the window mask.
	if (width == m_shape_width && height == m_shape_height
			&& radius == m_shape_radius && composited == m_shape_composited)
		return;
	m_shape_width = width;
	m_shape_height = height;
	m_shape_radius = radius;
	m_shape_composited = composited;

	GtkWidget* widget = GTK_WIDGET(m_window);
	if (!gtk_widget_get_realized(widget))
		return;

	// Square window: clear any prior mask so the toplevel is its full rectangle.
	// Covers both corner radius 0 (supported behavior) and the non-composited fallback
	// (supported behavior), which both render clean square corners.
	if (radius <= 0 || !composited || width <= 0 || height <= 0)
	{
		gtk_widget_shape_combine_region(widget, nullptr);
		return;
	}

	// Build a 1-bit (no anti-alias) rounded-rect mask matching the cairo clip in
	// on_draw_event, then apply it as the window's bounding shape. The crisp mask
	// edge sits one pixel outside the inset AA border stroke, so the border still
	// reads smoothly while the hard mask removes the square native-window corner.
	cairo_surface_t* mask = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
	cairo_t* mc = cairo_create(mask);
	cairo_set_antialias(mc, CAIRO_ANTIALIAS_NONE);
	cairo_set_operator(mc, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(mc, 0.0, 0.0, 0.0, 0.0);
	cairo_paint(mc);
	cairo_set_source_rgba(mc, 0.0, 0.0, 0.0, 1.0);
	const double rr = radius;
	cairo_new_path(mc);
	cairo_arc(mc, rr,         rr,          rr, G_PI,       G_PI * 1.5);
	cairo_arc(mc, width - rr, rr,          rr, G_PI * 1.5, G_PI * 2.0);
	cairo_arc(mc, width - rr, height - rr, rr, 0.0,        G_PI * 0.5);
	cairo_arc(mc, rr,         height - rr, rr, G_PI * 0.5, G_PI);
	cairo_close_path(mc);
	cairo_fill(mc);
	cairo_destroy(mc);
	cairo_surface_flush(mask);

	cairo_region_t* region = gdk_cairo_region_create_from_surface(mask);
	gtk_widget_shape_combine_region(widget, region);
	cairo_region_destroy(region);
	cairo_surface_destroy(mask);
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_draw_event(GtkWidget* widget, cairo_t* cr)
{
	if (!gtk_widget_get_realized(widget))
	{
		gtk_widget_realize(widget);
	}

	const double width = gtk_widget_get_allocated_width(widget);
	const double height = gtk_widget_get_allocated_height(widget);

	GdkScreen* screen = gtk_widget_get_screen(widget);
	const bool enabled = gdk_screen_is_composited(screen);

	// Single shared corner-radius clamp [0,24] — the live property-changed
	// handler queues a redraw that re-enters here, so the visible rounding can
	// never diverge from the control's range.
	const double r = meow::meowmenu_clamp_corner_radius(m_settings->corner_radius);

	// Mask the toplevel window itself to the rounded silhouette. The cairo clip
	// below only bounds windowless children; native-windowed regions ignore it
	// and would paint a square corner outside the outline without this shape.
	apply_window_shape(static_cast<int>(width), static_cast<int>(height),
		static_cast<int>(r), enabled && m_supports_alpha);

	auto clip_rounded = [&](cairo_t* c)
	{
		if (r > 0.0)
		{
			cairo_new_path(c);
			cairo_arc(c, r,         r,          r, G_PI,       G_PI * 1.5);
			cairo_arc(c, width - r, r,          r, G_PI * 1.5, G_PI * 2.0);
			cairo_arc(c, width - r, height - r, r, 0.0,        G_PI * 0.5);
			cairo_arc(c, r,         height - r, r, G_PI * 0.5, G_PI);
			cairo_close_path(c);
		}
		else
		{
			cairo_rectangle(c, 0.0, 0.0, width, height);
		}
	};

	// Path for the single border: the same silhouette as the clip, inset by half
	// the 1 px line width so the whole stroke stays inside the surface (and inside
	// the rounded clip at the corners) instead of being half-clipped at the edge.
	const double border_width = 1.0;
	auto border_path = [&](cairo_t* c)
	{
		const double inset = border_width / 2.0;
		const double br = (r > inset) ? (r - inset) : 0.0;
		cairo_new_path(c);
		if (br > 0.0)
		{
			cairo_arc(c, inset + br,         inset + br,          br, G_PI,       G_PI * 1.5);
			cairo_arc(c, width - inset - br, inset + br,          br, G_PI * 1.5, G_PI * 2.0);
			cairo_arc(c, width - inset - br, height - inset - br, br, 0.0,        G_PI * 0.5);
			cairo_arc(c, inset + br,         height - inset - br, br, G_PI * 0.5, G_PI);
			cairo_close_path(c);
		}
		else
		{
			cairo_rectangle(c, inset, inset, width - border_width, height - border_width);
		}
	};

	const bool is_fullscreen = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);
	GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
	const double menu_alpha = meowmenu_opacity_alpha(m_settings->menu_opacity);

	auto widget_rectangle = [&](GtkWidget* target, GdkRectangle* rectangle) -> bool
	{
		if (!GTK_IS_WIDGET(target) || !gtk_widget_get_visible(target)
				|| !rectangle)
			return false;
		GtkAllocation allocation = {};
		gtk_widget_get_allocation(target, &allocation);
		if (allocation.width <= 0 || allocation.height <= 0)
			return false;
		int x = 0;
		int y = 0;
		if (!gtk_widget_translate_coordinates(target, widget, 0, 0, &x, &y))
			return false;
		*rectangle = { x, y, allocation.width, allocation.height };
		return true;
	};

	auto paint_rectangle = [&](const GdkRectangle& rectangle,
			const GdkRGBA& colour, double alpha)
	{
		GdkRGBA source = colour;
		source.alpha = alpha;
		cairo_save(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_rectangle(cr, rectangle.x, rectangle.y,
				rectangle.width, rectangle.height);
		cairo_clip(cr);
		gdk_cairo_set_source_rgba(cr, &source);
		cairo_paint(cr);
		cairo_restore(cr);
	};

	auto paint_chrome_geometry = [&](const MenuChromeGeometry& geometry,
			const GdkRGBA& colour, double alpha)
	{
		if (!geometry.vertical.visible && !geometry.band.visible)
			return;
		GdkRGBA source = colour;
		source.alpha = alpha;
		cairo_save(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		if (geometry.vertical.visible)
			cairo_rectangle(cr, geometry.vertical.x, geometry.vertical.y,
					geometry.vertical.width, geometry.vertical.height);
		if (geometry.band.visible)
			cairo_rectangle(cr, geometry.band.x, geometry.band.y,
					geometry.band.width, geometry.band.height);
		cairo_clip(cr);
		gdk_cairo_set_source_rgba(cr, &source);
		cairo_paint(cr);
		cairo_restore(cr);
	};

	auto paint_surfaces = [&](double alpha)
	{
		const GdkRGBA& baseline = is_fullscreen
				? m_surface_palette.fullscreen : m_surface_palette.content;
		paint_rectangle({ 0, 0, static_cast<int>(width),
				static_cast<int>(height) }, baseline, alpha);
		if (is_fullscreen)
			return;

		auto surface_rectangle = [&](GtkWidget* target) -> MenuSurfaceRectangle
		{
			GdkRectangle rectangle = {};
			if (!widget_rectangle(target, &rectangle))
				return { 0, 0, 0, 0, false };
			return { rectangle.x, rectangle.y, rectangle.width,
					rectangle.height, true };
		};
		const MenuChromeGeometry geometry = meow_resolve_chrome_geometry(
				m_composition, static_cast<int>(width), static_cast<int>(height),
				m_layout_metrics.region_gap_px,
				surface_rectangle(m_profile->get_widget()),
				surface_rectangle(GTK_WIDGET(m_sidebar)),
				surface_rectangle(GTK_WIDGET(m_categories_box)),
				surface_rectangle(GTK_WIDGET(m_secondary_row)));
		// Both rectangles enter one SOURCE clip, so their L-shaped intersection
		// is painted once at the resolved menu alpha rather than compounded.
		paint_chrome_geometry(geometry, m_surface_palette.chrome, alpha);
		if (geometry.separator.visible)
			paint_rectangle({ geometry.separator.x, geometry.separator.y,
					geometry.separator.width, geometry.separator.height },
					m_separator_rgba, alpha);
		if (geometry.secondary_separator.visible)
			paint_rectangle({ geometry.secondary_separator.x,
					geometry.secondary_separator.y,
					geometry.secondary_separator.width,
					geometry.secondary_separator.height },
					m_separator_rgba, alpha);
	};

	if (enabled && m_supports_alpha)
	{
		// Erase the previous frame so pixels outside the rounded clip are transparent.
		cairo_save(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
		cairo_paint(cr);
		cairo_restore(cr);

		// Clip the ENTIRE window draw — shell background AND the propagated child
		// regions — to the rounded silhouette, then paint the one shell background
		// at the single menu alpha across the whole clipped area. The clip must
		// bound the children too so the radius rounds what the user actually sees.
		// At r==0 this is a plain rectangular clip, so corners reduce to clean
		// squares.
		cairo_save(cr);
		clip_rounded(cr);
		cairo_clip(cr);
		// Paint one baseline, then source-replace only disjoint Chrome regions.
		// SOURCE keeps every pixel at the single menu alpha even where semantic
		// rectangles meet the baseline.
		paint_surfaces(menu_alpha);
		if (child)
			gtk_container_propagate_draw(GTK_CONTAINER(widget), child, cr);
		cairo_restore(cr);

		// Exactly one intentional border, following the same rounded path, drawn
		// only docked + composited; full-screen reads as one seamless surface.
		if (meow::meowmenu_frame_draws_border(is_fullscreen, m_supports_alpha))
		{
			cairo_save(cr);
			cairo_set_line_width(cr, border_width);
			gdk_cairo_set_source_rgba(cr, &m_separator_rgba);
			border_path(cr);
			cairo_stroke(cr);
			cairo_restore(cr);
		}

		// We have drawn the background, the clipped children, and the border, so
		// stop emission: the default handler must not re-draw the children
		// unclipped (which would leave their square fill outside the rounded edge).
		return GDK_EVENT_STOP;
	}

	// Non-composited fallback (no RGBA visual): solid, square, no rounding
	// (supported behavior). Draw the shell background and propagate the children unclipped,
	// then a single SQUARE border — gated on docked only (not the composited
	// predicate, which requires supports_alpha), so a non-composited full-screen
	// menu stays one seamless square surface with no outline.
	paint_surfaces(1.0);
	if (child)
		gtk_container_propagate_draw(GTK_CONTAINER(widget), child, cr);
	if (!is_fullscreen)
	{
		cairo_save(cr);
		cairo_set_line_width(cr, border_width);
		gdk_cairo_set_source_rgba(cr, &m_separator_rgba);
		cairo_rectangle(cr, border_width / 2.0, border_width / 2.0,
			width - border_width, height - border_width);
		cairo_stroke(cr);
		cairo_restore(cr);
	}
	return GDK_EVENT_STOP;
}

//-----------------------------------------------------------------------------

/* Window::centered_layout:
 *
 * Returns: true when /layout-mode classifies as Centered. Reads the stored key
 * through the shared classifier so an unknown/stale value falls back to Docked
 * (false) without ever being written back.
 */
bool WhiskerMenu::Window::centered_layout() const
{
	return layout_mode_from_key(m_settings->layout_mode) == LayoutMode::Centered;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::center_window()
{
	// Add the monitor origin so the window centres on the *target* monitor, not
	// only on a monitor whose origin happens to be (0,0). Uses full monitor
	// geometry (m_monitor), which is the Centered-mode reference rectangle.
	m_geometry.x = m_monitor.x + (m_monitor.width - m_geometry.width) / 2;
	m_geometry.y = m_monitor.y + (m_monitor.height - m_geometry.height) / 2;
}

//-----------------------------------------------------------------------------

/* Window::move_window:
 *
 * Applies panel-relative gap and monitor clamping from the caller's current
 * unadjusted geometry, then moves either the normal toplevel or layer surface.
 * Resize completion resolves a fresh panel base before entering this path.
 */
void WhiskerMenu::Window::move_window()
{
	InteractiveResize::PanelEdge panel_edge =
			InteractiveResize::PanelEdge::None;
	if (m_position == PositionAtButton && !centered_layout())
	{
		const XfceScreenPosition screen_pos = m_plugin->get_screen_position();
		if (xfce_screen_position_is_top(screen_pos))
			panel_edge = InteractiveResize::PanelEdge::Top;
		else if (xfce_screen_position_is_bottom(screen_pos))
			panel_edge = InteractiveResize::PanelEdge::Bottom;
		else if (xfce_screen_position_is_left(screen_pos))
			panel_edge = InteractiveResize::PanelEdge::Left;
		else
			panel_edge = InteractiveResize::PanelEdge::Right;
	}

	// Derive the gap and monitor clamp from one unadjusted placement. During a
	// resize this base is freshly resolved from the panel before settling, so
	// repeated motion never compounds the configured gap.
	const InteractiveResize::Rectangle placed =
			InteractiveResize::place_docked(
					{m_geometry.x, m_geometry.y,
						m_geometry.width, m_geometry.height},
					{m_monitor.x, m_monitor.y,
						m_monitor.width, m_monitor.height},
					panel_edge,
					m_settings->panel_gap);
	m_geometry.x = placed.x;
	m_geometry.y = placed.y;

	// Move window
#ifdef HAVE_GTK_LAYER_SHELL
	if (gtk_layer_is_supported())
	{
		gtk_layer_set_margin(m_window, GTK_LAYER_SHELL_EDGE_LEFT, m_geometry.x - m_monitor.x);
		gtk_layer_set_margin(m_window, GTK_LAYER_SHELL_EDGE_TOP, m_geometry.y - m_monitor.y);
	}
	else
#endif
	{
		gtk_window_move(m_window, m_geometry.x, m_geometry.y);
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_mode_button_content(GtkToggleButton* button,
		bool show_icons, const char* const* icon_chain, const char* short_label,
		const char* long_label, GtkIconSize icon_size, int icon_px)
{
	GtkWidget* child = gtk_bin_get_child(GTK_BIN(button));

	// The pure helper picks the label placement (supported behavior): the long
	// descriptive name is the accessible name in both modes and the tooltip in
	// icon-only mode; the short label is the visible text in text mode.
	const ModeButtonLabels labels =
			meow_mode_button_labels(show_icons, short_label, long_label);
	atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(button)), labels.accessible_name);
	gtk_widget_set_tooltip_text(GTK_WIDGET(button), labels.tooltip_text);

	if (show_icons)
	{
		GtkIconTheme* theme = gtk_icon_theme_get_default();
		const char* name = meow_resolve_icon_name(theme, icon_chain);
		// Reuse an existing image child so a relayout that only changes the
		// derived size (e.g. category-icon-size edited) refreshes the pixel size
		// in place rather than no-op'ing on the early child check (supported behavior).
		GtkImage* image;
		if (GTK_IS_IMAGE(child))
		{
			image = GTK_IMAGE(child);
			gtk_image_set_from_icon_name(image, name, icon_size);
		}
		else
		{
			image = GTK_IMAGE(gtk_image_new_from_icon_name(name, icon_size));
			if (child)
				gtk_container_remove(GTK_CONTAINER(button), child);
			gtk_container_add(GTK_CONTAINER(button), GTK_WIDGET(image));
			gtk_widget_show(GTK_WIDGET(image));
		}
		// Raw pixel requests preserve category/Search references and allow the
		// Session-row visual compensation to follow its live GTK source metric. A
		// negative value clears the override when that metric is unavailable.
		gtk_image_set_pixel_size(image, icon_px);
	}
	else
	{
		// Visible text mode shows the short label; the long name stays on the
		// tooltip/accessible name set above.
		if (GTK_IS_LABEL(child))
		{
			gtk_label_set_text(GTK_LABEL(child), labels.visible_text);
			return;
		}
		GtkWidget* text = gtk_label_new(labels.visible_text);
		if (child)
			gtk_container_remove(GTK_CONTAINER(button), child);
		gtk_container_add(GTK_CONTAINER(button), text);
		gtk_widget_show(text);
	}
}

//-----------------------------------------------------------------------------

/* Window::apply_switch_presentation:
 * @presentation: final-home selector presentation for this pass.
 *
 * Applies icon/text content and region-derived sizing to the Apps/Places
 * selector. Shared secondary rows start from the Session command allocation
 * and compensate for their fuller icon artwork; sidebar and search-row homes
 * retain their own natural sources.
 */
void WhiskerMenu::Window::apply_switch_presentation(
		const SelectorPresentation& presentation)
{
	const bool icons = presentation.content == SelectorContent::Icons;
	const bool session_size = presentation.icon_size_source
			== SelectorIconSizeSource::SessionToolbar;
	const GtkIconSize icon_size = session_size
			? MEOWMENU_SESSION_BUTTON_ICON_SIZE : GTK_ICON_SIZE_BUTTON;
	const int icon_px = session_size
			? meow_selector_session_icon_px(presentation.icon_px)
			: presentation.icon_px;

	// Text↔icon child swap (supported behavior): the visible text label is the short
	// "Apps"/"Places"; the long "Applications"/"Places" stays as tooltip and
	// accessible name in both modes.
	set_mode_button_content(m_mode_btn_apps, icons, MEOW_SWITCH_APPS_ICONS,
			_("Apps"), _("Applications"), icon_size, icon_px);
	set_mode_button_content(m_mode_btn_places, icons, MEOW_SWITCH_PLACES_ICONS,
			_("Places"), _("Places"), icon_size, icon_px);

	GtkStyleContext* selector_context =
			gtk_widget_get_style_context(GTK_WIDGET(m_mode_selector_box));
	gtk_style_context_remove_class(selector_context, "strip");
	gtk_widget_set_size_request(GTK_WIDGET(m_mode_btn_apps), -1, -1);
	gtk_widget_set_size_request(GTK_WIDGET(m_mode_btn_places), -1, -1);

	// Switch-box orientation (supported behavior): vertical only on a vertical
	// sidebar with category names hidden; horizontal everywhere else.
	gtk_orientable_set_orientation(GTK_ORIENTABLE(m_mode_selector_box),
			presentation.orientation == SwitchOrientation::Vertical
					? GTK_ORIENTATION_VERTICAL
					: GTK_ORIENTATION_HORIZONTAL);
}

//-----------------------------------------------------------------------------

/* Window::apply_menu_composition:
 * @composition: resolved rows, control homes, columns, and physical slot order.
 *
 * Applies the composition through stable primary and secondary containers.
 * Profile moves as one block, Search keeps its live entry, and Session keeps
 * its existing command widgets and signals. Hidden controls lose focusability
 * in the same pass that removes their row allocation.
 */
void WhiskerMenu::Window::apply_menu_composition(
		const MenuComposition& composition)
{
	GtkWidget* profile = m_profile ? m_profile->get_widget() : nullptr;
	GtkWidget* sidebar_reserve = m_sidebar_header_reserve;
	GtkWidget* profile_content = m_profile ? m_profile->get_content() : nullptr;
	GtkWidget* profile_leading = m_profile
			? m_profile->get_leading_spacer() : nullptr;
	GtkWidget* profile_trailing = m_profile
			? m_profile->get_trailing_spacer() : nullptr;
	GtkWidget* picture = m_profile ? m_profile->get_picture() : nullptr;
	GtkWidget* username = m_profile ? m_profile->get_username() : nullptr;
	const bool fullscreen_middle = composition.search_column
			== MenuColumnRole::MiddleResults;

	if (fullscreen_middle)
	{
		if (gtk_box_get_center_widget(m_primary_row)
				!= GTK_WIDGET(m_primary_middle))
		{
			gtk_box_set_center_widget(m_primary_row,
					GTK_WIDGET(m_primary_middle));
		}
		// The middle box is created before the first show_all() pass and is
		// parentless until Full Screen composition selects it as the center
		// widget. GTK does not reveal a child merely because it is adopted by
		// GtkBox, so make the Search container visible on every transition.
		gtk_widget_show(GTK_WIDGET(m_primary_middle));
		if (profile)
			meow_box_repack_child(m_primary_row, profile,
					false, false, false, 0);
		meow_box_repack_child(m_primary_middle, GTK_WIDGET(m_search_entry),
				false, true, true, 0);
		gtk_widget_show(GTK_WIDGET(m_search_entry));
		meow_box_repack_child(m_primary_row, GTK_WIDGET(m_commands_box),
				true, false, false, 0);
	}
	else
	{
		if (gtk_box_get_center_widget(m_primary_row)
				== GTK_WIDGET(m_primary_middle))
		{
			gtk_box_set_center_widget(m_primary_row, nullptr);
		}
		gtk_widget_hide(GTK_WIDGET(m_primary_middle));
		if (profile)
			meow_box_repack_child(m_primary_row, profile,
					false, false, false, 0);
		meow_box_repack_child(m_primary_row, GTK_WIDGET(m_search_entry),
				false, true, true, 0);
		meow_box_repack_child(
				composition.session_location == MenuControlLocation::PrimaryRow
						? m_primary_row : m_secondary_row,
				GTK_WIDGET(m_commands_box), false, true, true, 0);
	}

	GtkBox* switch_parent = nullptr;
	if (composition.apps_places_location == MenuControlLocation::PrimaryRow)
		switch_parent = fullscreen_middle ? m_primary_middle : m_primary_row;
	else if (composition.apps_places_location == MenuControlLocation::SecondaryRow)
		switch_parent = m_secondary_row;
	else if (composition.apps_places_location == MenuControlLocation::Sidebar)
		switch_parent = m_category_buttons;
	if (switch_parent)
		meow_box_repack_child(switch_parent, GTK_WIDGET(m_mode_selector_box),
				false, false, false, 0);
	else if (GtkWidget* parent = gtk_widget_get_parent(
			GTK_WIDGET(m_mode_selector_box)))
	{
		if (GTK_IS_CONTAINER(parent))
			gtk_container_remove(GTK_CONTAINER(parent),
					GTK_WIDGET(m_mode_selector_box));
	}
	if (fullscreen_middle
			&& composition.apps_places_location
					== MenuControlLocation::PrimaryRow)
	{
		// GtkBox mirrors this semantic start-to-end pair in RTL.
		gtk_box_reorder_child(m_primary_middle,
				GTK_WIDGET(m_mode_selector_box), 0);
		gtk_box_reorder_child(m_primary_middle,
				GTK_WIDGET(m_search_entry), 1);
	}
	else if (composition.apps_places_location == MenuControlLocation::Sidebar)
	{
		// This is the last selector-owning composition pass. Keep its canonical
		// prefix authoritative even after a cross-parent move or packing update.
		meow_restore_vertical_selector_prefix(m_category_buttons,
				GTK_WIDGET(m_mode_selector_box), m_mode_selector_separator);
	}

	meow_widget_set_visible_if_valid(profile, composition.effective_profile);
	const bool reserve_sidebar_header = !fullscreen_middle
			&& !composition.effective_profile
			&& (composition.sidebar == CompositionSidebar::Left
				|| composition.sidebar == CompositionSidebar::Right);
	meow_widget_set_visible_if_valid(sidebar_reserve, reserve_sidebar_header);
	meow_widget_set_visible_if_valid(picture, composition.effective_profile);
	meow_widget_set_visible_if_valid(username, composition.effective_profile);
	meow_widget_set_visible_if_valid(GTK_WIDGET(m_commands_box),
			composition.effective_session);
	meow_widget_set_visible_if_valid(GTK_WIDGET(m_mode_selector_box),
			composition.apps_places_location != MenuControlLocation::Hidden);
	for (GtkWidget* button : m_commands_button)
	{
		meow_widget_set_can_focus_if_valid(button,
				composition.effective_session && gtk_widget_get_visible(button)
						&& gtk_widget_get_sensitive(button));
	}

	meow_widget_set_hexpand_if_valid(profile, false);
	meow_widget_set_hexpand_if_valid(GTK_WIDGET(m_search_entry), true);
	meow_widget_set_halign_if_valid(GTK_WIDGET(m_search_entry), GTK_ALIGN_FILL);
	meow_widget_set_hexpand_if_valid(GTK_WIDGET(m_mode_selector_box), false);
	meow_widget_set_vexpand_if_valid(GTK_WIDGET(m_mode_selector_box), false);
	meow_widget_set_valign_if_valid(GTK_WIDGET(m_mode_selector_box), GTK_ALIGN_CENTER);
	meow_widget_set_hexpand_if_valid(GTK_WIDGET(m_commands_box), false);
	meow_widget_set_halign_if_valid(GTK_WIDGET(m_commands_box),
			composition.session_alignment == MenuAlignment::Fill
					? GTK_ALIGN_FILL : GTK_ALIGN_END);
	// A right vertical sidebar owns the trailing selector slot, so its Session
	// buttons must use the opposite edge of the expanded command allocation.
	// Full Screen and layouts without a vertical sidebar retain the established
	// logical-trailing Session placement.
	const bool right_sidebar_session = composition.secondary_visible
			&& composition.sidebar == CompositionSidebar::Right
			&& composition.session_location
					== MenuControlLocation::SecondaryRow;
	gtk_box_reorder_child(m_commands_box, m_commands_spacer,
			meow_session_spacer_position(right_sidebar_session, m_layout_ltr));
	gtk_widget_set_margin_top(GTK_WIDGET(m_mode_selector_box), 0);
	gtk_widget_set_margin_bottom(GTK_WIDGET(m_mode_selector_box), 0);
	meow_widget_set_visible_if_valid(GTK_WIDGET(m_primary_row), true);
	meow_widget_set_visible_if_valid(GTK_WIDGET(m_secondary_row),
			composition.secondary_visible);

	int primary_position = 0;
	for (MenuSlot slot : composition.primary_slots)
	{
		GtkWidget* widget = nullptr;
		if (slot == MenuSlot::Profile)
			widget = profile;
		else if (slot == MenuSlot::SidebarReserve)
			widget = sidebar_reserve;
		else if (slot == MenuSlot::Search && !fullscreen_middle)
			widget = GTK_WIDGET(m_search_entry);
		else if (slot == MenuSlot::AppsPlaces && !fullscreen_middle)
			widget = GTK_WIDGET(m_mode_selector_box);
		else if (slot == MenuSlot::Session)
			widget = GTK_WIDGET(m_commands_box);
		if (widget && meow_container_contains_child(
				GTK_CONTAINER(m_primary_row), widget))
			gtk_box_reorder_child(m_primary_row, widget, primary_position++);
	}

	int secondary_position = 0;
	for (MenuSlot slot : composition.secondary_slots)
	{
		GtkWidget* widget = slot == MenuSlot::AppsPlaces
				? GTK_WIDGET(m_mode_selector_box)
				: (slot == MenuSlot::Session
					? GTK_WIDGET(m_commands_box) : nullptr);
		if (widget && meow_container_contains_child(
				GTK_CONTAINER(m_secondary_row), widget))
			gtk_box_reorder_child(m_secondary_row, widget,
					secondary_position++);
	}

	int band_position = 0;
	for (MenuBand band : composition.vertical_bands)
	{
		GtkWidget* widget = nullptr;
		if (band == MenuBand::Primary)
			widget = GTK_WIDGET(m_primary_row);
		else if (band == MenuBand::Results)
			widget = GTK_WIDGET(m_contents_stack);
		else if (band == MenuBand::Secondary)
			widget = GTK_WIDGET(m_secondary_row);
		// The horizontal sidebar is owned inside the Results grid.
		if (widget && meow_container_contains_child(GTK_CONTAINER(m_vbox), widget))
			gtk_box_reorder_child(m_vbox, widget, band_position++);
	}

	const char* sidebar_value = m_settings->sidebar_position;
	const bool vertical_sidebar = !fullscreen_middle
			&& m_settings->sidebar_enabled
			&& (g_strcmp0(sidebar_value, "left") == 0
				|| g_strcmp0(sidebar_value, "right") == 0);
	if (vertical_sidebar)
	{
		// Profile and navigation share one content-derived width, but not a
		// GtkSizeGroup: a group spanning the primary row and Results grid can feed
		// surplus to the sidebar when the window grows.
		meow_configure_profile_sidebar_alignment(profile, profile_content,
				profile_leading, profile_trailing,
				m_applications->get_button()->get_widget(), true,
				m_settings->category_show_name);
		const int sidebar_width = meow_configure_vertical_sidebar_width(
				GTK_WIDGET(m_sidebar), profile,
				true, composition.effective_profile);
		gtk_widget_set_size_request(sidebar_reserve,
				reserve_sidebar_header ? sidebar_width : -1, -1);
		meow_configure_vertical_sidebar_content(
				GTK_WIDGET(m_category_buttons), true);
		meow_widget_set_halign_if_valid(profile, GTK_ALIGN_FILL);
		meow_widget_set_halign_if_valid(GTK_WIDGET(m_mode_selector_box),
				GTK_ALIGN_START);
	}
	else
	{
		// The inner alignment belongs only to the windowed vertical-sidebar role.
		// Restore ordinary leading content before another composition reuses
		// Profile in its primary row.
		meow_configure_profile_sidebar_alignment(profile, profile_content,
				profile_leading, profile_trailing,
				m_applications->get_button()->get_widget(), false, true);
		if (!fullscreen_middle)
		{
			meow_configure_vertical_sidebar_width(GTK_WIDGET(m_sidebar), profile,
					false, false);
			meow_widget_set_halign_if_valid(profile, GTK_ALIGN_START);
			meow_widget_set_halign_if_valid(GTK_WIDGET(m_mode_selector_box),
					GTK_ALIGN_START);
		}
		gtk_widget_set_size_request(sidebar_reserve, -1, -1);
		meow_configure_vertical_sidebar_content(
				GTK_WIDGET(m_category_buttons), false);
	}
}

//-----------------------------------------------------------------------------

/* Window::update_layout:
 *
 * Reconciles the current settings, transient Session availability, and derived
 * composition into one stable GTK hierarchy. Repeated calls are safe while
 * Properties or command availability changes are being processed.
 */
void WhiskerMenu::Window::update_layout()
{
	m_layout_ltr = gtk_widget_get_default_direction() != GTK_TEXT_DIR_RTL;
	m_layout_sidebar_enabled = m_settings->sidebar_enabled;

	// Resolve category navigation from stored sidebar intent. Selector placement
	// remains undecided until the complete menu composition is available.
	SidebarLayoutState layout_state;
	layout_state.sidebar_enabled = m_settings->sidebar_enabled;
	layout_state.position = meow_parse_sidebar_position(m_settings->sidebar_position);
	layout_state.category_show_name = m_settings->category_show_name;
	const SidebarPresentation sidebar_presentation =
			meow_compute_sidebar_presentation(layout_state);

	// Session availability is transient, so refresh it before resolving the
	// selector presentation. This keeps the selector's sizing source aligned
	// with its actual secondary-row home during live command changes.
	unsigned int available_session_actions = 0;
	if (m_settings->show_session)
	{
		for (auto command : m_settings->command)
		{
			if (command->check())
				++available_session_actions;
		}
	}
	m_layout_available_session_actions = available_session_actions;

	CompositionSidebar composition_sidebar = CompositionSidebar::Hidden;
	if (layout_state.sidebar_enabled)
	{
		if (layout_state.position == SidebarPosition::Right)
			composition_sidebar = CompositionSidebar::Right;
		else if (layout_state.position == SidebarPosition::Horizontal)
			composition_sidebar = CompositionSidebar::Horizontal;
		else
			composition_sidebar = CompositionSidebar::Left;
	}
	m_layout_sidebar_position = composition_sidebar;
	m_layout_categories_horizontal = composition_sidebar
			== CompositionSidebar::Horizontal;
	MenuCompositionInput composition_input = {
			layout_mode_from_key(m_settings->layout_mode),
			g_strcmp0(m_settings->search_bar_position, "bottom") == 0
					? PrimaryEdge::Bottom : PrimaryEdge::Top,
			composition_sidebar,
			m_settings->show_profile,
			m_settings->show_session,
			available_session_actions,
			m_settings->places_enabled,
			m_layout_ltr ? MenuDirection::LeftToRight
					: MenuDirection::RightToLeft,
	};
	int session_icon_px = 0;
	gtk_icon_size_lookup(MEOWMENU_SESSION_BUTTON_ICON_SIZE,
			&session_icon_px, nullptr);
	int search_entry_h = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_search_entry), nullptr,
			&search_entry_h);
	const int search_bar_px = CLAMP(search_entry_h - 8, 16, 128);
	const MenuLayoutSnapshotInput snapshot_input = {
		composition_input,
			sidebar_presentation.effective_show_category_names,
			m_settings->places_switch_show_icons,
		m_settings->category_icon_size.get_size(),
		search_bar_px,
		session_icon_px
	};
	m_layout_snapshot = meow_resolve_layout_snapshot(snapshot_input);
	MenuComposition composition = m_layout_snapshot.composition;
	m_composition = composition;
	const SelectorPresentation selector_presentation =
			meow_resolve_selector_presentation(
					composition.apps_places_location,
					composition_input.layout_mode,
					sidebar_presentation.effective_show_category_names,
					m_settings->places_switch_show_icons,
					m_places_active,
					m_settings->category_icon_size.get_size(),
					search_bar_px, session_icon_px);
	const bool switch_visible = selector_presentation.home != SelectorHome::Hidden;
	gtk_widget_set_visible(GTK_WIDGET(m_mode_selector_box), switch_visible);
	if (m_mode_selector_separator)
	{
		// The separator only belongs under the switch in the vertical sidebar
		// list; every row presentation drops it.
		gtk_widget_set_visible(m_mode_selector_separator,
				switch_visible
				&& selector_presentation.home == SelectorHome::Sidebar);
	}
	// Visibility is owned by the same transaction used for opening and mode
	// switches, so layout changes cannot reintroduce a divergent matrix.
	apply_menu_mode(m_places_active ? MenuMode::Places
			: MenuMode::Applications, MenuModeTransition::Reevaluate);
	apply_switch_presentation(selector_presentation);

	// Sidebar-free pages retain their identity even when the selected model is
	// intentionally empty. Each wrapper names the page it actually contains.
	const bool show_default_heading =
			sidebar_presentation.show_default_category_heading;
	gtk_widget_set_visible(m_favorites_heading, show_default_heading);
	gtk_widget_set_visible(m_recent_heading, show_default_heading);
	m_applications->set_default_heading(show_default_heading);

	// Arrange the category list and the Apps/Places switch structurally
	// (sidebar behavior). Three category-list placements — vertical sidebar,
	// Horizontal strip, or hidden (sidebar disabled) — and three switch homes —
	// vertical sidebar list, shared secondary row, or unified Search row — are
	// reconciled here. A steady-state pass observes the current parent and
	// performs no reparenting.
	const bool want_vertical = sidebar_presentation.sidebar_visible
			&& !sidebar_presentation.categories_horizontal;
	const bool want_strip = sidebar_presentation.sidebar_visible
			&& sidebar_presentation.categories_horizontal;
	const int  want_struct   = want_vertical ? 1 : (want_strip ? 2 : 3);

	if (want_struct != m_sidebar_struct)
	{
		// The switch may ride inside m_category_buttons here; it is not detached
		// first because the re-homing block below runs in the same pass (before
		// anything is drawn) and moves it from whatever container it lands in to
		// its computed target. The box keeps a ref across the reparent, so the
		// switch is never orphaned or destroyed in between.
		g_object_ref(m_category_buttons);
		GtkWidget* cb_parent = gtk_widget_get_parent(GTK_WIDGET(m_category_buttons));
		if (cb_parent)
			gtk_container_remove(GTK_CONTAINER(cb_parent), GTK_WIDGET(m_category_buttons));

		if (want_strip)
		{
			gtk_orientable_set_orientation(GTK_ORIENTABLE(m_category_buttons),
					GTK_ORIENTATION_HORIZONTAL);
			// GtkViewport stretches its child to the view width and ignores child
			// halign, so the prebuilt expanding spacers provide centering without
			// changing the strip's width authority or overflow behavior.
			scroller_add_child(m_strip_scroll, GTK_WIDGET(m_category_buttons));
			// Single row: the category group is centered by the symmetric spacers.
			// The categories box fills the available width. The fullscreen pass
			// applies the shared main-column margins so the strip
			// tracks the results box.
			gtk_box_reorder_child(m_category_buttons, m_strip_lead_spacer,
				meow_strip_spacer_order(true));
			gtk_box_reorder_child(m_category_buttons, m_strip_trail_spacer, -1);
			gtk_widget_show(m_strip_lead_spacer);
			gtk_widget_show(m_strip_trail_spacer);
			gtk_widget_set_halign(GTK_WIDGET(m_categories_box), GTK_ALIGN_FILL);
			// The strip's column width and its edge alignment with the results box
			// in full-screen are owned by the FILL + symmetric-margin geometry
			// applied in show() (docked = menu width, full-screen = main column),
			// so no size group is needed to track the search box.
			// Reveal the scroller/viewport without gtk_widget_show_all, which
			// would override the per-button visibility set above.
			gtk_widget_show(GTK_WIDGET(m_strip_scroll));
			if (GtkWidget* vp = gtk_bin_get_child(GTK_BIN(m_strip_scroll)))
				gtk_widget_show(vp);
			gtk_widget_show(GTK_WIDGET(m_category_buttons));
			gtk_widget_set_visible(GTK_WIDGET(m_sidebar), false);
			gtk_widget_set_visible(GTK_WIDGET(m_categories_box), true);
		}
		else
		{
			// Vertical sidebar, or hidden: the button box lives in the sidebar
			// scroller either way; the sidebar itself is shown only when enabled.
			gtk_orientable_set_orientation(GTK_ORIENTABLE(m_category_buttons),
					GTK_ORIENTATION_VERTICAL);
			// Strip-only spacers must not consume space in the vertical sidebar
			// list; hiding them removes them from allocation entirely.
			if (m_strip_lead_spacer)
				gtk_widget_hide(m_strip_lead_spacer);
			if (m_strip_trail_spacer)
				gtk_widget_hide(m_strip_trail_spacer);
			scroller_add_child(m_sidebar, GTK_WIDGET(m_category_buttons));
			gtk_widget_set_visible(GTK_WIDGET(m_categories_box), false);
			gtk_widget_set_visible(GTK_WIDGET(m_sidebar), want_vertical);
			// No strip in this structure: drop the strip alignment so the (hidden)
			// container imposes nothing on its size group.
			gtk_widget_set_halign(GTK_WIDGET(m_categories_box), GTK_ALIGN_FILL);
		}
		g_object_unref(m_category_buttons);
		m_sidebar_struct = want_struct;
	}
	if (m_strip_lead_spacer && m_strip_trail_spacer)
	{
		const bool spacers_visible =
				meow_strip_spacers_visible(want_strip);
		if (spacers_visible)
		{
			gtk_box_reorder_child(m_category_buttons, m_strip_lead_spacer,
					meow_strip_spacer_order(true));
			gtk_box_reorder_child(m_category_buttons, m_strip_trail_spacer, -1);
		}
		// Apply visibility even when the structural parent is unchanged. Initial
		// show-all reveals both spacers, so transition-only updates would leave a
		// sparse vertical Places group centred between them.
		gtk_widget_set_visible(m_strip_lead_spacer, spacers_visible);
		gtk_widget_set_visible(m_strip_trail_spacer, spacers_visible);
	}

	// Re-home the Apps/Places switch to match the computed presentation.
	{
		GtkWidget* sw = GTK_WIDGET(m_mode_selector_box);
		GtkWidget* cur = gtk_widget_get_parent(sw);
		GtkWidget* target = nullptr;
		switch (selector_presentation.home)
		{
		case SelectorHome::Sidebar:
			// Apps/Places is owned by a vertical category list only. Horizontal
			// category strips never use this location.
			target = GTK_WIDGET(m_category_buttons);
			break;
		case SelectorHome::SecondaryRow:
			// A visible Session group owns the secondary row. The composition
			// pass below performs the final logical leading/trailing reorder.
			target = GTK_WIDGET(m_secondary_row);
			break;
		case SelectorHome::WindowedPrimary:
		case SelectorHome::FullScreenSearch:
			// The resolver performs the final row placement below. Park the switch
			// in the ordinary row until that atomic pass chooses its supported home.
			target = GTK_WIDGET(m_search_box);
			break;
		case SelectorHome::Hidden:
		default:
			break;
		}

		if (target && cur != target)
		{
			g_object_ref(sw);
			if (cur)
				gtk_container_remove(GTK_CONTAINER(cur), sw);
			if (selector_presentation.home == SelectorHome::WindowedPrimary
					|| selector_presentation.home == SelectorHome::FullScreenSearch)
			{
				// Search-row staging: the embedded switch is anchored before the
				// command buttons (the leading side), not at the very
				// trailing edge, so the commands stay rightmost and the search entry
				// shrinks to make room (supported behavior). When no commands share the row the
				// switch becomes the trailing element. The leading placement is the
				// single source of truth in meow_embedded_switch_slot() and is pinned
				// by a regression test; do not inline the ordering decision here.
				gtk_box_pack_start(GTK_BOX(target), sw, false, false, 0);
				GtkWidget* cmd = GTK_WIDGET(m_commands_box);
				const bool commands_box_is_in_search_box =
						(gtk_widget_get_parent(cmd) == target);
				if (meow_embedded_switch_slot(commands_box_is_in_search_box)
						== EmbeddedSwitchSlot::BeforeCommands)
				{
					GList* kids = gtk_container_get_children(GTK_CONTAINER(target));
					gtk_box_reorder_child(GTK_BOX(target), sw,
							g_list_index(kids, cmd));
					g_list_free(kids);
				}
				else
				{
					gtk_box_reorder_child(GTK_BOX(target), sw, -1);
				}
			}
			else
			{
				// The vertical sidebar keeps the selector before its categories.
				gtk_box_pack_start(GTK_BOX(target), sw, false, false, 0);
				gtk_box_reorder_child(GTK_BOX(target), sw, 0);
			}
			g_object_unref(sw);
		}
		else if (target && cur == target
				&& selector_presentation.home == SelectorHome::Sidebar
				&& want_vertical)
		{
			gtk_box_reorder_child(GTK_BOX(target), sw, 0);
		}
		else if (!target && cur)
		{
			// A hidden selector owns no parent, allocation, or focus target.
			gtk_container_remove(GTK_CONTAINER(cur), sw);
		}
	}

	// Visibility follows the supported boolean controls and effective action
	// availability in every layout mode.
	const bool profile_hidden = !m_settings->show_profile;
	const bool commands_hidden = !m_settings->show_session
			|| m_layout_available_session_actions == 0;

	GtkWidget* profile_picture = m_profile ? m_profile->get_picture() : nullptr;
	GtkWidget* profile_username = m_profile ? m_profile->get_username() : nullptr;

	meow_widget_set_visible_if_valid(profile_picture, !profile_hidden);
	meow_widget_set_visible_if_valid(profile_username, !profile_hidden);
	meow_widget_set_visible_if_valid(GTK_WIDGET(m_commands_box), !commands_hidden);
	for (int i = 0; i < 9; ++i)
	{
		meow_widget_set_can_focus_if_valid(m_commands_button[i], !commands_hidden);
	}

	// Reparenting the selector during a layout transition can move it after a
	// category. Restore the stable vertical-sidebar order on every pass.
	if (want_vertical)
	{
		gtk_box_reorder_child(m_category_buttons,
				GTK_WIDGET(m_mode_selector_box), 0);
		gtk_box_reorder_child(m_category_buttons,
				m_mode_selector_separator, 1);
	}

	// Command buttons follow the physical row direction while the resolver
	// owns the Session group's parent and alignment.
	meow_widget_set_halign_if_valid(profile_username,
			m_layout_ltr ? GTK_ALIGN_START : GTK_ALIGN_END);
	for (int i = 0; i < 9; ++i)
	{
		meow_box_reorder_child_if_present(m_commands_box,
				m_commands_button[i], m_layout_ltr ? i : 8 - i);
	}

	// Arrange horizontal order of applications and sidebar
	g_object_ref(m_categories_box);
	g_object_ref(m_panels_stack);
	g_object_ref(m_sidebar);

	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(m_category_buttons));
	if (gtk_style_context_has_class(context, "left"))
	{
		gtk_style_context_remove_class(context, "left");
	}
	else if (gtk_style_context_has_class(context, "right"))
	{
		gtk_style_context_remove_class(context, "right");
	}
	else if (gtk_style_context_has_class(context, "top"))
	{
		gtk_style_context_remove_class(context, "top");
	}
	else if (gtk_style_context_has_class(context, "bottom"))
	{
		gtk_style_context_remove_class(context, "bottom");
	}

	// Horizontal is derived to the edge opposite the windowed primary row and
	// always below Results in Full Screen.
	const SidebarPosition strip_position = meow_resolve_sidebar_edge(
			meow_parse_sidebar_position(m_settings->sidebar_position),
			g_strcmp0(m_settings->search_bar_position, "bottom") == 0,
			g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);
	const StripGeometry strip_geom = meow_compute_strip_geometry(
			strip_position, m_layout_ltr);
	const bool strip_below_results = m_layout_categories_horizontal
			&& strip_geom.order == StripOrder::StripBelowResults;
	const bool strip_top_row = m_layout_categories_horizontal
			? !strip_below_results
			: true;

	const int region_gap = m_layout_metrics.region_gap_px;
	gtk_container_set_border_width(GTK_CONTAINER(m_vbox), region_gap);
	gtk_box_set_spacing(m_vbox, region_gap);
	gtk_box_set_spacing(m_primary_row, region_gap);
	gtk_box_set_spacing(m_primary_middle, region_gap);
	gtk_box_set_spacing(m_secondary_row, region_gap);
	// The grid owns the strip/results or sidebar/results boundary. Widget
	// margins stay zero so adjacent regions can never sum two gaps.
	gtk_widget_set_margin_top(GTK_WIDGET(m_categories_box), 0);
	gtk_widget_set_margin_bottom(GTK_WIDGET(m_categories_box), 0);
	const MenuContentMargins contents_margins =
			meow_resolve_contents_frame_margins(composition,
					Resizer::HandleSize);
	// Resize handles occupy the outer edge but paint no surface. Keep the
	// adjacent Results allocation inside the optically centred separator so
	// native GtkScrolledWindow clipping and the visible boundary are identical.
	gtk_widget_set_margin_top(GTK_WIDGET(m_contents_stack),
			contents_margins.top);
	gtk_widget_set_margin_bottom(GTK_WIDGET(m_contents_stack),
			contents_margins.bottom);

	gtk_grid_remove_row(m_contents_box, 1);
	gtk_grid_remove_row(m_contents_box, 0);
	if (m_layout_categories_horizontal)
	{
		gtk_grid_set_column_spacing(m_contents_box, 0);
		gtk_grid_set_row_spacing(m_contents_box,
				meow_resolve_boundary_gap(true, true, region_gap));

		gtk_style_context_add_class(context, strip_below_results ? "bottom" : "top");
	}
	else
	{
		gtk_grid_set_column_spacing(m_contents_box,
				meow_resolve_boundary_gap(m_layout_sidebar_enabled,
						true, region_gap));
		gtk_grid_set_row_spacing(m_contents_box, 0);

		gtk_style_context_add_class(context,
				m_layout_sidebar_position == CompositionSidebar::Left
						? "left" : "right");
	}

	const bool sidebar_in_first_grid_column = m_layout_ltr
			== (m_layout_sidebar_position == CompositionSidebar::Left);
	if (!sidebar_in_first_grid_column)
	{
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 0, 0, 1, 1);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 1, 0, 1, 1);
	}
	else
	{
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 0, 0, 1, 1);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 1, 0, 1, 1);
	}

	if (strip_top_row)
	{
		// Strip above the results row (Top strip; below the search bar since the
		// whole grid sits under it). The inserted row pushes the results down.
		gtk_grid_insert_row(m_contents_box, 0);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_categories_box), 0, 0, 2, 1);
	}
	else
	{
		// Strip below the results row (Bottom strip).
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_categories_box), 0, 1, 2, 1);
	}

	g_object_unref(m_sidebar);
	g_object_unref(m_panels_stack);
	g_object_unref(m_categories_box);

	apply_menu_composition(composition);

	bind_category_focus_adjustments();
	reveal_active_category();
	update_favourite_drop_targets();
}

//-----------------------------------------------------------------------------
