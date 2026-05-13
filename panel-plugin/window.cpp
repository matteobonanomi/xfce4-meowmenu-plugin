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

#include "applications-page.h"
#include "category-button.h"
#include "command.h"
#include "favorites-page.h"
#include "launcher-view.h"
#include "plugin.h"
#include "profile.h"
#include "recent-page.h"
#include "resizer.h"
#include "search-page.h"
#include "settings.h"
#include "slot.h"
#include "unified-bar.h"

#include <libxfce4ui/libxfce4ui.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

#include <ctime>

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
	m_sidebar_size_group(nullptr),
	m_geometry{0,0,1,1},
	m_layout_ltr(true),
	m_layout_categories_horizontal(false),
	m_layout_categories_alternate(false),
	m_layout_search_alternate(false),
	m_layout_commands_alternate(false),
	m_layout_profile_alternate(false),
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

	// Create the border of the window
	m_frame = GTK_FRAME(gtk_frame_new(nullptr));
	GtkShadowType initial_shadow = (m_settings->corner_radius > 0) ? GTK_SHADOW_NONE : GTK_SHADOW_OUT;
	gtk_frame_set_shadow_type(m_frame, initial_shadow);
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

	// Create action buttons
	for (int i = 0; i < 9; ++i)
	{
		m_commands_button[i] = m_settings->command[i]->get_button();
		m_command_slots[i] = connect(m_commands_button[i], "clicked",
			[this](GtkButton*)
			{
				hide();
			});
	}

	// Create search entry
	m_search_entry = GTK_ENTRY(gtk_search_entry_new());
	gtk_window_set_focus(m_window, GTK_WIDGET(m_search_entry));

	connect(m_search_entry, "changed",
		[this](GtkEditable*)
		{
			search();
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
	gtk_stack_add_named(m_contents_stack, GTK_WIDGET(search_results), "search");
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
	gtk_stack_add_named(m_panels_stack, m_applications->get_widget(), "applications");

	// Create box for packing sidebar
	m_category_buttons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(m_category_buttons, favorites_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, recent_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, applications_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), false, false, 4);

	m_sidebar = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 1, 1, 1, 1);
	gtk_scrolled_window_set_propagate_natural_height(m_sidebar, true);
	gtk_scrolled_window_set_shadow_type(m_sidebar, GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));

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

	// Re-evaluate RGBA visual and redraw when corner-radius changes so that
	// the rounded-rect clip path is activated/deactivated without reopening the menu.
	if (m_settings->channel)
	{
		g_signal_connect(m_settings->channel, "property-changed",
			G_CALLBACK(+[](XfconfChannel*, const gchar* property, const GValue*, gpointer user_data) -> void
			{
				if (g_strcmp0(property, "/corner-radius") != 0
						&& g_strcmp0(property, "/categories-opacity") != 0
						&& g_strcmp0(property, "/apps-opacity") != 0)
					return;
				auto* self = static_cast<Window*>(user_data);
				if (g_strcmp0(property, "/corner-radius") == 0 && self->m_frame)
				{
					const int r = CLAMP(static_cast<int>(self->m_settings->corner_radius), 0, 24);
					gtk_frame_set_shadow_type(self->m_frame,
						r > 0 ? GTK_SHADOW_NONE : GTK_SHADOW_OUT);
				}

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

	delete m_applications;
	delete m_search_results;
	delete m_recent;
	delete m_favorites;

	delete m_profile;

	for (Resizer* resizer : m_resize)
	{
		delete resizer;
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

Page* WhiskerMenu::Window::get_active_page()
{
	Page* page = nullptr;
	if (g_strcmp0(gtk_stack_get_visible_child_name(m_contents_stack), "search") == 0)
	{
		page = m_search_results;
	}
	else if (m_favorites->get_button()->get_active())
	{
		page = m_favorites;
	}
	else if (m_recent->get_button()->get_active())
	{
		page = m_recent;
	}
	else
	{
		page = m_applications;
	}
	return page;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::hide(bool lost_focus)
{
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

	// Handle showing tooltips
	if (m_settings->launcher_show_tooltip)
	{
		m_search_results->get_view()->show_tooltips();
		m_favorites->get_view()->show_tooltips();
		m_recent->get_view()->show_tooltips();
		m_applications->get_view()->show_tooltips();
	}
	else
	{
		m_search_results->get_view()->hide_tooltips();
		m_favorites->get_view()->hide_tooltips();
		m_recent->get_view()->hide_tooltips();
		m_applications->get_view()->hide_tooltips();
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

	// Make sure icon sizes are correct
	m_favorites->get_button()->reload_icon_size();
	m_recent->get_button()->reload_icon_size();
	m_applications->get_button()->reload_icon_size();

	m_applications->reload_category_icon_size();

	m_search_results->get_view()->reload_icon_size();
	m_favorites->get_view()->reload_icon_size();
	m_recent->get_view()->reload_icon_size();
	m_applications->get_view()->reload_icon_size();

	GdkMonitor* monitor_gdk = nullptr;
	if (position == PositionAtButton)
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

	// NOTE: schema v2 — horizontal-categories is derived from sidebar-position
	// (top|bottom); the legacy /position-categories-horizontal key is migrated
	// away in Settings::migrate_schema().
	const bool cats_horizontal = (g_strcmp0(sidebar_pos, "top") == 0)
			|| (g_strcmp0(sidebar_pos, "bottom") == 0);
	const bool unified_eff = unified_bar_effective(*m_settings);
	if ((m_layout_ltr != layout_ltr)
			|| (m_layout_categories_horizontal != cats_horizontal)
			|| (m_layout_categories_alternate != cats_alt)
			|| (m_layout_search_alternate != search_alt)
			|| (m_layout_commands_alternate != commands_alt)
			|| (m_layout_profile_alternate != profile_alt)
			|| (m_layout_unified_bar != unified_eff)
			|| (m_profile_shape != m_settings->profile_shape))
	{
		m_layout_ltr = layout_ltr;
		m_layout_categories_horizontal = cats_horizontal;
		m_layout_categories_alternate = cats_alt;
		m_layout_search_alternate = search_alt;
		m_layout_commands_alternate = commands_alt;
		m_layout_profile_alternate = profile_alt;
		m_profile->update_picture();
		m_profile_shape = m_settings->profile_shape;
		update_layout();
	}

	// T044/T045: handle sidebar hidden and fullscreen mode
	gtk_widget_set_visible(GTK_WIDGET(m_sidebar),
			g_strcmp0(sidebar_pos, "hidden") != 0);

	// T045: FullScreen mode + size-sensitive layout tweaks
	const bool is_fullscreen = (g_strcmp0(m_settings->layout_mode, "fullscreen") == 0);

	// Apply mode-dependent child size requests *before* resizing the toplevel.
	// This prevents stale fullscreen requests from forcing docked presets wider
	// than their configured menu-width when switching back from FullScreen.
	if (is_fullscreen)
	{
		// Center search bar at 50% of screen width (issue #3)
		gtk_widget_set_halign(GTK_WIDGET(m_search_box), GTK_ALIGN_CENTER);
		gtk_widget_set_size_request(GTK_WIDGET(m_search_box), m_workarea.width / 2, -1);
		// Keep sidebar width meaningful, and compensate on the opposite side so
		// the applications grid stays centered.
		const int sidebar_width = m_workarea.width / 6;
		const bool sidebar_on_left = (m_layout_ltr == m_layout_categories_alternate);
		gtk_widget_set_size_request(GTK_WIDGET(m_sidebar), sidebar_width, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack), sidebar_on_left ? 0 : sidebar_width);
		gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack), sidebar_on_left ? sidebar_width : 0);
	}
	else
	{
		gtk_widget_set_halign(GTK_WIDGET(m_search_box), GTK_ALIGN_FILL);
		gtk_widget_set_size_request(GTK_WIDGET(m_search_box), -1, -1);
		gtk_widget_set_size_request(GTK_WIDGET(m_sidebar), -1, -1);
		gtk_widget_set_margin_start(GTK_WIDGET(m_panels_stack), 0);
		gtk_widget_set_margin_end(GTK_WIDGET(m_panels_stack), 0);
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

	// Fetch position again to make sure window does not overlap panel
	if (position == PositionAtButton)
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

void WhiskerMenu::Window::resize(int delta_x, int delta_y, int delta_width, int delta_height)
{
	if (set_size(m_geometry.width + delta_width, m_geometry.height + delta_height))
	{
		check_scrollbar_needed();
	}

	if (delta_x || delta_y)
	{
		m_geometry.x += delta_x;
		m_geometry.y += delta_y;
		move_window();
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::resize_start()
{
	m_resizing = true;
	set_child_has_focus();
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::resize_end()
{
	// Store new size (never persist fullscreen dimensions as the normal menu size)
	if (g_strcmp0(m_settings->layout_mode, "fullscreen") != 0)
	{
		m_settings->menu_width = m_geometry.width;
		m_settings->menu_height = m_geometry.height;
	}

	// Move window back to panel button or center of screen
	if (m_position == PositionAtButton)
	{
		m_plugin->get_menu_position(&m_geometry.x, &m_geometry.y);
	}
	else if (m_position == PositionAtCenter)
	{
		center_window();
	}
	move_window();

	// Allow menu to hide
	m_resizing = false;
	m_child_has_focus = false;
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
	for (auto button : categories)
	{
		button->join_group(last_button);
		last_button = button;
		gtk_box_pack_start(m_category_buttons, button->get_widget(), false, false, 0);
		connect(button->get_widget(), "toggled",
			[this](GtkToggleButton*)
			{
				category_toggled();
			});
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

GtkWidget* WhiskerMenu::Window::get_active_category_button()
{
	GtkWidget* widget = m_default_button->get_widget();

	GList* children = gtk_container_get_children(GTK_CONTAINER(m_category_buttons));
	for (GList* li = children; li; li = li->next)
	{
		GtkToggleButton* button = GTK_TOGGLE_BUTTON(li->data);
		if (button && gtk_toggle_button_get_active(button))
		{
			widget = GTK_WIDGET(button);
			break;
		}
	}
	g_list_free(children);

	return widget;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_key_press_event(GtkWidget* widget, GdkEventKey* key_event)
{
	if (key_event->keyval == GDK_KEY_Escape)
	{
		// Cancel resize
		if (m_resizing)
		{
			for (Resizer* resizer : m_resize)
			{
				resizer->cancel();
			}
			set_size(m_settings->menu_width, m_settings->menu_height);
			resize_end();
		}
		// Hide if escape is pressed and there is no text in search entry
		else if (xfce_str_is_empty(gtk_entry_get_text(m_search_entry)))
		{
			hide();
		}
		// Clear search entry of text if escape is pressed
		else
		{
			gtk_entry_set_text(m_search_entry, "");
		}
		return GDK_EVENT_STOP;
	}

	Page* page = get_active_page();
	GtkWidget* view = page->get_view()->get_widget();
	GtkWidget* search = GTK_WIDGET(m_search_entry);

	switch (key_event->keyval)
	{
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left:
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right:
		// Allow keyboard navigation out of treeview
		if (GTK_IS_TREE_VIEW(view) && ((widget == view) || (gtk_window_get_focus(m_window) == view)))
		{
			gtk_widget_grab_focus(get_active_category_button());
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
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
	{
		// Determine if there is a selected item
		bool reset = page != m_search_results;
		if (reset)
		{
			GtkTreePath* path = page->get_view()->get_selected_path();
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
		// Only select first item if there is no selected item
		if ((gtk_window_get_focus(m_window) == view) && reset)
		{
			page->select_first();
			return GDK_EVENT_STOP;
		}
		break;
	}

	// Pass PageUp and PageDown keys to current view
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
		if ((widget == search) || (gtk_window_get_focus(m_window) == search))
		{
			gtk_widget_grab_focus(view);
		}
		break;

	default:
		break;
	}

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_key_press_event_after(GtkWidget* widget, GdkEventKey* key_event)
{
	// Pass unhandled key presses to search entry
	GtkWidget* search_entry = GTK_WIDGET(m_search_entry);
	if ((widget != search_entry) && (gtk_window_get_focus(m_window) != search_entry))
	{
		if (key_event->is_modifier)
		{
			return GDK_EVENT_PROPAGATE;
		}
		gtk_entry_grab_focus_without_selecting(m_search_entry);
		gtk_window_propagate_key_event(m_window, key_event);
		return GDK_EVENT_STOP;
	}
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
	GdkRGBA bg = { 0.12, 0.12, 0.12, 1.0 };
	if (!gtk_style_context_lookup_color(context, "theme_bg_color", &bg))
	{
		gtk_style_context_lookup_color(context, "bg_color", &bg);
	}

	const int red   = CLAMP(static_cast<int>(bg.red   * 255.0 + 0.5), 0, 255);
	const int green = CLAMP(static_cast<int>(bg.green * 255.0 + 0.5), 0, 255);
	const int blue  = CLAMP(static_cast<int>(bg.blue  * 255.0 + 0.5), 0, 255);
	const double cats_alpha = CLAMP(m_settings->categories_opacity, 0, 100) / 100.0;
	const double apps_alpha = CLAMP(m_settings->apps_opacity, 0, 100) / 100.0;

	gchar* css = g_strdup_printf(
		".meowmenu { background-image: none; background-color: rgba(%d, %d, %d, %.3f); }"
		".meowmenu > *,"
		".meowmenu frame,"
		".meowmenu frame > *,"
		".meowmenu stack,"
		".meowmenu stack > *,"
		".meowmenu scrolledwindow,"
		".meowmenu scrolledwindow > *,"
		".meowmenu grid,"
		".meowmenu grid > *,"
		".meowmenu .search-area,"
		".meowmenu .title-area,"
		".meowmenu .commands-area,"
		".meowmenu .contents,"
		".meowmenu .contents > *,"
		".meowmenu .categories,"
		".meowmenu treeview,"
		".meowmenu flowbox,"
		".meowmenu flowboxchild,"
		".meowmenu list,"
		".meowmenu row"
		"{ background-color: transparent; background-image: none; }"
		".meowmenu .applications-area,"
		".meowmenu .applications-area > *"
		"{ background-image: none; background-color: rgba(%d, %d, %d, %.3f); }"
		".meowmenu .category-button,"
		".meowmenu .category-button *,"
		".meowmenu .category-button image,"
		".meowmenu .category-button label"
		"{ opacity: 1; }",
		red, green, blue, cats_alpha,
		red, green, blue, apps_alpha);

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

	// Build rounded-rect clip path (T040: corner-radius)
	const double r = CLAMP(m_settings->corner_radius, 0, 24);
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

	if (enabled && m_supports_alpha)
	{
		// Erase the previous frame so pixels outside the rounded clip are transparent.
		cairo_save(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
		cairo_paint(cr);
		cairo_restore(cr);

		cairo_surface_t* background = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
		cairo_t* cr_background = cairo_create(background);
		cairo_set_operator(cr_background, CAIRO_OPERATOR_SOURCE);
		gtk_render_background(context, cr_background, 0.0, 0.0, width, height);
		cairo_destroy(cr_background);

		cairo_set_source_surface(cr, background, 0.0, 0.0);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_save(cr);
		clip_rounded(cr);
		cairo_clip(cr);
		cairo_paint(cr);
		cairo_restore(cr);

		cairo_surface_destroy(background);
	}
	else
	{
		gtk_render_background(context, cr, 0.0, 0.0, width, height);
	}

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::check_scrollbar_needed()
{
	// Find height of sidebar and buttons
	int buttons_height = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_category_buttons), nullptr, &buttons_height);

	int sidebar_height = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_sidebar), nullptr, &sidebar_height);

	// Always show scrollbar if sidebar is shorter than buttons
	if (sidebar_height >= buttons_height)
	{
		gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
	else
	{
		gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::favorites_toggled()
{
	m_favorites->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "favorites");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::recent_toggled()
{
	m_recent->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "recent");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::category_toggled()
{
	m_applications->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "applications");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::center_window()
{
	m_geometry.x = (m_monitor.width - m_geometry.width) / 2;
	m_geometry.y = (m_monitor.height - m_geometry.height) / 2;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::move_window()
{
	// T042: apply panel-gap offset away from the panel
	const int gap = m_settings->panel_gap;
	if (gap > 0 && m_position == PositionAtButton)
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

bool WhiskerMenu::Window::set_size(int width, int height)
{
	bool resized = false;
	width = CLAMP(width, 10, m_monitor.width);
	height = CLAMP(height, 10, m_monitor.height);
	if ((m_geometry.width != width) || (m_geometry.height != height))
	{
		m_geometry.width = width;
		m_geometry.height = height;
		gtk_widget_set_size_request(GTK_WIDGET(m_window), m_geometry.width, m_geometry.height);
		gtk_window_resize(m_window, 1, 1);
		resized = true;
	}
	return resized;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::reset_default_button()
{
	switch (m_settings->default_category)
	{
	case Settings::CategoryRecent:
		m_default_button = m_recent->get_button();
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(), 0);
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(), 1);
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(), 2);
		break;

	case Settings::CategoryAll:
		m_default_button = m_applications->get_button();
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(), 0);
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(), 1);
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(), 2);
		break;

	default:
		m_default_button = m_favorites->get_button();
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(), 0);
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(), 1);
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(), 2);
		break;
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_favorites()
{
	// Switch to favorites panel
	m_favorites->get_button()->set_active(true);

	// Clear search entry
	gtk_entry_set_text(m_search_entry, "");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_default_page()
{
	// Switch to favorites panel
	m_default_button->set_active(true);

	// Clear search entry
	gtk_entry_set_text(m_search_entry, "");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::search()
{
	// Fetch search string
	const gchar* text = gtk_entry_get_text(m_search_entry);
	if (xfce_str_is_empty(text))
	{
		text = nullptr;
	}

	if (text)
	{
		// Show search results
		gtk_stack_set_visible_child_name(m_contents_stack, "search");
	}
	else
	{
		// Show active panel
		gtk_stack_set_visible_child_name(m_contents_stack, "contents");
	}

	// Apply filter
	m_search_results->set_filter(text);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::update_layout()
{
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

	// Set horizontal position of categories
	g_object_ref(m_category_buttons);
	if (m_layout_categories_horizontal)
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(m_category_buttons)) == GTK_ORIENTATION_VERTICAL)
		{
			gtk_orientable_set_orientation(GTK_ORIENTABLE(m_category_buttons), GTK_ORIENTATION_HORIZONTAL);
			gtk_container_remove(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));
			gtk_widget_set_visible(GTK_WIDGET(m_sidebar), false);
			gtk_widget_set_visible(GTK_WIDGET(m_categories_box), true);
			gtk_box_set_center_widget(m_categories_box, GTK_WIDGET(m_category_buttons));
		}
	}
	else
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(m_category_buttons)) == GTK_ORIENTATION_HORIZONTAL)
		{
			gtk_orientable_set_orientation(GTK_ORIENTABLE(m_category_buttons), GTK_ORIENTATION_VERTICAL);
			gtk_container_remove(GTK_CONTAINER(m_categories_box), GTK_WIDGET(m_category_buttons));
			gtk_widget_set_visible(GTK_WIDGET(m_categories_box), false);
			gtk_widget_set_visible(GTK_WIDGET(m_sidebar), true);
			gtk_container_add(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));
		}
	}
	g_object_unref(m_category_buttons);

	// Handle showing username and profile
	if (m_profile_shape != Settings::ProfileHidden)
	{
		gtk_widget_set_visible(m_profile->get_picture(), true);
		gtk_widget_set_visible(m_profile->get_username(), true);
		gtk_widget_set_visible(GTK_WIDGET(m_title_box), true);
	}
	else
	{
		gtk_widget_set_visible(m_profile->get_picture(), false);
		gtk_widget_set_visible(m_profile->get_username(), false);
		gtk_widget_set_visible(GTK_WIDGET(m_title_box), !m_layout_categories_alternate);
	}

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

	gtk_grid_remove_row(m_contents_box, 1);
	gtk_grid_remove_row(m_contents_box, 0);
	if (m_layout_categories_horizontal)
	{
		gtk_grid_set_column_spacing(m_contents_box, 0);
		gtk_grid_set_row_spacing(m_contents_box, 6);

		gtk_style_context_add_class(context, m_layout_categories_alternate ? "bottom" : "top");
	}
	else
	{
		gtk_grid_set_column_spacing(m_contents_box, 6);
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

	if (!m_layout_categories_alternate)
	{
		gtk_grid_insert_row(m_contents_box, 0);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_categories_box), 0, 0, 2, 1);
	}
	else
	{
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

	// Unified-bar transitions: re-parent m_search_entry between m_search_box
	// and m_title_box. Preconditions are evaluated in unified_bar_effective();
	// neither flip cares about the order of search_alt / profile_alt above —
	// when the predicate is true those positions are guaranteed coherent.
	const bool eff = unified_bar_effective(*m_settings);
	const bool was_unified = m_layout_unified_bar;
	GtkStyleContext* title_ctx = gtk_widget_get_style_context(GTK_WIDGET(m_title_box));
	if (eff && !was_unified)
	{
		g_object_ref(m_search_entry);
		gtk_container_remove(GTK_CONTAINER(m_search_box), GTK_WIDGET(m_search_entry));
		gtk_widget_set_hexpand(GTK_WIDGET(m_search_entry), TRUE);
		gtk_widget_set_halign(GTK_WIDGET(m_search_entry), GTK_ALIGN_FILL);
		gtk_box_pack_start(m_title_box, GTK_WIDGET(m_search_entry), TRUE, TRUE, 0);
		// Order: picture, username (hidden), search_entry, commands_box.
		gtk_box_reorder_child(m_title_box, GTK_WIDGET(m_search_entry), 2);
		gtk_widget_set_visible(m_profile->get_username(), FALSE);
		gtk_widget_set_visible(GTK_WIDGET(m_search_box), FALSE);
		gtk_style_context_add_class(title_ctx, "unified-bar");
		g_object_unref(m_search_entry);
	}
	else if (!eff && was_unified)
	{
		g_object_ref(m_search_entry);
		gtk_container_remove(GTK_CONTAINER(m_title_box), GTK_WIDGET(m_search_entry));
		gtk_widget_set_hexpand(GTK_WIDGET(m_search_entry), TRUE);
		gtk_widget_set_halign(GTK_WIDGET(m_search_entry), GTK_ALIGN_FILL);
		gtk_box_pack_start(m_search_box, GTK_WIDGET(m_search_entry), TRUE, TRUE, 0);
		// Restore username visibility per the existing profile-shape rule.
		gtk_widget_set_visible(m_profile->get_username(),
				m_profile_shape != Settings::ProfileHidden);
		gtk_widget_set_visible(GTK_WIDGET(m_search_box), TRUE);
		gtk_style_context_remove_class(title_ctx, "unified-bar");
		// Reset leading margin that was applied to constrain search width (FR-004).
		gtk_widget_set_margin_start(GTK_WIDGET(m_search_entry), 0);
		g_object_unref(m_search_entry);
	}

	// Search entry leading margin: align search start with the applications panel
	// so the entry width ≈ app-box width rather than the full bar width (FR-002/FR-004).
	// Re-read the sidebar allocation on every pass so the margin stays current
	// if the sidebar width changes (e.g. category labels added/removed).
	if (eff)
	{
		const int sidebar_px = gtk_widget_get_allocated_width(GTK_WIDGET(m_sidebar));
		gtk_widget_set_margin_start(GTK_WIDGET(m_search_entry), MAX(0, sidebar_px));
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
