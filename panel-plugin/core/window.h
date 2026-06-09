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

#ifndef WHISKERMENU_WINDOW_H
#define WHISKERMENU_WINDOW_H

#include <vector>

#include <gtk/gtk.h>

#include "sidebar-layout.h"
#include "window-keyboard.h"

namespace WhiskerMenu
{

class ApplicationsPage;
class CategoryButton;
class FavoritesPage;
class Page;
class PlacesPage;
class Plugin;
class Profile;
class Resizer;
class RecentPage;
class SearchPage;
class Settings;

class Window
{
public:
	Window(Settings* settings, Plugin* plugin);
	~Window();

	Window(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(const Window&) = delete;
	Window& operator=(Window&&) = delete;

	enum Position
	{
		PositionAtButton,
		PositionAtCursor,
		PositionAtCenter
	};

	GtkWidget* get_widget() const
	{
		return GTK_WIDGET(m_window);
	}

	GtkEntry* get_search_entry() const
	{
		return m_search_entry;
	}

	ApplicationsPage* get_applications() const
	{
		return m_applications;
	}

	FavoritesPage* get_favorites() const
	{
		return m_favorites;
	}

	RecentPage* get_recent() const
	{
		return m_recent;
	}

	Page* get_active_page();

	PlacesPage* get_places() const { return m_places; }
	bool is_places_active() const { return m_places_active; }
	void switch_mode(bool to_places);

	void hide(bool lost_focus = false);
	void show(const Position position);
	void resize(int delta_x, int delta_y, int delta_width, int delta_height);
	void resize_start();
	void resize_end();
	void set_child_has_focus();
	void set_categories(const std::vector<CategoryButton*>& categories);
	void set_items();
	void set_loaded();
	void unset_items();

private:
	GtkWidget* get_active_category_button();
	gboolean on_key_press_event(GtkWidget* widget, GdkEventKey* key_event);
	gboolean on_key_press_event_after(GtkWidget* widget, GdkEventKey* key_event);

	/* current_visibility_mask:
	 *
	 * Folds the live layout flags and per-zone "hidden" positions into a
	 * Keyboard::VisibilityMask. Search and Results are forced visible
	 * (FR-030); the Sidebar, Mode selector, and Profile bar follow the
	 * preset's per-zone position string and visibility flags.
	 */
	Keyboard::VisibilityMask current_visibility_mask() const;

	/* current_menu_state:
	 *
	 * Returns Searching iff the search entry holds at least one
	 * character, Browsing otherwise. Used by the focus router to skip
	 * the inert sidebar while typing (FR-046).
	 */
	Keyboard::MenuState current_menu_state() const;

	/* grab_focus_in_zone:
	 * @zone: target zone for Tab/Shift+Tab.
	 *
	 * Maps a logical Zone to the concrete widget that should receive
	 * focus on entry per data-model §"Entry widget mapping" and calls
	 * gtk_widget_grab_focus on it. If the natural entry widget is not
	 * realized/visible the call is a no-op and the previously focused
	 * widget keeps focus.
	 */
	void grab_focus_in_zone(Keyboard::Zone zone);

	/* current_zone:
	 *
	 * Identifies which logical Zone currently holds the focus, by
	 * walking up the focused widget's ancestor chain. Falls back to
	 * Zone::Search when nothing matches (e.g. focus is on the menu
	 * window itself just after open).
	 */
	Keyboard::Zone current_zone() const;
	gboolean on_map_event();
	void on_state_flags_changed(GtkWidget* widget);
	void on_screen_changed(GtkWidget* widget);
	gboolean on_draw_event(GtkWidget* widget, cairo_t* cr);

	/* apply_window_shape:
	 * @width:      current window allocation width in pixels.
	 * @height:     current window allocation height in pixels.
	 * @radius:     clamped corner radius in pixels (0 = square).
	 * @composited: true when an RGBA visual / compositor is available.
	 *
	 * Masks the toplevel GdkWindow to the rounded silhouette so the corner
	 * rounding clips EVERY child, including native-windowed regions (the apps
	 * scrolledwindow/treeview) that composite straight onto the toplevel surface
	 * and therefore ignore the cairo rounded clip in on_draw_event — they would
	 * otherwise leave an opaque square corner outside the rounded outline. The
	 * mask is reset (square, full rectangle) when radius is 0 or the desktop is
	 * not composited. The (width, height, radius, composited) signature is cached
	 * so a re-entrant draw only touches the shape when the silhouette changes.
	 */
	void apply_window_shape(int width, int height, int radius, bool composited);

	/* fill_resizer_ring:
	 * @cr:     the toplevel draw context.
	 * @width:  window allocation width in pixels.
	 * @height: window allocation height in pixels.
	 *
	 * Paints the strip the 3x3 resizer grid reserves around the content vbox
	 * (the window rectangle minus the vbox rectangle) with the chrome background.
	 * The docked window shell is transparent, so this prevents that strip from
	 * showing the desktop as a band between the border and the content. The vbox
	 * area is deliberately left unpainted so the apps region keeps its own alpha.
	 */
	void fill_resizer_ring(cairo_t* cr, double width, double height);
	void update_background_css();
	void check_scrollbar_needed();
	void favorites_toggled();
	void recent_toggled();
	void category_toggled();
	void center_window();
	void move_window();
	// True when /layout-mode resolves to Centered. Drives the centred
	// placement, panel-gap suppression and continuous re-centre-on-resize
	// paths; classified defensively so an unknown value behaves as Docked.
	bool centered_layout() const;
	bool set_size(int width, int height);
	void reset_default_button();
	void show_favorites();
	void show_default_page();
	void search();
	void update_layout();

	/* set_mode_button_content:
	 * @button: one of the Apps/Places mode toggles.
	 * @show_icons: TRUE for a themed icon child, FALSE for the text label.
	 * @icon_chain: NULL-terminated icon fallback chain used when @show_icons.
	 * @short_label: gettext-translated short name ("Apps"/"Places") used to
	 *         build the visible text-mode label.
	 * @long_label: gettext-translated descriptive name ("Applications"/"Places")
	 *         used for the tooltip + accessible name in both modes, so the full
	 *         meaning survives even in icon-only mode.
	 * @icon_px: pixel size for the icon child, derived from the toggle's region
	 *         (category icon size in a sidebar, search-bar height otherwise);
	 *         <= 0 leaves the themed default and is used when the toggle is
	 *         hidden.
	 *
	 * Swaps the toggle's child between a GtkLabel and a GtkImage in place,
	 * leaving the toggle's active state and styling untouched. When the child is
	 * already an image it is reused and only its icon name and pixel size are
	 * refreshed, so a live category-icon-size change resizes the toggle too.
	 */
	void set_mode_button_content(GtkToggleButton* button, bool show_icons,
			const char* const* icon_chain, const char* short_label,
			const char* long_label, int icon_px);

	/* apply_switch_presentation:
	 * @pres: the computed presentation for this layout pass.
	 *
	 * Drives the Apps/Places switch from @pres: text↔icon child swap,
	 * box orientation, and relocation between the category list and the
	 * search-bar row (using per-pass g_object_ref guards). Reflects computed
	 * state only — the stored switch/sidebar intent is never written.
	 */
	void apply_switch_presentation(const SwitchPresentation& pres);

private:
	Settings* const m_settings;
	Plugin* m_plugin;

	GtkWindow* m_window;
	GtkFrame* m_frame;

	GtkStack* m_window_stack;
	GtkSpinner* m_window_load_spinner;

	GtkBox* m_vbox;
	GtkBox* m_title_box;
	GtkBox* m_commands_box;
	GtkBox* m_search_box;
	// Full-screen unified-bar only: holds the search entry and, when Places is
	// on, the trailing Apps/Places switch, so the pair is centred as one unit.
	// Owns a ref because it is unparented in every non-unified layout.
	GtkWidget* m_search_cluster;
	GtkStack* m_contents_stack;
	GtkGrid* m_contents_box;
	GtkBox* m_categories_box;
	GtkStack* m_panels_stack;
	GtkCssProvider* m_css_provider;

	Resizer* m_resize[8];
	Position m_position;
	GdkRectangle m_monitor;
	GdkRectangle m_workarea;

	Profile* m_profile;

	GtkWidget* m_commands_spacer;
	GtkWidget* m_commands_button[9];
	gulong m_command_slots[9];

	GtkEntry* m_search_entry;

	// Three void bands for FullScreen unified-bar mode (FR-008, FR-017, FR-018).
	GtkWidget* m_void_top;
	GtkWidget* m_void_middle;
	GtkWidget* m_void_bottom;

	SearchPage* m_search_results;
	FavoritesPage* m_favorites;
	RecentPage* m_recent;
	ApplicationsPage* m_applications;

	// Places mode (milestone 005)
	PlacesPage* m_places;
	GtkBox* m_mode_selector_box;
	GtkToggleButton* m_mode_btn_apps;
	GtkToggleButton* m_mode_btn_places;
	CategoryButton* m_places_home_btn;
	CategoryButton* m_places_history_btn;
	CategoryButton* m_places_fav_btn;
	bool m_places_active;
	bool m_mode_switch_in_progress;
	gulong m_places_property_slot;
	GtkWidget* m_mode_selector_separator;
	std::vector<GtkWidget*> m_app_category_widgets;

	GtkScrolledWindow* m_sidebar;
	// Horizontally-scrolling container for the Top/Bottom category strip
	// (FR-012). Created lazily on the first strip layout; the switch is pinned
	// outside it (FR-014). nullptr until the sidebar is first shown on top/bottom.
	GtkScrolledWindow* m_strip_scroll;
	// Expanding spacer pinned as the leading child of m_category_buttons in
	// strip mode so the category icons sit flush-trailing while the slack falls
	// between them and the leading toggle (FR-005). Hidden (and thus ignored in
	// allocation) in the vertical sidebar. Created lazily with m_strip_scroll.
	GtkWidget* m_strip_lead_spacer;
	// Current structural placement of the category list, so update_layout()
	// only reparents on an actual transition: 1 = vertical sidebar,
	// 2 = horizontal strip, 3 = hidden (sidebar disabled).
	int m_sidebar_struct;
	// Where the Apps/Places switch currently lives, to avoid redundant
	// reparenting across passes.
	SwitchLocation m_switch_loc;
	GtkBox* m_category_buttons;
	CategoryButton* m_default_button;
	// NOTE: ignore_hidden=FALSE keeps the sidebar at the widest button's
	// width even while some buttons are hidden during an Apps↔Places switch.
	GtkSizeGroup* m_category_width_group;
	GtkSizeGroup* m_sidebar_size_group;
	// Forces the two Apps/Places mode buttons to equal width in every layout
	// and preset, surviving the icon↔text child swap (FR-013).
	GtkSizeGroup* m_mode_button_size_group;

	GdkRectangle m_geometry;
	bool m_layout_ltr;
	bool m_layout_categories_horizontal;
	// Tracked stored intent so show() can fire update_layout() when these
	// switch/sidebar settings change (none are legacy layout booleans).
	bool m_layout_sidebar_enabled;
	bool m_layout_switch_show_icons;
	bool m_layout_category_show_name;
	// Tracked category icon size so show() re-runs update_layout() when it
	// changes, keeping the Apps/Places toggle in sync with the category icons.
	int m_layout_category_icon_size;
	bool m_layout_categories_alternate;
	bool m_layout_search_alternate;
	bool m_layout_commands_alternate;
	bool m_layout_profile_alternate;
	// Cached hidden state of the profile/commands clusters. Tracked separately
	// from the *_alternate edge flags because a hidden ↔ visible transition can
	// leave both edge flags unchanged; without these, show() would skip
	// update_layout() and the restored element would never re-render.
	bool m_layout_profile_hidden;
	bool m_layout_commands_hidden;
	bool m_layout_unified_bar;
	int m_profile_shape;
	bool m_supports_alpha;
	// Theme-derived separator colour (luminance-nudged from the menu background),
	// computed once in update_background_css() and reused by on_draw_event so the
	// single window border and the CSS region styling cannot diverge in colour.
	GdkRGBA m_separator_rgba = { 0.0, 0.0, 0.0, 1.0 };
	// Chrome/frame background (theme bg at the categories-region alpha), computed
	// in update_background_css() and reused by on_draw_event to fill the resizer
	// ring around the content. The docked window shell is transparent, so without
	// this the 6 px the 3x3 resizer grid reserves around the content would show
	// the desktop as a band between the border and the content.
	GdkRGBA m_chrome_rgba = { 0.0, 0.0, 0.0, 1.0 };
	// Cached signature of the last shape mask applied to the toplevel window, so
	// apply_window_shape() re-masks only when the rounded silhouette changes.
	// Initialised to -1 so the first draw always applies a shape.
	int m_shape_width = -1;
	int m_shape_height = -1;
	int m_shape_radius = -1;
	bool m_shape_composited = false;
	bool m_child_has_focus;
	bool m_resizing;
};

}

#endif // WHISKERMENU_WINDOW_H
