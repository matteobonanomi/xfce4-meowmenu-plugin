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

#include "launcher/applications-page.h"
#include "launcher/category-button.h"
#include "launcher/command.h"
#include "launcher/favorites-page.h"
#include "places/favourites-section.h"
#include "places/history-section.h"
#include "places/home-section.h"
#include "ui/launcher-view.h"
#include "places/places-page.h"
#include "plugin.h"
#include "profile.h"
#include "launcher/recent-page.h"
#include "resizer.h"
#include "opacity-model.h"
#include "search/search-page.h"
#include "settings.h"
#include "sidebar-layout.h"
#include "theme-fallback.h"
#include "user-session-layout.h"
#include "window-frame.h"
#include "ui/slot.h"
#include "ui/switch-icons.h"
#include "search/unified-bar.h"
#include "window-keyboard.h"

#include <libxfce4ui/libxfce4ui.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

#include <algorithm>
#include <ctime>
#include <iterator>
#include <vector>

using namespace WhiskerMenu;

namespace
{

/* vertical_end_of:
 * @value: a position string ("top", "top-right", "bottom", "bottom-right",
 *         "hidden", "left", "right", ...).
 *
 * Collapses a position key down to its vertical end. Anything that does not
 * begin with "top" or "bottom" is treated as "neutral" (e.g. "hidden", "left",
 * "right") and contributes no constraint to the unified-bar predicate.
 *
 * Returns: 't' for top, 'b' for bottom, 0 otherwise.
 */
static char vertical_end_of(const char* value)
{
	if (!value || !*value)
		return 0;
	if (g_str_has_prefix(value, "top"))
		return 't';
	if (g_str_has_prefix(value, "bottom"))
		return 'b';
	return 0;
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
		if (!gtk_widget_get_visible(w) || !gtk_widget_get_sensitive(w))
			continue;
		out = g_list_append(out, w);
	}
	g_list_free(children);
	return out;
}

} // namespace

namespace WhiskerMenu
{

/* unified_bar_preconditions_raw:
 * @layout_mode:         e.g. "fullscreen", "docked".
 * @search_bar_position: e.g. "top", "bottom".
 * @profile_position:    e.g. "top", "bottom", "hidden".
 * @commands_position:   e.g. "top-right", "bottom-right", "hidden".
 *
 * Pure variant of unified_bar_preconditions_met() that takes raw strings so
 * it can be unit-tested without a live Settings object. The unified bar is
 * only coherent on FullScreen when all non-neutral vertical ends agree.
 * "hidden" profile/commands are transparent — no constraint.
 *
 * Returns: true iff a unified bar would be coherent given these values.
 */
bool unified_bar_preconditions_raw(const char* layout_mode,
                                    const char* search_bar_position,
                                    const char* profile_position,
                                    const char* commands_position)
{
	if (g_strcmp0(layout_mode, "fullscreen") != 0)
		return false;
	const char ends[] = {
		vertical_end_of(search_bar_position),
		vertical_end_of(profile_position),
		vertical_end_of(commands_position),
	};
	char anchor = 0;
	for (char e : ends)
	{
		if (!e)
			continue;
		if (!anchor)
			anchor = e;
		else if (anchor != e)
			return false;
	}
	return vertical_end_of(search_bar_position) != 0;
}

bool unified_bar_preconditions_met(const Settings& s)
{
	return unified_bar_preconditions_raw(s.layout_mode,
		s.search_bar_position, s.profile_position, s.commands_position);
}

/* unified_bar_effective:
 * @s: current settings value-bag.
 *
 * Returns: true iff /unified-bar is on AND the preconditions are met.
 */
bool unified_bar_effective(const Settings& s)
{
	return s.unified_bar && unified_bar_preconditions_met(s);
}

} // namespace WhiskerMenu

//-----------------------------------------------------------------------------

WhiskerMenu::Window::Window(Settings* settings, Plugin* plugin) :
	m_settings(settings),
	m_plugin(plugin),
	m_window(nullptr),
	m_css_provider(nullptr),
	m_position(PositionAtButton),
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
	m_mode_selector_separator(nullptr),
	m_strip_scroll(nullptr),
	m_strip_lead_spacer(nullptr),
	m_sidebar_struct(1),
	m_switch_loc(SwitchLocation::InSidebar),
	m_category_width_group(nullptr),
	m_sidebar_size_group(nullptr),
	m_mode_button_size_group(nullptr),
	m_geometry{0,0,1,1},
	m_layout_ltr(true),
	m_layout_categories_horizontal(false),
	m_layout_sidebar_enabled(true),
	m_layout_switch_show_icons(false),
	m_layout_category_show_name(true),
	m_layout_category_icon_size(-2),
	m_layout_categories_alternate(false),
	m_layout_search_alternate(false),
	m_layout_commands_alternate(false),
	m_layout_profile_alternate(false),
	m_layout_profile_hidden(false),
	m_layout_commands_hidden(false),
	m_layout_unified_bar(false),
	m_profile_shape(0),
	m_supports_alpha(false),
	m_child_has_focus(false),
	m_resizing(false)
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

	// Create action buttons. FR-022 / SC-005: a 250 ms monotonic-clock
	// debounce absorbs held-Enter key-repeat bursts so a single press
	// (or burst) launches exactly once. The state is process-global
	// across the nine session buttons because the user can only target
	// one button per keypress, and a stuck key must not fan out across
	// adjacent buttons.
	static Keyboard::ActivationDebounce s_command_debounce;
	for (int i = 0; i < 9; ++i)
	{
		m_commands_button[i] = m_settings->command[i]->get_button();
		// FR-040: session buttons participate in the keyboard focus chain.
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

	// FR-040: ←/→ move focus between visible session buttons in physical
	// box order with NO wrap. ↑/↓ are no-ops within the Profile bar so
	// they do not accidentally jump to other zones (Tab is the cross-zone
	// motion). The avatar/username are kept non-focusable below.
	auto profile_arrow_handler = [this](GtkWidget* widget, GdkEvent* ev) -> gboolean
	{
		GdkEventKey* key = reinterpret_cast<GdkEventKey*>(ev);
		const bool left  = (key->keyval == GDK_KEY_Left
				|| key->keyval == GDK_KEY_KP_Left);
		const bool right = (key->keyval == GDK_KEY_Right
				|| key->keyval == GDK_KEY_KP_Right);
		const bool up    = (key->keyval == GDK_KEY_Up
				|| key->keyval == GDK_KEY_KP_Up);
		const bool down  = (key->keyval == GDK_KEY_Down
				|| key->keyval == GDK_KEY_KP_Down);
		if (up || down)
		{
			return GDK_EVENT_STOP; // explicit no-op within the bar
		}
		if (!left && !right)
		{
			return GDK_EVENT_PROPAGATE;
		}

		// Walk the focusable session buttons in their CURRENT physical
		// box order. m_commands_box may reorder children for RTL or for
		// the alternate-commands layout (see update_layout()), so we
		// must read the live order rather than the m_commands_button
		// declaration order.
		GList* kids = gtk_container_get_children(GTK_CONTAINER(m_commands_box));
		std::vector<GtkWidget*> ordered;
		for (GList* li = kids; li; li = li->next)
		{
			GtkWidget* w = GTK_WIDGET(li->data);
			if (!GTK_IS_BUTTON(w))
				continue;
			if (!gtk_widget_get_visible(w) || !gtk_widget_get_sensitive(w))
				continue;
			ordered.push_back(w);
		}
		g_list_free(kids);
		if (ordered.empty())
			return GDK_EVENT_STOP;

		auto it = std::find(ordered.begin(), ordered.end(), widget);
		if (it == ordered.end())
			return GDK_EVENT_PROPAGATE;

		// FR-120: in RTL the visual left/right are swapped relative to
		// physical box order. Use the widget's default direction to
		// pick the move sign rather than assuming LTR.
		const bool ltr = (gtk_widget_get_default_direction()
				!= GTK_TEXT_DIR_RTL);
		const bool move_next = (ltr ? right : left);
		const bool move_prev = (ltr ? left  : right);

		if (move_next && std::next(it) != ordered.end())
		{
			gtk_widget_grab_focus(*std::next(it));
		}
		else if (move_prev && it != ordered.begin())
		{
			gtk_widget_grab_focus(*std::prev(it));
		}
		// At either end: explicit no-op (FR-040 "NO wrap").
		return GDK_EVENT_STOP;
	};
	for (int i = 0; i < 9; ++i)
	{
		connect(m_commands_button[i], "key-press-event", profile_arrow_handler);
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

	// FR-021: Enter on the search entry activates the first match (or
	// is a silent no-op when the current query has zero results — see
	// clarification Q4). The debounce inside Page::launcher_activated
	// covers held-Enter key-repeat (FR-022).
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
		[this](GtkToggleButton*)
		{
			favorites_toggled();
		});

	// Create recent
	m_recent = new RecentPage(m_settings, this);

	CategoryButton* recent_button = m_recent->get_button();
	recent_button->join_group(favorites_button);
	connect(recent_button->get_widget(), "toggled",
		[this](GtkToggleButton*)
		{
			recent_toggled();
		});

	// Create applications
	m_applications = new ApplicationsPage(m_settings, this);

	CategoryButton* applications_button = m_applications->get_button();
	applications_button->join_group(recent_button);
	connect(applications_button->get_widget(), "toggled",
		[this](GtkToggleButton*)
		{
			category_toggled();
		});

	// Places mode (milestone 005) — built unconditionally; visibility is gated
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
	// active section button so arrow navigation can continue (FR-003; C1/C2).
	connect(m_places_home_btn->get_widget(), "toggled",
		[this](GtkToggleButton* b)
		{
			if (!gtk_toggle_button_get_active(b))
				return;
			m_places->set_active_section(m_places->get_home_section());
			gtk_stack_set_visible_child_name(m_panels_stack, "places");
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
			if (!m_keyboard_category_nav)
				gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
		});

	// Mode selector: two toggle buttons forming a manual radio group.
	m_mode_selector_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_mode_selector_box)),
			"places-mode-selector");
	m_mode_btn_apps = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(_("Apps")));
	m_mode_btn_places = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(_("Places")));
	// FR-090: no Alt-mnemonic activation on the mode toggles — the labels
	// "Apps"/"Places" must render verbatim, not as "_Apps"/"_Places".
	gtk_button_set_use_underline(GTK_BUTTON(m_mode_btn_apps),   FALSE);
	gtk_button_set_use_underline(GTK_BUTTON(m_mode_btn_places), FALSE);
	// FR-002 / SC-007: shared focus-indicator class. The rule lives in
	// the CSS provider built by update_background_css() so themes can
	// override the ring colour/thickness by re-styling .meow-focus-ring.
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_mode_btn_apps)),
			"meow-focus-ring");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_mode_btn_places)),
			"meow-focus-ring");
	gtk_toggle_button_set_active(m_mode_btn_apps, true);
	gtk_box_pack_start(m_mode_selector_box, GTK_WIDGET(m_mode_btn_apps), true, true, 0);
	gtk_box_pack_start(m_mode_selector_box, GTK_WIDGET(m_mode_btn_places), true, true, 0);
	// Equal-width Apps/Places buttons in every layout/preset (FR-013). A size
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
	// never a keyboard-focus stop (FR-007). Make both toggle buttons
	// non-focusable so GTK's own Tab traversal can never land on them and
	// so no arrow-key mode flip is possible from them; mouse activation
	// via the "toggled" handlers above is unaffected.
	gtk_widget_set_can_focus(GTK_WIDGET(m_mode_btn_apps),   FALSE);
	gtk_widget_set_can_focus(GTK_WIDGET(m_mode_btn_places), FALSE);

	// Create search results
	m_search_results = new SearchPage(m_settings, this);

	GtkBox* search_results = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
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
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_title_box), false, false, 0);
	gtk_box_pack_start(m_title_box, m_profile->get_picture(), false, false, 0);
	gtk_box_pack_start(m_title_box, m_profile->get_username(), true, true, 0);
	gtk_box_pack_start(m_title_box, GTK_WIDGET(m_commands_box), false, false, 0);

	// Add search to layout
	m_search_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_search_box), false, true, 0);
	gtk_box_pack_start(m_search_box, GTK_WIDGET(m_search_entry), true, true, 0);

	// Unified-bar centring cluster (full-screen only). In unified-bar mode the
	// search entry and the Apps/Places switch must travel together as one
	// centred unit (switch trailing the entry), so they live in this box and the
	// box — not the bare entry — becomes m_title_box's centre widget. A fixed
	// width on the cluster makes the entry yield room to the switch instead of
	// the row growing. We hold an owning ref because the cluster is unparented in
	// every non-unified layout; it is released in the destructor.
	m_search_cluster = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	g_object_ref_sink(m_search_cluster);

	// Three void bands for FullScreen unified-bar mode (FR-008, FR-017, FR-018).
	// All three are hidden until update_layout() activates the unified bar.
	// Fixed size_request heights give breathing room; theme authors can override
	// the "symmetric-void" CSS class min-height. Top/bottom: 12 px; middle: 16 px.
	auto make_void_band = [](int height_px) -> GtkWidget*
	{
		GtkWidget* w = gtk_label_new(nullptr);
		gtk_widget_set_hexpand(w, TRUE);
		gtk_widget_set_can_focus(w, FALSE);
		atk_object_set_role(gtk_widget_get_accessible(w), ATK_ROLE_FILLER);
		gtk_style_context_add_class(gtk_widget_get_style_context(w), "symmetric-void");
		gtk_widget_set_size_request(w, -1, height_px);
		gtk_widget_set_visible(w, FALSE);
		return w;
	};
	m_void_top    = make_void_band(12);
	m_void_middle = make_void_band(16);
	m_void_bottom = make_void_band(12);
	gtk_box_pack_start(m_vbox, m_void_top,    FALSE, FALSE, 0);
	gtk_box_pack_start(m_vbox, m_void_middle, FALSE, FALSE, 0);
	gtk_box_pack_start(m_vbox, m_void_bottom, FALSE, FALSE, 0);

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
	gtk_stack_add_named(m_panels_stack, m_favorites->get_widget(), "favorites");
	gtk_stack_add_named(m_panels_stack, m_recent->get_widget(), "recent");
	// Use the outer wrapper so the default-category heading (sidebar-disabled
	// case, FR-020) sits above the applications launcher view.
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
	gtk_box_pack_start(m_category_buttons, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), false, false, 4);

	// Prevent the sidebar from narrowing when Apps<->Places hides some
	// buttons: group all category-button widgets so every button requests
	// the same width as the widest one, even while hidden.
	// HACK: gtk_size_group_set_ignore_hidden() is deprecated since GTK 3.22
	// (the behaviour it controls became the default in GTK 4), but GTK 3 still
	// requires the explicit call.  Suppress the deprecation warning locally.
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	m_category_width_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_set_ignore_hidden(m_category_width_group, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
	// Both the Apps-mode buttons and the Places-section buttons join the width
	// group, so switching Apps↔Places (which only toggles visibility) never
	// resizes the sidebar (FR-023/024) — the group already sizes to the widest
	// member, hidden or not.
	gtk_size_group_add_widget(m_category_width_group, favorites_button->get_widget());
	gtk_size_group_add_widget(m_category_width_group, recent_button->get_widget());
	gtk_size_group_add_widget(m_category_width_group, applications_button->get_widget());
	gtk_size_group_add_widget(m_category_width_group, m_places_home_btn->get_widget());
	gtk_size_group_add_widget(m_category_width_group, m_places_history_btn->get_widget());
	gtk_size_group_add_widget(m_category_width_group, m_places_fav_btn->get_widget());

	m_sidebar = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 1, 1, 1, 1);
	gtk_scrolled_window_set_propagate_natural_height(m_sidebar, true);
	gtk_scrolled_window_set_shadow_type(m_sidebar, GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));

	// FR-100: keep the focused category in view as Tab/arrow keys move
	// through the sidebar. GtkScrolledWindow wraps m_category_buttons in
	// a viewport on gtk_container_add; binding the viewport's
	// adjustments to the box's focus chain makes the viewport scroll
	// automatically on focus-grab. NOTE: needs both axes because the
	// sidebar can be vertical or horizontal depending on preset
	// (sidebar_position: left|right|top|bottom).
	{
		GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(m_sidebar));
		if (GTK_IS_VIEWPORT(viewport))
		{
			gtk_container_set_focus_vadjustment(GTK_CONTAINER(m_category_buttons),
					gtk_scrolled_window_get_vadjustment(m_sidebar));
			gtk_container_set_focus_hadjustment(GTK_CONTAINER(m_category_buttons),
					gtk_scrolled_window_get_hadjustment(m_sidebar));
		}
	}

	// Handle default page
	reset_default_button();

	// Add CSS classes
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_window)), "meowmenu");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_search_box)), "search-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_title_box)), "title-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_commands_box)), "commands-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_contents_stack)), "contents");

	m_css_provider = gtk_css_provider_new();
	GdkScreen* css_screen = gtk_widget_get_screen(GTK_WIDGET(m_window));
	gtk_style_context_add_provider_for_screen(
		css_screen ? css_screen : gdk_screen_get_default(),
		GTK_STYLE_PROVIDER(m_css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	update_background_css();

	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(m_category_buttons));
	gtk_style_context_add_class(context, "categories");
	gtk_style_context_add_class(context, "right");

	// Show widgets
	gtk_widget_show_all(GTK_WIDGET(m_frame));

	// NOTE: gtk_widget_show_all above unconditionally reveals the apps/places
	// mode selector. update_layout() is the source of truth for its visibility,
	// but show() only invokes update_layout() when one of the layout booleans
	// changed — and for presets whose layout booleans happen to match the
	// constructor defaults (e.g. Classic), it never fires on first open. Sync
	// the mode-selector and its separator with places_enabled here so the
	// initial state matches the toggle even when update_layout() is skipped.
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

	// Places mode property-change subscriptions (milestone 005).
	if (m_settings->channel)
	{
		m_places_property_slot = g_signal_connect(m_settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue*, gpointer user_data) -> void
			{
				auto* self = static_cast<Window*>(user_data);
				if (g_strcmp0(property, "/places/enabled") == 0)
				{
					self->update_layout();
					if (!self->m_settings->places_enabled && self->m_places_active)
					{
						self->switch_mode(false);
					}
				}
				else if (g_strcmp0(property, "/places/history-enabled") == 0
						|| g_strcmp0(property, "/places/favourites-enabled") == 0)
				{
					self->update_layout();
				}
				else if (g_strcmp0(property, "/places/favourite-sync") == 0)
				{
					self->m_places->get_favourites_section()->refresh_mode();
					if (self->m_places_active)
						self->m_places->refresh_active();
				}
			}), this);
	}

	// Re-evaluate RGBA visual and redraw when corner-radius changes so that
	// the rounded-rect clip path is activated/deactivated without reopening the menu.
	if (m_settings->channel)
	{
		g_signal_connect(m_settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue*, gpointer user_data) -> void
			{
				if (g_strcmp0(property, "/corner-radius") != 0
						&& g_strcmp0(property, "/categories-opacity") != 0
						&& g_strcmp0(property, "/apps-opacity") != 0
						&& g_strcmp0(property, "/full-screen-opacity") != 0)
					return;
				// The corner radius is applied entirely by re-clipping and
				// re-stroking in on_draw_event; the redraw queued below picks up
				// the new radius live. No frame-shadow toggle remains.
				auto* self = static_cast<Window*>(user_data);
				self->update_background_css();
				self->on_screen_changed(GTK_WIDGET(self->m_window));
				gtk_widget_queue_draw(GTK_WIDGET(self->m_window));
			}), this);
	}

	// Load applications
	m_applications->load();

	g_object_ref_sink(m_window);
}

//-----------------------------------------------------------------------------

WhiskerMenu::Window::~Window()
{
	for (int i = 0; i < 9; ++i)
	{
		g_signal_handler_disconnect(m_commands_button[i], m_command_slots[i]);
		gtk_container_remove(GTK_CONTAINER(m_commands_box), m_commands_button[i]);
	}

	if (m_places_property_slot && m_settings && m_settings->channel)
	{
		g_signal_handler_disconnect(m_settings->channel, m_places_property_slot);
		m_places_property_slot = 0;
	}

	delete m_places;
	delete m_places_home_btn;
	delete m_places_history_btn;
	delete m_places_fav_btn;

	delete m_applications;
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

	if (m_search_cluster)
	{
		g_object_unref(m_search_cluster);
		m_search_cluster = nullptr;
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
	g_object_unref(m_window);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::hide(bool lost_focus)
{
	// Persist Places last-mode so the next open can resume in the same mode.
	if (m_settings->places_enabled && m_settings->places_remember_last_mode)
	{
		m_settings->places_last_mode = m_places_active ? "places" : "apps";
	}

	// Save settings
	m_settings->favorites.save();
	m_settings->recent.save();

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

	// Restore last mode when Places is enabled and remember-last-mode is on.
	if (m_settings->places_enabled && m_settings->places_remember_last_mode
			&& (g_strcmp0(m_settings->places_last_mode, "places") == 0))
	{
		switch_mode(true);
	}
	else if (m_places_active && !m_settings->places_enabled)
	{
		switch_mode(false);
	}

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

	// Make sure commands are valid and visible
	for (auto command : m_settings->command)
	{
		command->check();
	}

	// Make sure recent item count is within max
	m_recent->enforce_item_count();

	// Make sure recent button is only visible when tracked
	gtk_widget_set_visible(m_recent->get_button()->get_widget(), m_settings->recent_items_max);

	// Make sure applications list is current; does nothing unless list has changed
	if (m_applications->load())
	{
		set_loaded();
	}
	else
	{
		m_plugin->set_loaded(false);
		gtk_stack_set_visible_child_name(m_window_stack, "load");
		gtk_spinner_start(m_window_load_spinner);
	}

	// Update default page
	reset_default_button();
	show_default_page();

	// Clear any previous selection
	m_favorites->reset_selection();
	m_recent->reset_selection();
	m_applications->reset_selection();

	// Make sure icon sizes are correct. The Places section buttons are reloaded
	// on the SAME trigger as the Apps category buttons so sidebar label
	// visibility (names vs icon-only) is identical in both modes — both consult
	// the one shared decision in CategoryButton::reload_icon_size (FR-015/016).
	m_favorites->get_button()->reload_icon_size();
	m_recent->get_button()->reload_icon_size();
	m_applications->get_button()->reload_icon_size();
	m_places_home_btn->reload_icon_size();
	m_places_history_btn->reload_icon_size();
	m_places_fav_btn->reload_icon_size();

	m_applications->reload_category_icon_size();

	m_search_results->get_view()->reload_icon_size();
	m_favorites->get_view()->reload_icon_size();
	m_recent->get_view()->reload_icon_size();
	m_applications->get_view()->reload_icon_size();
	m_places->get_view()->reload_icon_size();

	GdkMonitor* monitor_gdk = nullptr;
	// Centered always references the panel button's monitor (FR-004), so it takes
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
	// T044: new string settings (sidebar-position, search-bar-position, profile-position,
	//       commands-position) take precedence over the legacy boolean toggles.
	const bool layout_ltr = gtk_widget_get_default_direction() != GTK_TEXT_DIR_RTL;

	const char* sidebar_pos  = m_settings->sidebar_position;
	const char* search_pos   = m_settings->search_bar_position;
	const char* profile_pos  = m_settings->profile_position;
	const char* commands_pos = m_settings->commands_position;

	// Map string settings → layout booleans
	// sidebar_position: "left" (default) / "right" (alternate) / "hidden"
	// cats_alt=true → update_layout puts sidebar at column 0 (left side)
	const bool cats_alt = (g_strcmp0(sidebar_pos, "left") == 0)
			|| (g_strcmp0(sidebar_pos, "hidden") != 0 && g_strcmp0(sidebar_pos, "right") != 0
				&& m_settings->position_categories_alternate);
	// search_bar_position: "bottom" = alternate
	const bool search_alt = (g_strcmp0(search_pos, "bottom") == 0)
			|| (g_strcmp0(search_pos, "top") != 0 && m_settings->position_search_alternate);
	// profile_position: "bottom" or "bottom-right" = alternate (bottom)
	const bool profile_alt = (g_strcmp0(profile_pos, "bottom") == 0
			|| g_strcmp0(profile_pos, "bottom-right") == 0)
			|| (g_strcmp0(profile_pos, "top") != 0 && g_strcmp0(profile_pos, "hidden") != 0
				&& m_settings->position_profile_alternate);
	// commands_position: "bottom-right" = alternate
	const bool commands_alt = (g_strcmp0(commands_pos, "bottom-right") == 0)
			|| (g_strcmp0(commands_pos, "top-right") != 0 && g_strcmp0(commands_pos, "hidden") != 0
				&& m_settings->position_commands_alternate);
	// NOTE: hidden state is tracked independently of the edge flags above. A
	// Hidden ↔ visible transition (e.g. "hidden" → "top") leaves *_alternate
	// unchanged, so without these comparisons show() would skip update_layout()
	// and the restored element would never re-render (defect 2, FR-005/006).
	const bool profile_hidden  = (g_strcmp0(profile_pos, "hidden") == 0);
	const bool commands_hidden = (g_strcmp0(commands_pos, "hidden") == 0);

	// NOTE: schema v2 — horizontal-categories is derived from sidebar-position
	// (top|bottom); the legacy /position-categories-horizontal key is migrated
	// away in Settings::migrate_schema().
	// Horizontal categories only when the sidebar is enabled AND on top/bottom;
	// a disabled sidebar shows no strip regardless of the stored position.
	const bool cats_horizontal = m_settings->sidebar_enabled
			&& ((g_strcmp0(sidebar_pos, "top") == 0)
				|| (g_strcmp0(sidebar_pos, "bottom") == 0));
	const bool unified_eff = unified_bar_effective(*m_settings);
	// Feature 020: these stored settings are not legacy layout booleans, but
	// they change the switch/sidebar presentation, so include them in the
	// relayout trigger (orientation, icon-mode, enable/disable, heading).
	const bool sidebar_enabled = m_settings->sidebar_enabled;
	const bool switch_show_icons = m_settings->places_switch_show_icons;
	const bool category_show_name = m_settings->category_show_name;
	// The Apps/Places toggle inherits the category icon size when it lives in a
	// sidebar, so a live category-icon-size edit must re-run update_layout() to
	// resize the toggle (the category buttons reload unconditionally below).
	const int category_icon_size = m_settings->category_icon_size;
	if ((m_layout_ltr != layout_ltr)
			|| (m_layout_categories_horizontal != cats_horizontal)
			|| (m_layout_sidebar_enabled != sidebar_enabled)
			|| (m_layout_switch_show_icons != switch_show_icons)
			|| (m_layout_category_show_name != category_show_name)
			|| (m_layout_category_icon_size != category_icon_size)
			|| (m_layout_categories_alternate != cats_alt)
			|| (m_layout_search_alternate != search_alt)
			|| (m_layout_commands_alternate != commands_alt)
			|| (m_layout_profile_alternate != profile_alt)
			|| (m_layout_profile_hidden != profile_hidden)
			|| (m_layout_commands_hidden != commands_hidden)
			|| (m_layout_unified_bar != unified_eff)
			|| (m_profile_shape != m_settings->profile_shape))
	{
		m_layout_ltr = layout_ltr;
		m_layout_categories_horizontal = cats_horizontal;
		m_layout_sidebar_enabled = sidebar_enabled;
		m_layout_switch_show_icons = switch_show_icons;
		m_layout_category_show_name = category_show_name;
		m_layout_category_icon_size = category_icon_size;
		m_layout_categories_alternate = cats_alt;
		m_layout_search_alternate = search_alt;
		m_layout_commands_alternate = commands_alt;
		m_layout_profile_alternate = profile_alt;
		m_layout_profile_hidden = profile_hidden;
		m_layout_commands_hidden = commands_hidden;
		m_profile->update_picture();
		m_profile_shape = m_settings->profile_shape;
		update_layout();
	}

	// Sidebar visibility now follows the Enable-sidebar switch (FR-022/032);
	// update_layout() owns the in-strip/relocated cases, this is the docked
	// vertical-sidebar show/hide. (Legacy "hidden" position migrated away.)
	gtk_widget_set_visible(GTK_WIDGET(m_sidebar),
			sidebar_enabled && !cats_horizontal);

	// T045: FullScreen mode + size-sensitive layout tweaks
	const bool is_fullscreen = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);

	// Apply mode-dependent child size requests *before* resizing the toplevel.
	// This prevents stale fullscreen requests from forcing docked presets wider
	// than their configured menu-width when switching back from FullScreen.
	if (is_fullscreen)
	{
		// Centre the search bar in a fixed half-width column (issue #3). FILL plus
		// symmetric side margins pin the column to exactly workarea/2 regardless
		// of the search entry's natural width or whether the Apps/Places switch
		// shares the row, so entry + switch together never grow past the width the
		// entry alone occupies with Places off. A bare halign CENTER would instead
		// grow to the children's natural width and let the switch widen the row.
		const int column_width  = m_workarea.width / 2;
		const int column_margin = (m_workarea.width - column_width) / 2;
		gtk_widget_set_halign(GTK_WIDGET(m_search_box), GTK_ALIGN_FILL);
		gtk_widget_set_size_request(GTK_WIDGET(m_search_box), -1, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_search_box), column_margin);
		gtk_widget_set_margin_end(GTK_WIDGET(m_search_box), column_margin);
		// Full-screen strip parity (FR-010/011/019/020): the Top/Bottom category
		// strip shares that exact column. Identical FILL + margins put the leading
		// toggle on the search bar's left edge and the trailing category icons on
		// its right edge, with the slack between them. Set on every relayout so
		// docked↔full-screen and orientation flips reflow with no stale margins
		// (FR-014).
		gtk_widget_set_halign(GTK_WIDGET(m_categories_box), GTK_ALIGN_FILL);
		gtk_widget_set_margin_start(GTK_WIDGET(m_categories_box), column_margin);
		gtk_widget_set_margin_end(GTK_WIDGET(m_categories_box), column_margin);
		// Keep sidebar width meaningful, and compensate on the opposite side so
		// the applications grid stays centered.
		// NOTE (FR-026/027): the symmetry margin is a fixed fraction of the work
		// area and is deliberately independent of the icon/category-name toggles
		// and the relocated switch. The switch, when the sidebar is disabled,
		// packs into m_title_box (the unified-bar row) rather than the
		// m_contents_box grid columns, so this void around the results box is
		// never perturbed. Keep both invariants when editing.
		const int sidebar_width = m_workarea.width / 6;
		const bool sidebar_on_left = (m_layout_ltr == m_layout_categories_alternate);
		gtk_widget_set_size_request(GTK_WIDGET(m_sidebar), sidebar_width, -1);
		// The one-sided compensating margin is correct *only* when a vertical
		// sidebar actually occupies one side. The sidebar is shown only when it is
		// enabled and not laid out as a Top/Bottom strip; in every other case
		// (sidebar disabled, or a horizontal strip) no widget fills that side, so a
		// one-sided margin would shove the results box off-centre — exactly the
		// "no void on the left" symptom. Mirror the margin on both sides there to
		// keep the grid centred with symmetric voids, aligned with the centred
		// search bar (FR-023/SC-007).
		const bool vertical_sidebar_visible = sidebar_enabled && !cats_horizontal;
		if (vertical_sidebar_visible)
		{
			gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack), sidebar_on_left ? 0 : sidebar_width);
			gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack), sidebar_on_left ? sidebar_width : 0);
		}
		else
		{
			gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack), sidebar_width);
			gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack), sidebar_width);
		}
	}
	else
	{
		gtk_widget_set_halign(GTK_WIDGET(m_search_box), GTK_ALIGN_FILL);
		gtk_widget_set_size_request(GTK_WIDGET(m_search_box), -1, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_search_box), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_search_box), 0);
		gtk_widget_set_size_request(GTK_WIDGET(m_sidebar), -1, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack), 0);
		// Docked: the strip fills the menu width so the toggle reaches the leading
		// edge and the categories the trailing edge of the menu (FR-005/008).
		gtk_widget_set_halign(GTK_WIDGET(m_categories_box), GTK_ALIGN_FILL);
		gtk_widget_set_margin_start(GTK_WIDGET(m_categories_box), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_categories_box), 0);
	}

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

void WhiskerMenu::Window::set_categories(const std::vector<CategoryButton*>& categories)
{
	CategoryButton* last_button = m_applications->get_button();
	m_app_category_widgets.clear();
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
			[this](GtkToggleButton*)
			{
				category_toggled();
			});
	}

	// NOTE: if Places mode is already active when categories arrive, keep
	// the application categories hidden so the sidebar matches the mode.
	if (m_places_active)
	{
		for (GtkWidget* w : m_app_category_widgets)
		{
			gtk_widget_set_visible(w, false);
		}
	}

	show_default_page();
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
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::unset_items()
{
	m_search_results->unset_menu_items();
	m_favorites->unset_menu_items();
	m_recent->unset_menu_items();
}

//-----------------------------------------------------------------------------

/* user_session_row_visible:
 * @unified:              true when the unified search/profile/session bar is active.
 * @profile_hidden:       true when profile_position == "hidden".
 * @commands_hidden:      true when commands_position == "hidden".
 * @categories_alternate: unused; retained for signature stability with the
 *                        mirrored unit test.
 *
 * Pure visibility decision for the docked user/session row (m_title_box). In a
 * unified-bar / full-screen layout the row hosts the centred search cluster, so
 * it is always kept and only the profile/commands clusters hide (FR-004). In a
 * docked layout the row is present whenever either cluster is visible and
 * collapses only when both are hidden, so no empty strip remains (FR-003).
 *
 * NOTE: row visibility must NOT depend on category placement. The previous
 * `return !categories_alternate` branch coupled the row's existence to where
 * the category list sat, which suppressed a visible commands cluster whenever
 * the categories were at the bottom (defect 1, FR-001).
 *
 * Returns: true if the user/session row should be visible.
 */
static bool user_session_row_visible(bool unified, bool profile_hidden,
                                     bool commands_hidden, bool /*categories_alternate*/)
{
	if (unified)
		return true;
	return !(profile_hidden && commands_hidden);
}

//-----------------------------------------------------------------------------

Keyboard::VisibilityMask WhiskerMenu::Window::current_visibility_mask() const
{
	// FR-030: Search and Results are never hidden. Optional zones follow
	// the preset's per-zone "hidden" position string and the legacy
	// visibility flags. The Apps/Places toggle is not a focus zone (FR-007),
	// so it has no entry here; Tab reads places_enabled directly.
	Keyboard::VisibilityMask mask;
	mask.search  = true;
	mask.results = true;

	const char* sidebar_pos  = m_settings->sidebar_position;
	const char* commands_pos = m_settings->commands_position;

	mask.sidebar = (g_strcmp0(sidebar_pos, "hidden") != 0);

	// The "profile" focus zone is the session-command row: only the command
	// buttons are focusable (the avatar/username are not), so a hidden profile
	// contributes nothing focusable. The zone is therefore gated on
	// commands_position != "hidden" with at least one visible command, which
	// already keeps focus off both hidden commands and a hidden profile (FR-005).
	//
	// NOTE: availability must also require gtk_widget_get_can_focus, matching
	// grab_focus_in_zone()'s own target predicate. A command button can be
	// visible yet non-focusable; reporting such a Profile zone "available"
	// would let next_zone() route Ctrl+Tab to it, the grab would silently fail,
	// and forward cycling would stall in Results (the US3 defect). Tying
	// availability to focusability keeps the abstract mask consistent with what
	// the grab can actually land (FR-010).
	bool any_command_focusable = false;
	if (g_strcmp0(commands_pos, "hidden") != 0)
	{
		for (int i = 0; i < 9; ++i)
		{
			if (m_commands_button[i]
					&& gtk_widget_get_visible(m_commands_button[i])
					&& gtk_widget_get_can_focus(m_commands_button[i]))
			{
				any_command_focusable = true;
				break;
			}
		}
	}
	mask.profile = any_command_focusable;

	return mask;
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

bool WhiskerMenu::Window::grab_focus_in_zone(Keyboard::Zone zone)
{
	GtkWidget* target = nullptr;
	Page* results_page = nullptr;

	switch (zone)
	{
	case Keyboard::Zone::Search:
		target = GTK_WIDGET(m_search_entry);
		break;

	case Keyboard::Zone::Results:
	{
		// FR-032: on Tab entry, land on the currently selected item; if
		// nothing is selected, select the first item so the user has a
		// visible anchor (select_first() is called after the grab below).
		// NOTE: in Places mode get_active_page() returns the hidden applications
		// page; target m_places directly so the grab lands on the visible widget.
		if (m_places_active)
		{
			target = m_places->get_view()->get_widget();
		}
		else
		{
			Page* page = const_cast<Window*>(this)->get_active_page();
			if (page)
			{
				target = page->get_view()->get_widget();
				results_page = page;
			}
		}
		break;
	}

	case Keyboard::Zone::Sidebar:
		target = const_cast<Window*>(this)->get_active_category_button();
		break;

	case Keyboard::Zone::Profile:
		// First visible+focusable command button in physical box order.
		for (int i = 0; i < 9; ++i)
		{
			if (m_commands_button[i]
					&& gtk_widget_get_visible(m_commands_button[i])
					&& gtk_widget_get_can_focus(m_commands_button[i]))
			{
				target = m_commands_button[i];
				break;
			}
		}
		break;
	}

	// A grab can only land on a target that is visible, sensitive and
	// focusable; mirror that predicate and report the outcome so the forward
	// Ctrl+Tab loop can advance past a zone whose grab would silently fail
	// (FR-010). gtk_widget_is_focus() confirms the target became the toplevel's
	// focus widget rather than assuming the grab took effect.
	if (target
			&& gtk_widget_get_visible(target)
			&& gtk_widget_get_sensitive(target)
			&& gtk_widget_get_can_focus(target))
	{
		gtk_widget_grab_focus(target);
		if (results_page)
		{
			GtkTreePath* sel = results_page->get_view()->get_selected_path();
			if (!sel)
			{
				results_page->select_first();
			}
			else
			{
				gtk_tree_path_free(sel);
			}
		}
		return gtk_widget_is_focus(target);
	}
	return false;
}

//-----------------------------------------------------------------------------

Keyboard::Zone WhiskerMenu::Window::current_zone() const
{
	GtkWidget* focused = gtk_window_get_focus(m_window);
	if (!focused)
	{
		return Keyboard::Zone::Search;
	}

	// Walk ancestors; the first recognised container wins. Order matters
	// here — the search entry sits inside m_search_box which is a child
	// of m_vbox, so test the entry directly before any box ancestor.
	if (focused == GTK_WIDGET(m_search_entry))
	{
		return Keyboard::Zone::Search;
	}

	// NOTE: the Apps/Places toggle box is intentionally not matched here:
	// its buttons are non-focusable (FR-007) so focus can never rest in it.
	GtkWidget* sidebar   = m_sidebar            ? GTK_WIDGET(m_sidebar)            : nullptr;
	GtkWidget* cats_box  = m_categories_box     ? GTK_WIDGET(m_categories_box)     : nullptr;
	GtkWidget* cmds_box  = m_commands_box       ? GTK_WIDGET(m_commands_box)       : nullptr;
	GtkWidget* panels    = m_panels_stack       ? GTK_WIDGET(m_panels_stack)       : nullptr;

	for (GtkWidget* w = focused; w; w = gtk_widget_get_parent(w))
	{
		if (cmds_box && w == cmds_box)
			return Keyboard::Zone::Profile;
		if (sidebar && w == sidebar)
			return Keyboard::Zone::Sidebar;
		if (cats_box && w == cats_box)
			return Keyboard::Zone::Sidebar;
		if (panels && w == panels)
			return Keyboard::Zone::Results;
	}

	return Keyboard::Zone::Search;
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
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_key_press_event(GtkWidget* widget, GdkEventKey* key_event)
{
	// Type-to-search catch-all (FR-010, FR-012, FR-013, FR-015). This
	// runs BEFORE GTK's default key chain so that:
	//   - Printable keys typed while focus is on the results view do
	//     not reach the view's default Space-activates-row handler.
	//   - Each character is delivered to the entry exactly once (the
	//     post-default signal must not also re-route, otherwise every
	//     keystroke would be inserted twice once Window::search()
	//     moves focus to the view).
	// Backspace gets a dedicated branch (FR-013): it must remove a
	// character from the query regardless of which zone holds focus,
	// but it is not classified Printable (control codepoint) and
	// therefore cannot ride the generic printable redirect.
	GtkWidget* entry = GTK_WIDGET(m_search_entry);
	GtkWidget* focused = gtk_window_get_focus(m_window);
	if (focused && focused != entry)
	{
		if (Keyboard::is_printable_for_search(key_event)
				|| key_event->keyval == GDK_KEY_BackSpace)
		{
			gtk_entry_grab_focus_without_selecting(m_search_entry);
			gtk_window_propagate_key_event(m_window, key_event);
			return GDK_EVENT_STOP;
		}
	}

	// Tab family is intercepted before GTK's default focus-chain and split
	// on the Control modifier:
	//   - bare Tab / Shift+Tab        → Applications⇄Places mode toggle
	//                                    (FR-001, FR-002, FR-006);
	//   - Ctrl+Tab / Ctrl+Shift+Tab   → canonical focus-area cycling
	//                                    (FR-004, FR-005, FR-007).
	// The two are mutually exclusive on GDK_CONTROL_MASK and both always
	// consume the event so GTK's own Tab traversal never also fires.
	if (key_event->keyval == GDK_KEY_Tab
			|| key_event->keyval == GDK_KEY_ISO_Left_Tab
			|| key_event->keyval == GDK_KEY_KP_Tab)
	{
		if (key_event->state & GDK_CONTROL_MASK)
		{
			// Ctrl+Tab forward, Ctrl+Shift+Tab (or ISO_Left_Tab) backward.
			const Keyboard::Direction dir =
					((key_event->state & GDK_SHIFT_MASK)
					 || key_event->keyval == GDK_KEY_ISO_Left_Tab)
						? Keyboard::Direction::Backward
						: Keyboard::Direction::Forward;
			const Keyboard::Zone here = current_zone();
			const Keyboard::VisibilityMask mask = current_visibility_mask();
			const Keyboard::MenuState state = current_menu_state();

			if (dir == Keyboard::Direction::Forward)
			{
				// Forward cycling is self-correcting. Advance to the next
				// candidate zone; if its grab does not actually land (a region
				// the mask reported available is currently unfocusable), keep
				// advancing. Walk at most once around CANONICAL_CYCLE: if the
				// search wraps back to `here` (or stops making progress) no
				// other zone can take focus, so the press is a harmless no-op
				// and focus stays put (FR-010, FR-011). With the focusability-
				// aware mask the first grab normally lands; this loop is the
				// defensive guarantee against any residual widget-state drift.
				Keyboard::Zone candidate = here;
				for (std::size_t step = 0; step < Keyboard::CANONICAL_CYCLE.size(); ++step)
				{
					const Keyboard::Zone next =
							Keyboard::next_zone(mask, state, candidate, dir);
					if (next == here || next == candidate)
					{
						break; // wrapped around / no progress → no-op
					}
					if (grab_focus_in_zone(next))
					{
						break; // grab landed on a focusable zone
					}
					candidate = next; // dead zone; try the one after it
				}
			}
			else
			{
				// Reverse cycling (Ctrl+Shift+Tab) is out of scope and left
				// behaviorally unchanged: a single step with no grab-retry.
				const Keyboard::Zone next =
						Keyboard::next_zone(mask, state, here, dir);
				if (next != here)
				{
					grab_focus_in_zone(next);
				}
			}
			return GDK_EVENT_STOP;
		}

		// Bare Tab/Shift+Tab toggles the mode (or is inert when Places is
		// unavailable). FR-006: Tab never silently falls back to area
		// cycling, so the Inert arm consumes the event and changes nothing.
		switch (Keyboard::tab_action(m_settings->places_enabled))
		{
		case Keyboard::TabAction::ToggleMode:
			// switch_mode resets the new view to its default selection and
			// clears the query; the Tab path then lands focus on Results so
			// the user sees the new view's default item (FR-003).
			switch_mode(!m_places_active);
			grab_focus_in_zone(Keyboard::Zone::Results);
			break;
		case Keyboard::TabAction::Inert:
			break;
		}
		return GDK_EVENT_STOP;
	}

	if (key_event->keyval == GDK_KEY_Escape)
	{
		// Esc ladder (FR-060, FR-061). Strict priority:
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
			// launcher (FR-051).
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
			for (Resizer* resizer : m_resize)
			{
				resizer->cancel();
			}
			set_size(m_settings->menu_width, m_settings->menu_height);
			resize_end();
			break;

		case Keyboard::EscAction::ClearQuery:
			gtk_entry_set_text(m_search_entry, "");
			// FR-014 follow-up: focus returns to the entry so the user is
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

	Page* page = get_active_page();
	// NOTE: in Places mode get_active_page() returns the hidden applications
	// page; use m_places directly so focus and selection target the visible view.
	GtkWidget* view = m_places_active
		? m_places->get_view()->get_widget()
		: page->get_view()->get_widget();
	GtkWidget* search = GTK_WIDGET(m_search_entry);

	// FR-043 / FR-046: sidebar exit arrow → focus the results view. The
	// "outward" arrow depends on sidebar side (left/right/top/bottom),
	// which is encoded by m_layout_categories_horizontal,
	// m_layout_categories_alternate and m_layout_ltr. Mirror is applied
	// via gtk_widget_get_default_direction() so RTL behaves correctly.
	// In Searching state the sidebar is inert and the exit arrow is a
	// no-op (FR-046).
	if (current_zone() == Keyboard::Zone::Sidebar
			&& current_menu_state() == Keyboard::MenuState::Browsing)
	{
		guint outward = 0;
		if (m_layout_categories_horizontal)
		{
			// alternate=true → sidebar at bottom → outward is Up
			outward = m_layout_categories_alternate
					? GDK_KEY_Up : GDK_KEY_Down;
		}
		else
		{
			// sidebar_on_left when (m_layout_ltr == m_layout_categories_alternate)
			const bool sidebar_on_left
					= (m_layout_ltr == m_layout_categories_alternate);
			outward = sidebar_on_left ? GDK_KEY_Right : GDK_KEY_Left;
		}
		const guint kv = key_event->keyval;
		const bool match_outward
				= (kv == outward)
				|| (outward == GDK_KEY_Up    && kv == GDK_KEY_KP_Up)
				|| (outward == GDK_KEY_Down  && kv == GDK_KEY_KP_Down)
				|| (outward == GDK_KEY_Left  && kv == GDK_KEY_KP_Left)
				|| (outward == GDK_KEY_Right && kv == GDK_KEY_KP_Right);
		if (match_outward)
		{
			// FR-007 / C4.1: the outward arrow enters the results of the
			// COMMITTED category. Along-axis navigation never auto-commits in
			// hover-OFF mode, so `view` (resolved from get_active_page above)
			// already points at the committed category's view — no implicit
			// commit of a highlighted-but-uncommitted category happens here.
			gtk_widget_grab_focus(view);
			return GDK_EVENT_STOP;
		}

		// Along-axis category navigation (FR-001/002/009). Centralized here so
		// the event is consumed BEFORE GTK's default radio-group key navigation,
		// which would otherwise both move focus AND auto-activate the next radio
		// — the activation that previously ejected focus to the search entry.
		// The along-axis arrows are perpendicular to the outward arrow computed
		// above: vertical sidebar → Up/Down, horizontal sidebar → Left/Right.
		GtkWidget* sidebar_focused = gtk_window_get_focus(m_window);
		GtkWidget* parent = sidebar_focused ? gtk_widget_get_parent(sidebar_focused) : nullptr;
		if (parent && GTK_IS_RADIO_BUTTON(sidebar_focused))
		{
			const bool vertical = !GTK_IS_ORIENTABLE(parent)
					|| gtk_orientable_get_orientation(GTK_ORIENTABLE(parent))
							== GTK_ORIENTATION_VERTICAL;
			const bool is_prev = vertical
					? (kv == GDK_KEY_Up || kv == GDK_KEY_KP_Up)
					: (kv == GDK_KEY_Left || kv == GDK_KEY_KP_Left);
			const bool is_next = vertical
					? (kv == GDK_KEY_Down || kv == GDK_KEY_KP_Down)
					: (kv == GDK_KEY_Right || kv == GDK_KEY_KP_Right);
			const bool is_home = (kv == GDK_KEY_Home || kv == GDK_KEY_KP_Home);
			const bool is_end  = (kv == GDK_KEY_End  || kv == GDK_KEY_KP_End);

			if (is_prev || is_next || is_home || is_end)
			{
				GList* siblings = focusable_category_siblings(GTK_CONTAINER(parent));
				if (siblings)
				{
					GList* self = g_list_find(siblings, sidebar_focused);
					GtkWidget* target = nullptr;
					if (is_home)
					{
						target = GTK_WIDGET(siblings->data);
					}
					else if (is_end)
					{
						target = GTK_WIDGET(g_list_last(siblings)->data);
					}
					else if (self && is_next)
					{
						// Wrap past the last sibling back to the first.
						target = GTK_WIDGET(self->next ? self->next->data : siblings->data);
					}
					else if (self && is_prev)
					{
						// Wrap before the first sibling round to the last.
						target = GTK_WIDGET(self->prev ? self->prev->data
								: g_list_last(siblings)->data);
					}
					g_list_free(siblings);

					if (target && target != sidebar_focused)
					{
						keyboard_navigate_category(target);
					}
					// Single navigable category (or already at the Home/End
					// target): a harmless no-op that keeps focus in the sidebar
					// rather than falling through to the default chain (SC-002).
					return GDK_EVENT_STOP;
				}
			}

			// Enter/Space commits the highlighted category while keeping focus in
			// the sidebar (FR-008b). In hover-ON the target is already active, so
			// this is a no-op activation; in hover-OFF it is the explicit commit
			// that updates the results. Consumed under the guard either way.
			if (kv == GDK_KEY_Return || kv == GDK_KEY_KP_Enter
					|| kv == GDK_KEY_space || kv == GDK_KEY_KP_Space)
			{
				m_keyboard_category_nav = true;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sidebar_focused), true);
				m_keyboard_category_nav = false;
				return GDK_EVENT_STOP;
			}
		}
	}

	switch (key_event->keyval)
	{
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left:
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right:
		// Allow keyboard navigation out of treeview. NOTE: in Searching
		// state the sidebar is inert (FR-046); the exit arrow MUST be a
		// no-op so the user can keep refining the query with arrow keys
		// inside the result list.
		if (GTK_IS_TREE_VIEW(view) && ((widget == view) || (gtk_window_get_focus(m_window) == view)))
		{
			if (current_menu_state() == Keyboard::MenuState::Browsing)
			{
				gtk_widget_grab_focus(get_active_category_button());
			}
		}
		// Allow keyboard navigation out of search into iconview
		else if (GTK_IS_ICON_VIEW(view) && ((widget == search) || (gtk_window_get_focus(m_window) == search)))
		{
			const auto length = gtk_entry_get_text_length(m_search_entry);
			const bool at_end = length && (gtk_editable_get_position(GTK_EDITABLE(m_search_entry)) == length);
			const bool move_next = (gtk_widget_get_default_direction() != GTK_TEXT_DIR_RTL)
					? (key_event->keyval == GDK_KEY_Right) : (key_event->keyval == GDK_KEY_Left);
			if (at_end && move_next)
			{
				gtk_widget_grab_focus(view);
			}
		}
		break;

	// Make up and down keys scroll current list of applications from search
	//
	// NOTE: arrow navigation drives only the GTK cursor and single selection;
	// there is no separate "highlight" state. GTK moves the cursor on Up/Down
	// and the single-selection follows it, so the selection is the sole
	// highlight driver for keyboard navigation (FR-002). With pointer prelight
	// neutralised in the plugin CSS, exactly one row is ever painted.
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
	{
		// Determine if there is a selected item
		LauncherView* results_view = m_places_active
			? m_places->get_view() : page->get_view();
		bool reset = m_places_active ? true : (page != m_search_results);
		if (reset)
		{
			GtkTreePath* path = results_view->get_selected_path();
			if (path)
			{
				reset = false;
				gtk_tree_path_free(path);
			}
		}
		// Allow keyboard navigation out of search into view
		if ((widget == search) || (gtk_window_get_focus(m_window) == search))
		{
			gtk_widget_grab_focus(view);
		}
		// Only select first item if there is no selected item. This anchors a
		// single valid selection on first arrow entry; combined with the model
		// rebuild + select_first() on every query change (SearchPage::set_filter)
		// it guarantees the highlight resets to one valid row or none, with no
		// stale carry-over from the prior result set (FR-006).
		if ((gtk_window_get_focus(m_window) == view) && reset)
		{
			m_places_active ? m_places->select_first() : page->select_first();
			return GDK_EVENT_STOP;
		}
		break;
	}

	// Pass PageUp and PageDown keys to current view. FR-047: when focus
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
	// activates the selected row" default binding (FR-012). Keeping
	// a duplicate redirect here would cause every keystroke to be
	// inserted twice once Window::search() moves focus to the view
	// per FR-011. The post-default slot remains attached as a hook
	// point for future expansion; today it is a no-op.
	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_map_event()
{
	gtk_window_set_keep_above(m_window, true);

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

	update_background_css();
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

	const int red   = CLAMP(static_cast<int>(bg.red   * 255.0 + 0.5), 0, 255);
	const int green = CLAMP(static_cast<int>(bg.green * 255.0 + 0.5), 0, 255);
	const int blue  = CLAMP(static_cast<int>(bg.blue  * 255.0 + 0.5), 0, 255);
	// Separator colour for the docked results-area outline. The docked window
	// shell is fully transparent (so apps-opacity 0 reaches true transparency),
	// which means the inter-region gaps the window background used to fill are
	// no longer painted. The visible boundaries all sit around the results area,
	// so a single opaque 1 px outline owned by that region restores them without
	// putting any paint behind it. Nudge the seed toward or away from black by a
	// fixed step depending on the theme's lightness so the line reads on both
	// dark and light themes.
	const double sep_luma = 0.299 * red + 0.587 * green + 0.114 * blue;
	const int sep_step = 45;
	const int sep_r = (sep_luma < 128.0) ? CLAMP(red   + sep_step, 0, 255) : CLAMP(red   - sep_step, 0, 255);
	const int sep_g = (sep_luma < 128.0) ? CLAMP(green + sep_step, 0, 255) : CLAMP(green - sep_step, 0, 255);
	const int sep_b = (sep_luma < 128.0) ? CLAMP(blue  + sep_step, 0, 255) : CLAMP(blue  - sep_step, 0, 255);
	// Publish the seed for on_draw_event: the single window border stroke reuses
	// exactly this theme-derived colour, so the boundary keeps its previous hue
	// even though it is now drawn once along the rounded path instead of as a
	// per-region CSS outline.
	m_separator_rgba.red   = sep_r / 255.0;
	m_separator_rgba.green = sep_g / 255.0;
	m_separator_rgba.blue  = sep_b / 255.0;
	m_separator_rgba.alpha = 1.0;
	// Resolve which absolute alpha each region receives for this render pass.
	// The pure helper enforces the 0=transparent / 100=solid contract and the
	// dual single-alpha model: docked => transparent window with each region
	// owning its alpha (so apps-opacity 0 reaches true transparency with no
	// categories floor showing through); full-screen => the window shell owns
	// the one full-screen alpha and the regions are transparent (so the whole
	// surface, results area included, reads at exactly that one value). Neither
	// mode lets two backgrounds compound.
	const bool is_fullscreen = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);
	const OpacityRegionAlphas alphas = meowmenu_region_alphas(
			is_fullscreen,
			m_settings->categories_opacity,
			m_settings->apps_opacity,
			m_settings->full_screen_opacity);

	// Chrome/frame fill used by on_draw_event for the resizer ring: the theme
	// background at the categories-region alpha, so the 6 px the resizer grid
	// reserves around the content reads as part of the menu frame (same alpha as
	// the search/title/commands strips) instead of showing the desktop through
	// the transparent docked shell.
	m_chrome_rgba.red   = red   / 255.0;
	m_chrome_rgba.green = green / 255.0;
	m_chrome_rgba.blue  = blue  / 255.0;
	m_chrome_rgba.alpha = alphas.categories;

	// NOTE: the results area no longer emits its own 1 px outline. The single
	// intentional menu border is drawn once in on_draw_event along the rounded
	// clip path (docked only); a per-region CSS border here would double that
	// line and ignore the corner radius. Full-screen continues to show no
	// outline at all (one seamless surface).
	gchar* css = g_strdup_printf(
		".meowmenu { background-image: none; background-color: rgba(%d, %d, %d, %.3f); }"
		// NOTE: GTK wraps an undecorated, client-side-decorated toplevel in a
		// `decoration` subnode that the active theme styles with a drop shadow
		// (and often its own border-radius/border/margin). That node is painted
		// outside everything on_draw_event controls, so it surfaces as a faint
		// translucent halo sitting just beyond the single custom border — the
		// ghost outline FR-003 forbids. Neutralise it so the only thing framing
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
		// NOTE: .contents stays transparent on purpose — it is an ancestor of
		// the applications area, so giving it a background would re-introduce a
		// solid floor under the results region and defeat true-0 apps opacity.
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
		// and theme (FR-008).
		".meowmenu .launchers:hover:not(:selected),"
		".meowmenu .launchers cell:hover:not(:selected),"
		".meowmenu .launchers row:hover:not(:selected)"
		"{ background-color: transparent; background-image: none; }"
		// Sidebar/categories region plus the chrome strips (search, title,
		// commands bars) share one absolute alpha so the menu frame stays
		// cohesive. In full-screen this alpha is 0 (transparent) so only the
		// window shell carries the single full-screen opacity.
		".meowmenu .categories,"
		".meowmenu .search-area,"
		".meowmenu .title-area,"
		".meowmenu .commands-area"
		"{ background-image: none; background-color: rgba(%d, %d, %d, %.3f); }"
		".meowmenu .applications-area,"
		".meowmenu .applications-area > *"
		"{ background-image: none; background-color: rgba(%d, %d, %d, %.3f); }"
		".meowmenu .category-button,"
		".meowmenu .category-button *,"
		".meowmenu .category-button image,"
		".meowmenu .category-button label"
		"{ opacity: 1; }"
		// Default-category heading shown when the sidebar is disabled
		// (FR-020). Uppercase text comes from the catalog strings; the
		// letter-spacing/weight here are theme-overridable.
		".meowmenu .meow-default-heading"
		"{ font-weight: bold; letter-spacing: 1px; opacity: 0.65;"
		"  margin: 6px 6px 2px 6px; }"
		// FR-002 / SC-007: keyboard focus indicator. The 1 px inset
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
		// NOTE: the Apps/Places switch is rendered as one rounded segmented
		// control. The outer corners use a large constant radius (9999px) that
		// CSS clamps to half the control height in any theme, so the group reads
		// as a full pill matching the search field's rounded ends. The radius is
		// a fixed styling rule on purpose: it is NOT read from the active theme at
		// runtime and is NOT bound to the /corner-radius menu setting, so the
		// switch keeps its pill shape regardless of the window corner radius. The
		// two buttons abut (m_mode_selector_box is built with zero spacing) and
		// share a single 1px seam line carried by the first button's inner edge —
		// never two abutting borders, never a gap. :dir(ltr)/:dir(rtl) mirror the
		// rounded ends and the seam for RTL with no C++ special-casing. No rule
		// here changes padding, margin, min-size, hit area, the theme-owned
		// hover/pressed/active fills, or the .meow-focus-ring focus outline.
		".meowmenu .places-mode-selector button { border-radius: 0; }"
		".meowmenu .places-mode-selector button:dir(ltr):first-child"
		"{ border-top-left-radius: 9999px; border-bottom-left-radius: 9999px;"
		"  border-right: 1px solid alpha(@theme_fg_color, 0.2); }"
		".meowmenu .places-mode-selector button:dir(ltr):last-child"
		"{ border-top-right-radius: 9999px; border-bottom-right-radius: 9999px; }"
		".meowmenu .places-mode-selector button:dir(rtl):first-child"
		"{ border-top-right-radius: 9999px; border-bottom-right-radius: 9999px;"
		"  border-left: 1px solid alpha(@theme_fg_color, 0.2); }"
		".meowmenu .places-mode-selector button:dir(rtl):last-child"
		"{ border-top-left-radius: 9999px; border-bottom-left-radius: 9999px; }",
		red, green, blue, alphas.window,
		red, green, blue, alphas.categories,
		red, green, blue, alphas.apps);

	gtk_css_provider_load_from_data(m_css_provider, css, -1, nullptr);
	g_free(css);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::on_screen_changed(GtkWidget* widget)
{
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
	update_background_css();
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
	// Covers both corner radius 0 (FR-002) and the non-composited fallback
	// (FR-006), which both render clean square corners.
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

void WhiskerMenu::Window::fill_resizer_ring(cairo_t* cr, double width, double height)
{
	// The content vbox sits in the centre cell of the 3x3 resizer grid, inset by
	// the drag-handle strip on every side. Fill that strip — the window rectangle
	// minus the vbox rectangle — with the chrome background so the menu frame is
	// continuous up to the border. The vbox itself is left untouched so the
	// regions inside it (notably the apps area) keep their own alpha.
	GtkAllocation va;
	gtk_widget_get_allocation(GTK_WIDGET(m_vbox), &va);
	if (va.width <= 0 || va.height <= 0)
		return;

	const double vx = va.x;
	const double vy = va.y;
	const double vw = va.width;
	const double vh = va.height;

	cairo_save(cr);
	gdk_cairo_set_source_rgba(cr, &m_chrome_rgba);
	if (vy > 0.0)                       // top strip
		cairo_rectangle(cr, 0.0, 0.0, width, vy);
	if (vy + vh < height)               // bottom strip
		cairo_rectangle(cr, 0.0, vy + vh, width, height - (vy + vh));
	if (vx > 0.0)                       // left strip (between top and bottom)
		cairo_rectangle(cr, 0.0, vy, vx, vh);
	if (vx + vw < width)                // right strip
		cairo_rectangle(cr, vx + vw, vy, width - (vx + vw), vh);
	cairo_fill(cr);
	cairo_restore(cr);
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_draw_event(GtkWidget* widget, cairo_t* cr)
{
	if (!gtk_widget_get_realized(widget))
	{
		gtk_widget_realize(widget);
	}

	GtkStyleContext* context = gtk_widget_get_style_context(widget);
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

	if (enabled && m_supports_alpha)
	{
		// Erase the previous frame so pixels outside the rounded clip are transparent.
		cairo_save(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
		cairo_paint(cr);
		cairo_restore(cr);

		// Clip the ENTIRE window draw — shell background AND the propagated child
		// regions — to the rounded silhouette. Post-026 the visible fill is painted
		// by the regions (the docked shell is transparent), so the clip must bound
		// the children for the radius to round what the user actually sees. At r==0
		// this is a plain rectangular clip, so corners reduce to clean squares.
		cairo_save(cr);
		clip_rounded(cr);
		cairo_clip(cr);
		gtk_render_background(context, cr, 0.0, 0.0, width, height);
		// Docked only: fill the resizer ring with the chrome background. The 3x3
		// resizer grid reserves a strip (the drag handles) around the content
		// vbox; with the transparent docked shell that strip would otherwise show
		// the desktop as a band between the border and the content. Painting only
		// the ring — the window minus the content vbox — keeps the apps area's own
		// alpha intact (no opaque floor under the results). Full-screen needs no
		// fill: there the shell itself carries the single full-screen alpha.
		if (!is_fullscreen && m_vbox)
			fill_resizer_ring(cr, width, height);
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
	// (FR-006). Draw the shell background and propagate the children unclipped,
	// then a single SQUARE border — gated on docked only (not the composited
	// predicate, which requires supports_alpha), so a non-composited full-screen
	// menu stays one seamless square surface with no outline.
	gtk_render_background(context, cr, 0.0, 0.0, width, height);
	if (!is_fullscreen && m_vbox)
		fill_resizer_ring(cr, width, height);
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

void WhiskerMenu::Window::move_window()
{
	// T042: apply panel-gap offset away from the panel.
	// Centered placement never sits flush against a panel edge, so the gap is
	// meaningless there and is suppressed without mutating the stored value
	// (the Properties dialog also greys the gap control in Centered).
	const int gap = m_settings->panel_gap;
	if (gap > 0 && m_position == PositionAtButton && !centered_layout())
	{
		const XfceScreenPosition screen_pos = m_plugin->get_screen_position();
		if (xfce_screen_position_is_top(screen_pos))
			m_geometry.y += gap;
		else if (xfce_screen_position_is_bottom(screen_pos))
			m_geometry.y -= gap;
		else if (xfce_screen_position_is_left(screen_pos))
			m_geometry.x += gap;
		else
			m_geometry.x -= gap;
	}

	// Prevent window from leaving screen
	m_geometry.x = CLAMP(m_geometry.x, m_monitor.x, m_monitor.x + m_monitor.width - m_geometry.width);
	m_geometry.y = CLAMP(m_geometry.y, m_monitor.y, m_monitor.y + m_monitor.height - m_geometry.height);

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
		const char* long_label, int icon_px)
{
	GtkWidget* child = gtk_bin_get_child(GTK_BIN(button));

	// The pure helper picks the label placement (FR-012/014): the long
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
		// in place rather than no-op'ing on the early child check (FR-002).
		GtkImage* image;
		if (GTK_IS_IMAGE(child))
		{
			image = GTK_IMAGE(child);
			gtk_image_set_from_icon_name(image, name, GTK_ICON_SIZE_BUTTON);
		}
		else
		{
			image = GTK_IMAGE(gtk_image_new_from_icon_name(name, GTK_ICON_SIZE_BUTTON));
			if (child)
				gtk_container_remove(GTK_CONTAINER(button), child);
			gtk_container_add(GTK_CONTAINER(button), GTK_WIDGET(image));
			gtk_widget_show(GTK_WIDGET(image));
		}
		// Size the toggle icon from its containing region (FR-001/002/003): the
		// category icon size in a sidebar, the search-bar height in the search
		// row. icon_px <= 0 means the toggle is hidden, so keep the themed size.
		if (icon_px > 0)
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

void WhiskerMenu::Window::apply_switch_presentation(const SwitchPresentation& pres)
{
	const bool icons = pres.effective_show_icons;

	// Resolve the toggle icon size from its region (FR-001/003/012/013). The
	// sidebar source is the authoritative category icon size; the search-bar
	// source has no stored value, so it is measured live from the search entry.
	const int category_px = m_settings->category_icon_size.get_size();

	// NOTE: the search-bar height is not stored anywhere; the search entry's
	// natural height is the only authoritative measure. Reduce it by a small
	// fixed inset so the icon sits inside the control rather than touching its
	// edges, and clamp to the icon-size ladder's sane range (16..128 px).
	int search_entry_h = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_search_entry), nullptr, &search_entry_h);
	const int SEARCH_BAR_ICON_INSET = 8;
	const int search_bar_px = CLAMP(search_entry_h - SEARCH_BAR_ICON_INSET, 16, 128);

	const int icon_px =
			meow_toggle_icon_px(pres.switch_location, category_px, search_bar_px);

	// Text↔icon child swap (FR-012/014): the visible text label is the short
	// "Apps"/"Places"; the long "Applications"/"Places" stays as tooltip and
	// accessible name in both modes.
	set_mode_button_content(m_mode_btn_apps, icons, MEOW_SWITCH_APPS_ICONS,
			_("Apps"), _("Applications"), icon_px);
	set_mode_button_content(m_mode_btn_places, icons, MEOW_SWITCH_PLACES_ICONS,
			_("Places"), _("Places"), icon_px);

	// Switch-box orientation (FR-007/008/014): vertical only on a vertical
	// sidebar with category names hidden; horizontal everywhere else.
	gtk_orientable_set_orientation(GTK_ORIENTABLE(m_mode_selector_box),
			pres.switch_orientation == SwitchOrientation::Vertical
					? GTK_ORIENTATION_VERTICAL
					: GTK_ORIENTATION_HORIZONTAL);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::update_layout()
{
	// Places mode (milestone 005) — mode selector and sidebar section buttons.
	const bool places_enabled = m_settings->places_enabled;

	// Feature 020: resolve the effective sidebar/switch presentation from the
	// stored intent. Forcing rules live in the pure helper; nothing here writes
	// back to settings, so removing a forcing layout restores intent (FR-029).
	SidebarLayoutState layout_state;
	layout_state.sidebar_enabled = m_settings->sidebar_enabled;
	layout_state.position = meow_parse_sidebar_position(m_settings->sidebar_position);
	layout_state.category_show_name = m_settings->category_show_name;
	layout_state.switch_show_icons = m_settings->places_switch_show_icons;
	layout_state.search_bar_bottom =
			(g_strcmp0(m_settings->search_bar_position, "bottom") == 0);
	layout_state.fullscreen =
			(g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);
	layout_state.places_enabled = places_enabled;
	const SwitchPresentation pres = meow_compute_sidebar_layout(layout_state);

	const bool switch_visible = (pres.switch_location != SwitchLocation::None);
	gtk_widget_set_visible(GTK_WIDGET(m_mode_selector_box), switch_visible);
	if (m_mode_selector_separator)
	{
		// The separator only belongs under the switch in the vertical sidebar
		// list; the horizontal strip and the relocated (search-bar) switch
		// drop it.
		gtk_widget_set_visible(m_mode_selector_separator,
				switch_visible
				&& pres.switch_location == SwitchLocation::InSidebar
				&& !pres.categories_horizontal);
	}
	if (!places_enabled)
	{
		// Hide all Places-only sidebar buttons; show the Apps-mode buttons.
		gtk_widget_set_visible(m_places_home_btn->get_widget(),    false);
		gtk_widget_set_visible(m_places_history_btn->get_widget(), false);
		gtk_widget_set_visible(m_places_fav_btn->get_widget(),     false);
		gtk_widget_set_visible(m_favorites->get_button()->get_widget(), true);
		gtk_widget_set_visible(m_recent->get_button()->get_widget(),
				m_settings->recent_items_max);
		gtk_widget_set_visible(m_applications->get_button()->get_widget(), true);
		for (GtkWidget* w : m_app_category_widgets)
		{
			gtk_widget_set_visible(w, true);
		}
	}
	else
	{
		// In places mode, only the active mode's buttons are visible. In apps
		// mode, only the apps buttons. The disabled section buttons stay hidden.
		const bool to_places = m_places_active;
		gtk_widget_set_visible(m_favorites->get_button()->get_widget(),       !to_places);
		gtk_widget_set_visible(m_recent->get_button()->get_widget(),
				!to_places && m_settings->recent_items_max);
		gtk_widget_set_visible(m_applications->get_button()->get_widget(),    !to_places);
		gtk_widget_set_visible(m_places_home_btn->get_widget(),     to_places);
		gtk_widget_set_visible(m_places_history_btn->get_widget(),
				to_places && m_settings->places_history_enabled);
		gtk_widget_set_visible(m_places_fav_btn->get_widget(),
				to_places && m_settings->places_favourites_enabled);
		for (GtkWidget* w : m_app_category_widgets)
		{
			gtk_widget_set_visible(w, !to_places);
		}
	}

	// Apply the Apps/Places switch presentation (icon vs text, orientation,
	// tooltips). Structural relocation/strip placement is handled further down
	// once the category containers have been arranged.
	apply_switch_presentation(pres);

	// Default-category heading: visible only when the sidebar is disabled
	// (FR-020/021); text follows the configured default category.
	m_applications->set_default_heading(pres.show_default_category_heading,
			m_settings->default_category);

	// Set vertical position of commands
	g_object_ref(m_commands_box);
	gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(GTK_WIDGET(m_commands_box))), GTK_WIDGET(m_commands_box));
	if (m_layout_commands_alternate)
	{
		gtk_box_pack_start(m_search_box, GTK_WIDGET(m_commands_box), false, false, 0);

		if (!m_layout_categories_horizontal)
		{
			if (m_layout_ltr == m_layout_categories_alternate)
			{
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_commands_box), 0);
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_search_entry), 1);
			}
		}
		else
		{
			if (!m_layout_ltr)
			{
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_commands_box), 0);
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_search_entry), 1);
			}
		}
	}
	else
	{
		gtk_box_pack_start(m_title_box, GTK_WIDGET(m_commands_box), false, false, 0);
	}
	g_object_unref(m_commands_box);

	// Arrange the category list and the Apps/Places switch structurally
	// (feature 020). Three category-list placements — vertical sidebar,
	// horizontal Top/Bottom strip, or hidden (sidebar disabled) — and three
	// switch homes — sidebar list, strip-leading, or the search-bar row — are
	// reconciled here. Transitions are guarded by m_sidebar_struct / m_switch_loc
	// so a steady-state pass performs no reparenting.
	const bool want_vertical = pres.sidebar_visible && !pres.categories_horizontal;
	const bool want_strip    = pres.sidebar_visible && pres.categories_horizontal;
	const int  want_struct   = want_vertical ? 1 : (want_strip ? 2 : 3);
	const bool unified_now   = unified_bar_effective(*m_settings);

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
			if (!m_strip_scroll)
			{
				// Lazily build the horizontal strip scroller: overlay
				// horizontal scrollbar, no vertical scroll, no arrow chrome
				// (FR-012). Keyboard focus auto-scroll is provided by GTK.
				m_strip_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
				gtk_scrolled_window_set_shadow_type(m_strip_scroll, GTK_SHADOW_NONE);
				gtk_scrolled_window_set_policy(m_strip_scroll,
						GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
				gtk_scrolled_window_set_overlay_scrolling(m_strip_scroll, TRUE);
				// NOTE: the spurious scrollbar was a symptom of the old centred
				// collapse, not real overflow; the fix is the width source (the
				// FILL row below), not the policy. AUTOMATIC is retained so a
				// genuinely icon-heavy strip still scrolls (many-categories edge
				// case); propagate_natural_width stays FALSE so the strip can never
				// widen the menu — it scrolls within the search-box-matched width
				// instead (FR-008/009).
				gtk_scrolled_window_set_propagate_natural_width(m_strip_scroll, FALSE);
				gtk_box_pack_start(m_categories_box, GTK_WIDGET(m_strip_scroll), true, true, 0);
			}
			if (!m_strip_lead_spacer)
			{
				// Leading expander that pushes the category icons to the trailing
				// edge inside the (stretched) viewport, so the slack falls between
				// the leading toggle and the trailing icons (FR-005). A GtkViewport
				// stretches its child to the view width and ignores child halign,
				// so the trailing pull must come from an expanding child rather than
				// an alignment.
				m_strip_lead_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
				gtk_widget_set_hexpand(m_strip_lead_spacer, TRUE);
				gtk_box_pack_start(m_category_buttons, m_strip_lead_spacer, true, true, 0);
			}
			scroller_add_child(m_strip_scroll, GTK_WIDGET(m_category_buttons));
			// Single row: toggle flush-leading, categories flush-trailing
			// (StripGeometry toggle_anchor/categories_anchor). The categories box
			// fills the available width (docked = menu width); the fullscreen pass
			// in show() overrides to CENTER so the strip tracks the search-box
			// column. The switch is pinned leading by the re-homing block below;
			// this spacer pins the icons trailing.
			gtk_box_reorder_child(m_category_buttons, m_strip_lead_spacer, 0);
			gtk_widget_show(m_strip_lead_spacer);
			gtk_widget_set_halign(GTK_WIDGET(m_categories_box), GTK_ALIGN_FILL);
			// The strip's column width and its edge alignment with the search bar
			// in full-screen are owned by the FILL + symmetric-margin geometry
			// applied in show() (docked = menu width, full-screen = workarea/2),
			// so no size group is needed to track the search box (FR-019/020).
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
			// The strip-only trailing spacer must not consume space in the
			// vertical sidebar list; hiding it removes it from allocation entirely.
			if (m_strip_lead_spacer)
				gtk_widget_hide(m_strip_lead_spacer);
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

	// Re-home the Apps/Places switch to match the computed presentation.
	{
		GtkWidget* sw = GTK_WIDGET(m_mode_selector_box);
		GtkWidget* cur = gtk_widget_get_parent(sw);
		GtkWidget* target = nullptr;
		switch (pres.switch_location)
		{
		case SwitchLocation::InSidebar:
			// Strip: the toggle anchors to the row's leading edge
			// (StripGeometry.toggle_anchor == Leading). It is pack_start'd +
			// reordered to child 0 in m_categories_box, outside the scroller
			// (FR-006/014); pack_start is direction-aware, so it follows
			// m_layout_ltr (leading = left in LTR, right in RTL) for free, and the
			// trailing category icons stay pinned by m_strip_lead_spacer. When the
			// toggle is absent (None) this leading anchor is simply left empty
			// (FR-007). Vertical sidebar: first child of the button list.
			target = want_strip ? GTK_WIDGET(m_categories_box)
			                     : GTK_WIDGET(m_category_buttons);
			break;
		case SwitchLocation::InSearchBar:
			// Sidebar disabled: the switch joins the search entry. In unified-bar
			// mode that means the centring cluster (so [entry][switch] stays
			// centred as one unit); otherwise the plain search row, where it is
			// placed just before the command buttons (see below) so the commands
			// keep their trailing position and the entry yields the room (FR-019).
			target = unified_now ? m_search_cluster : GTK_WIDGET(m_search_box);
			break;
		case SwitchLocation::None:
		default:
			break;
		}

		if (target && cur != target)
		{
			g_object_ref(sw);
			if (cur)
				gtk_container_remove(GTK_CONTAINER(cur), sw);
			if (pres.switch_location == SwitchLocation::InSearchBar
					&& target == m_search_cluster)
			{
				// Unified bar: the switch trails the search entry inside the
				// centring cluster, so [entry][switch] stay centred as one unit and
				// the entry shrinks to leave room rather than the row growing
				// (FR-019). The entry is reordered to child 0 in the unified-bar
				// transition below, so reorder the switch to last here.
				gtk_box_pack_start(GTK_BOX(target), sw, false, false, 0);
				gtk_box_reorder_child(GTK_BOX(target), sw, -1);
			}
			else if (pres.switch_location == SwitchLocation::InSearchBar)
			{
				// Plain (non-unified) search row: the embedded switch is anchored
				// before the command buttons (the leading side), not at the very
				// trailing edge, so the commands stay rightmost and the search entry
				// shrinks to make room (FR-019). When no commands share the row the
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
				// Strip / vertical sidebar: the toggle anchors leading (child 0).
				gtk_box_pack_start(GTK_BOX(target), sw, false, false, 0);
				gtk_box_reorder_child(GTK_BOX(target), sw, 0);
			}
			g_object_unref(sw);
		}
		m_switch_loc = pres.switch_location;
	}

	// Profile and session-command visibility. profile_position and
	// commands_position are the authoritative hide signals; the legacy
	// ProfileHidden shape is migrated to profile_position == "hidden" on upgrade
	// and is no longer consulted here. The avatar + username follow the profile
	// position; the whole session command box follows the commands position.
	const bool unified         = unified_bar_effective(*m_settings);

	// Resolve the Profile/Commands pair through the shared coupling helper so the
	// rendered row reflects a coherent edge combination — the same resolution the
	// Preferences combos grey and prevent_invalid() persists (FR-014). A "hidden"
	// value is always preserved by the helper, so the visibility decisions below
	// are unchanged for hidden inputs; the edge resolution matters for the
	// reorder/packing that follows. Full-screen coupling is finalised in the
	// unified-bar packing further down.
	const LayoutMode user_session_mode =
			(g_strcmp0(m_settings->layout_mode, "fullscreen") == 0)
			? LayoutMode::FullScreen : LayoutMode::Docked;
	const UserSessionResolution user_session = normalize_user_session(
			user_session_mode, m_settings->search_bar_position,
			m_settings->profile_position, m_settings->commands_position);
	const bool profile_hidden  = (g_strcmp0(user_session.profile_position, "hidden") == 0);
	const bool commands_hidden = (g_strcmp0(user_session.commands_position, "hidden") == 0);

	gtk_widget_set_visible(m_profile->get_picture(),  !profile_hidden);
	gtk_widget_set_visible(m_profile->get_username(), !profile_hidden);
	gtk_widget_set_visible(GTK_WIDGET(m_commands_box), !commands_hidden);

	// A bottom-right commands cluster is packed into the search row (the
	// m_layout_commands_alternate branch above), not the user/session row, so it
	// does not keep the title row alive on its own — only commands that share the
	// title row count toward its visibility.
	const bool commands_in_title_row = !commands_hidden && !m_layout_commands_alternate;

	// Collapse the user/session row only when nothing remains in it (FR-003);
	// keep it in unified-bar / full-screen where it carries the shared search row
	// (FR-004). Independent of category placement (FR-001).
	gtk_widget_set_visible(GTK_WIDGET(m_title_box),
			user_session_row_visible(unified, profile_hidden, !commands_in_title_row,
					m_layout_categories_alternate));

	// When the profile cluster is hidden, no expanding username remains in the
	// title row to push the surviving commands cluster to the trailing edge, so
	// give the commands box the expand + end alignment itself (FR-002). Reset to
	// the neutral state otherwise; the unified bar handles its own right-edge
	// placement via pack_end below.
	const bool docked_solo_commands = commands_in_title_row && profile_hidden && !unified;
	gtk_widget_set_hexpand(GTK_WIDGET(m_commands_box), docked_solo_commands);
	gtk_widget_set_halign(GTK_WIDGET(m_commands_box),
			docked_solo_commands ? GTK_ALIGN_END : GTK_ALIGN_FILL);

	// Arrange horizontal order of profile picture, username, and commands
	if (m_layout_ltr && m_layout_commands_alternate)
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_START);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 0);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 1);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], i);
		}
	}
	else if (m_layout_commands_alternate)
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_END);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 1);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 0);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], 8 - i);
		}
	}
	else if (m_layout_ltr)
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_START);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 0);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 1);
		gtk_box_reorder_child(m_title_box, GTK_WIDGET(m_commands_box), 2);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], i);
		}
	}
	else
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_END);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 2);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 1);
		gtk_box_reorder_child(m_title_box, GTK_WIDGET(m_commands_box), 0);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], 8 - i);
		}
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

	// Top/Bottom strip ordering is driven by the sidebar position itself, not by
	// m_layout_categories_alternate (which encodes left/right) nor by the
	// search-bar position (FR-017/018, research R7). The pure helper resolves the
	// stacking order; vertical (left/right) layouts keep the legacy mapping for
	// the hidden strip container.
	const StripGeometry strip_geom = meow_compute_strip_geometry(
			meow_parse_sidebar_position(m_settings->sidebar_position), m_layout_ltr);
	const bool strip_below_results = m_layout_categories_horizontal
			&& strip_geom.order == StripOrder::StripBelowResults;
	const bool strip_top_row = m_layout_categories_horizontal
			? !strip_below_results
			: !m_layout_categories_alternate;

	// Rhythm-matched symmetric gap: equal space before and after the strip
	// (one gap is the inter-row spacing, the other the strip's outer margin),
	// so the strip is not flush against the menu edge (FR-021).
	const int STRIP_GAP = 6;
	gtk_widget_set_margin_top(GTK_WIDGET(m_categories_box),
			m_layout_categories_horizontal && !strip_below_results ? STRIP_GAP : 0);
	gtk_widget_set_margin_bottom(GTK_WIDGET(m_categories_box),
			m_layout_categories_horizontal && strip_below_results ? STRIP_GAP : 0);

	// Docked mode paints a transparent window shell, so any spacing between
	// regions would be a transparent band rather than the frame grey the window
	// background used to supply. Collapse the inter-region spacing to 0 in docked
	// mode and let the results-area outline (update_background_css) draw the
	// boundaries; full-screen keeps the 6 px rhythm because its single window
	// alpha fills the gaps and the centring math depends on the column gap.
	const bool docked = !layout_state.fullscreen;
	gtk_box_set_spacing(m_vbox, docked ? 0 : 6);

	gtk_grid_remove_row(m_contents_box, 1);
	gtk_grid_remove_row(m_contents_box, 0);
	if (m_layout_categories_horizontal)
	{
		gtk_grid_set_column_spacing(m_contents_box, 0);
		gtk_grid_set_row_spacing(m_contents_box, STRIP_GAP);

		gtk_style_context_add_class(context, strip_below_results ? "bottom" : "top");
	}
	else
	{
		gtk_grid_set_column_spacing(m_contents_box, docked ? 0 : 6);
		gtk_grid_set_row_spacing(m_contents_box, 0);

		gtk_style_context_add_class(context, (m_layout_ltr == m_layout_categories_alternate) ? "left" : "right");
	}

	if (m_layout_ltr != m_layout_categories_alternate)
	{
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 0, 0, 1, 1);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 1, 0, 1, 1);

		gtk_box_reorder_child(m_commands_box, m_commands_spacer, 0);
	}
	else
	{
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 0, 0, 1, 1);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 1, 0, 1, 1);

		gtk_box_reorder_child(m_commands_box, m_commands_spacer, 9);
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

	// Arrange vertical order of header, applications, and search
	if (m_layout_search_alternate && m_layout_profile_alternate)
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 2);
	}
	else if (m_layout_profile_alternate)
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 2);
	}
	else if (m_layout_search_alternate)
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 2);
	}
	else
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 2);
	}

	// Handle size group to category buttons. NOTE: schema v2 — sidebar-position
	// top|bottom forces icon-only categories regardless of category-show-name.
	const bool sidebar_horizontal = (g_strcmp0(m_settings->sidebar_position, "top") == 0
			|| g_strcmp0(m_settings->sidebar_position, "bottom") == 0);
	const bool category_show_name = m_settings->category_show_name && !sidebar_horizontal;
	if (!m_sidebar_size_group && m_layout_commands_alternate && category_show_name)
	{
		m_sidebar_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget(m_sidebar_size_group, GTK_WIDGET(m_sidebar));
		gtk_size_group_add_widget(m_sidebar_size_group, GTK_WIDGET(m_commands_box));
	}
	else if (m_sidebar_size_group && (!m_layout_commands_alternate || !category_show_name))
	{
		gtk_size_group_remove_widget(m_sidebar_size_group, GTK_WIDGET(m_sidebar));
		gtk_size_group_remove_widget(m_sidebar_size_group, GTK_WIDGET(m_commands_box));
		g_object_unref(m_sidebar_size_group);
		m_sidebar_size_group = nullptr;
	}

	// Unified-bar transitions: move m_search_entry between m_search_box (normal
	// mode) and the centre-widget slot of m_title_box (unified-bar mode).
	// gtk_box_set_center_widget positions the entry at the exact horizontal
	// centre of m_title_box regardless of the profile or session-button widths,
	// satisfying FR-002 without any per-widget size measurement (FR-004).
	const bool eff = unified_bar_effective(*m_settings);
	const bool was_unified = m_layout_unified_bar;
	GtkStyleContext* title_ctx = gtk_widget_get_style_context(GTK_WIDGET(m_title_box));
	if (eff && !was_unified)
	{
		g_object_ref(m_search_entry);
		gtk_container_remove(GTK_CONTAINER(m_search_box), GTK_WIDGET(m_search_entry));
		// The entry hexpands inside the cluster: the cluster has a fixed width
		// (set below), so the entry fills whatever the trailing switch leaves.
		gtk_widget_set_hexpand(GTK_WIDGET(m_search_entry), TRUE);
		gtk_widget_set_halign(GTK_WIDGET(m_search_entry), GTK_ALIGN_FILL);
		gtk_widget_set_margin_start(GTK_WIDGET(m_search_entry), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_search_entry), 0);
		gtk_box_pack_start(GTK_BOX(m_search_cluster), GTK_WIDGET(m_search_entry), TRUE, TRUE, 0);
		// Entry leads, switch (if any) trails: the re-homing block above may have
		// already parked the switch in the cluster, so pin the entry to child 0.
		gtk_box_reorder_child(GTK_BOX(m_search_cluster), GTK_WIDGET(m_search_entry), 0);
		gtk_widget_set_visible(m_search_cluster, TRUE);
		gtk_box_set_center_widget(m_title_box, m_search_cluster);
		gtk_widget_set_visible(GTK_WIDGET(m_search_box), FALSE);
		gtk_style_context_add_class(title_ctx, "unified-bar");
		g_object_unref(m_search_entry);
	}
	else if (!eff && was_unified)
	{
		g_object_ref(m_search_entry);
		gtk_box_set_center_widget(m_title_box, nullptr);
		// The entry now lives in the cluster (the switch, if it was there, has
		// already been re-homed to m_search_box by the block above on this pass).
		gtk_container_remove(GTK_CONTAINER(m_search_cluster), GTK_WIDGET(m_search_entry));
		gtk_widget_set_size_request(m_search_cluster, -1, -1);
		gtk_widget_set_size_request(GTK_WIDGET(m_search_entry), -1, -1);
		gtk_widget_set_hexpand(GTK_WIDGET(m_search_entry), TRUE);
		gtk_widget_set_halign(GTK_WIDGET(m_search_entry), GTK_ALIGN_FILL);
		gtk_widget_set_margin_start(GTK_WIDGET(m_search_entry), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_search_entry), 0);
		gtk_box_pack_start(m_search_box, GTK_WIDGET(m_search_entry), TRUE, TRUE, 0);
		// Restore username visibility per profile_position (the authoritative
		// hide signal) and re-expand it for the docked row.
		gtk_widget_set_visible(m_profile->get_username(),
				g_strcmp0(m_settings->profile_position, "hidden") != 0);
		gtk_widget_set_hexpand(m_profile->get_username(), TRUE);
		gtk_widget_set_visible(GTK_WIDGET(m_search_box), TRUE);
		gtk_style_context_remove_class(title_ctx, "unified-bar");
		g_object_unref(m_search_entry);
	}

	// Pin the search entry width to exactly match the applications panel (FR-004).
	// The centre-widget slot guarantees screen-centre alignment; all that remains
	// is setting the correct width so the entry lines up with the grid edges.
	// Subtract the 6 px column-spacing of m_contents_box so the search entry does
	// not overflow the app-grid boundary (sidebar_w + 6 + panels_stack + 6 + void
	// == workarea, so each side effectively costs sidebar_w + 3; we subtract the
	// full column gap once here to stay within the visual content column).
	//
	// NOTE: this width is computed purely from the workarea/sidebar geometry and
	// is independent of whether the profile or commands clusters are visible, so
	// hiding both (FR-013) leaves the centred search bar — and therefore the
	// results grid and the left/right void widths — at exactly the same size and
	// position as when both clusters are shown (SC-005, INV-4). The merged row
	// reads profile + void + search bar + void + session via the centre widget;
	// hiding a cluster only blanks that child, it never re-flows the geometry.
	if (eff)
	{
		const int sidebar_w = (m_workarea.width > 0) ? m_workarea.width / 6 : 0;
		const int col_gap = 6; // m_contents_box column-spacing in vertical-sidebar mode
		// Fix the *cluster* width (entry + optional trailing switch) to the grid
		// width so the pair stays centred and the entry shrinks to make room for
		// the switch rather than the row growing past the Places-off width
		// (FR-019). The entry itself hexpands inside the cluster, so it owns all
		// the width when Places is off and yields the switch's share when it is on.
		gtk_widget_set_size_request(GTK_WIDGET(m_search_entry), -1, -1);
		gtk_widget_set_size_request(m_search_cluster,
				m_workarea.width - 2 * sidebar_w - col_gap, -1);
	}

	// Move commands_box to the right edge of the unified bar on every layout pass.
	// update_layout() re-adds it as pack_start at the top of this function; we
	// move it to pack_end here so it appears right-aligned (FR-002: session buttons
	// at the trailing edge). Only relevant when commands are in title_box
	// (m_layout_commands_alternate == false).
	if (eff && !m_layout_commands_alternate)
	{
		g_object_ref(GTK_WIDGET(m_commands_box));
		gtk_container_remove(GTK_CONTAINER(m_title_box), GTK_WIDGET(m_commands_box));
		gtk_box_pack_end(m_title_box, GTK_WIDGET(m_commands_box), false, false, 0);
		g_object_unref(GTK_WIDGET(m_commands_box));
	}

	// Three void bands: top (above unified bar), middle (between bar and content),
	// bottom (below content). All shown only when unified bar is effective (FR-008/FR-009).
	// The vbox position numbers below account for the 3 main widgets (title_box,
	// search_box, contents_stack) plus all 3 void widgets in the pack_start list.
	const bool title_at_bottom = m_layout_search_alternate && m_layout_profile_alternate;
	if (eff)
	{
		if (!title_at_bottom)
		{
			// Top-bar layout: [void_top, title, search(hidden), void_middle, contents, void_bottom]
			gtk_box_reorder_child(m_vbox, m_void_top,                    0);
			gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box),       1);
			gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box),      2);
			gtk_box_reorder_child(m_vbox, m_void_middle,                  3);
			gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack),  4);
			gtk_box_reorder_child(m_vbox, m_void_bottom,                  5);
		}
		else
		{
			// Bottom-bar layout: [void_top, contents, void_middle, search(hidden), title, void_bottom]
			gtk_box_reorder_child(m_vbox, m_void_top,                    0);
			gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack),  1);
			gtk_box_reorder_child(m_vbox, m_void_middle,                  2);
			gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box),      3);
			gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box),       4);
			gtk_box_reorder_child(m_vbox, m_void_bottom,                  5);
		}
	}
	gtk_widget_set_visible(m_void_top,    eff);
	gtk_widget_set_visible(m_void_middle, eff);
	gtk_widget_set_visible(m_void_bottom, eff);

	// FR-015 hook: warn once per layout pass if the merged row is too narrow.
	if (eff)
	{
		static int warn_count = 0;
		const int min_w = gtk_entry_get_width_chars(m_search_entry) * 8;
		if (gtk_widget_get_allocated_width(GTK_WIDGET(m_search_entry)) > 0
				&& gtk_widget_get_allocated_width(GTK_WIDGET(m_search_entry)) < min_w
				&& (warn_count++ & 0x3f) == 0)
		{
			g_debug("unified-bar: search entry below min content width; visual may clip");
		}
	}

	m_layout_unified_bar = eff;
}

//-----------------------------------------------------------------------------
