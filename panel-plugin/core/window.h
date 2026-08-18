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

#include "interactive-resize.h"
#include "menu-composition.h"
#include "menu-mode-state.h"
#include "sidebar-layout.h"
#include "theme-fallback.h"
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
	bool interactive_resize_begin(
			InteractiveResize::Direction direction,
			InteractiveResize::BackendPolicy policy,
			const InteractiveResize::PointerSample& pointer);
	bool interactive_resize_step(
			const InteractiveResize::PointerSample& pointer);
	bool interactive_resize_complete(
			const InteractiveResize::PointerSample& pointer);
	bool interactive_resize_cancel();
	void set_child_has_focus();
	/* detach_categories:
	 *
	 * Ends the current dynamic application-category borrow epoch. The window
	 * removes borrowed category widgets from its containers and forgets them,
	 * but ownership remains with ApplicationsPage/CategoryButton.
	 */
	void detach_categories();
	/* refresh_layout:
	 *
	 * Re-applies current settings to the existing window without invalidating
	 * Garcon application/category contents. If the menu is open, it also
	 * re-runs the visible sizing and placement pass.
	 */
	void refresh_layout();
	void set_categories(const std::vector<CategoryButton*>& categories);
	void set_items();
	void set_loaded();
	void unset_items();

private:
	/* sync_category_label_width:
	 *
	 * Pin every sidebar category button (Apps and Places) to the same minimum
	 * label width — the widest label across both modes, clamped to the cap — so
	 * the sidebar width is carried by the visible buttons and survives an
	 * Apps<->Places switch. Call after the built-in buttons exist and again
	 * whenever the application categories change.
	 */
	void sync_category_label_width();

	GtkWidget* get_active_category_button();
	gboolean on_key_press_event(GtkWidget* widget, GdkEventKey* key_event);
	gboolean on_key_press_event_after(GtkWidget* widget, GdkEventKey* key_event);

	/* keyboard_navigate_category:
	 * @target: the category radio button to move keyboard focus to (already
	 *          confirmed visible/sensitive/focusable; must not be NULL).
	 *
	 * Performs a keyboard-origin category focus move under the
	 * m_keyboard_category_nav guard so the `toggled` handlers keep focus in the
	 * sidebar instead of handing off to the search entry. When the
	 * hover-activation setting is on the target is also activated so results
	 * update live; when it is off only focus/highlight moves and the committed
	 * category is left untouched until Enter/Space. Hover auto-activation is
	 * suppressed until the next genuine pointer motion so a stationary pointer
	 * cannot eject keyboard focus.
	 */
	void keyboard_navigate_category(GtkWidget* target);
	bool dispatch_directional_navigation(Keyboard::PhysicalDirection direction);

	/* current_menu_state:
	 *
	 * Returns Searching iff the search entry holds at least one
	 * character, Browsing otherwise. Used by the focus router to skip
	 * the inert sidebar while typing (supported behavior).
	 */
	Keyboard::MenuState current_menu_state() const;
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

	void update_background_css();
	void schedule_style_refresh();
	void refresh_theme_metrics();
	void update_view_redraw_safeguards();
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
	void clear_resize_handles();
	InteractiveResize::DisplaySignature resize_display_signature(
			GdkMonitor* monitor) const;
	void start_resize_display_watch(GdkMonitor* monitor);
	void stop_resize_display_watch();
	void validate_resize_display();
	void apply_resize_rectangle(
			const InteractiveResize::Rectangle& rectangle);
	void settle_resize_position();
	bool set_size(int width, int height);
	void reset_default_button();
	void show_favorites();
	void show_default_page();
	void apply_menu_mode(MenuMode requested_mode,
			MenuModeTransition transition);
	MenuContentTarget current_menu_content() const;
	void search();
	void update_layout();
	void apply_menu_composition(const MenuComposition& composition);

	/* set_mode_button_content:
	 * @button: one of the Apps/Places mode toggles.
	 * @show_icons: TRUE for a themed icon child, FALSE for the text label.
	 * @icon_chain: NULL-terminated icon fallback chain used when @show_icons.
	 * @short_label: gettext-translated short name ("Apps"/"Places") used to
	 *         build the visible text-mode label.
	 * @long_label: gettext-translated descriptive name ("Applications"/"Places")
	 *         used for the tooltip + accessible name in both modes, so the full
	 *         meaning survives even in icon-only mode.
	 * @icon_size: GTK theme size role used for the image request.
	 * @icon_px: pixel size for the icon child, derived from the toggle's region
	 *         (category icon size in a sidebar, search-bar height otherwise).
	 *         A negative value preserves @icon_size exactly; this is used beside
	 *         Session buttons so both controls follow the same GTK theme metric.
	 *
	 * Swaps the toggle's child between a GtkLabel and a GtkImage in place,
	 * leaving the toggle's active state and styling untouched. When the child is
	 * already an image it is reused and only its icon name and pixel size are
	 * refreshed, so a live category-icon-size change resizes the toggle too.
	 */
	void set_mode_button_content(GtkToggleButton* button, bool show_icons,
			const char* const* icon_chain, const char* short_label,
			const char* long_label, GtkIconSize icon_size, int icon_px);

	/* apply_switch_presentation:
	 * @pres: the computed presentation for this layout pass.
	 *
	 * Drives the Apps/Places switch from @pres: text↔icon child swap,
	 * box orientation, and relocation between the category list and the
	 * search-bar row (using per-pass g_object_ref guards). Reflects computed
	 * state only — the stored switch/sidebar intent is never written.
	 */
	void apply_switch_presentation(const SelectorPresentation& presentation);
	void update_favourite_drop_targets();
	bool application_favourites_drop_available() const;
	bool places_favourites_drop_available() const;
	gboolean on_application_favourites_drag_motion(GdkDragContext* context,
			guint time);
	gboolean on_places_favourites_drag_motion(GdkDragContext* context,
			guint time);
	void on_favourite_drag_leave(GtkWidget* widget);
	void on_application_favourites_drag_data_received(GdkDragContext* context,
			GtkSelectionData* data, guint info, guint time);
	void on_places_favourites_drag_data_received(GdkDragContext* context,
			GtkSelectionData* data, guint info, guint time);

private:
	Settings* const m_settings;
	Plugin* m_plugin;

	GtkWindow* m_window;
	GtkFrame* m_frame;

	GtkStack* m_window_stack;
	GtkSpinner* m_window_load_spinner;

	GtkBox* m_vbox;
	// Stable row containers. The historical aliases remain while the Full
	// Screen path is replaced independently.
	GtkBox* m_primary_row;
	GtkBox* m_primary_middle;
	GtkBox* m_secondary_row;
	GtkBox* m_title_box;
	GtkBox* m_commands_box;
	GtkBox* m_search_box;
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


	SearchPage* m_search_results;
	FavoritesPage* m_favorites;
	RecentPage* m_recent;
	ApplicationsPage* m_applications;

	// Places mode (current behavior)
	PlacesPage* m_places;
	GtkBox* m_mode_selector_box;
	GtkToggleButton* m_mode_btn_apps;
	GtkToggleButton* m_mode_btn_places;
	CategoryButton* m_places_home_btn;
	CategoryButton* m_places_history_btn;
	CategoryButton* m_places_fav_btn;
	bool m_places_active;
	bool m_mode_switch_in_progress;
	// Transient keyboard-origin guard for sidebar category activation. Set true
	// only for the span of a keyboard-driven category focus move / activation so
	// the category `toggled` handlers can tell keyboard navigation (keep focus in
	// the sidebar) from pointer selection (hand off to the search entry). Never
	// persisted and never observable across events; cleared immediately after the
	// activation completes. Mirrors the m_mode_switch_in_progress idiom above.
	bool m_keyboard_category_nav;
	gulong m_places_property_slot;
	gulong m_live_settings_property_slot;
	GtkWidget* m_mode_selector_separator;
	std::vector<GtkWidget*> m_app_category_widgets;
	// The dynamically-loaded application-category buttons, kept alongside their
	// widgets so the shared sidebar width floor can be recomputed when they
	// arrive (see sync_category_label_width). Reset on every set_categories().
	std::vector<CategoryButton*> m_app_categories;

	GtkScrolledWindow* m_sidebar;
	// Horizontally-scrolling container for the Horizontal category strip
	// (supported behavior). Created lazily on the first strip layout; the
	// Apps/Places selector is not a child of this strip. nullptr until the
	// sidebar is first shown horizontally.
	GtkScrolledWindow* m_strip_scroll;
	// Symmetric expanding spacers around the category group in strip mode. They
	// center fitting category lists while collapsing to the available slack when
	// the list overflows. Hidden in the vertical sidebar.
	GtkWidget* m_strip_lead_spacer;
	GtkWidget* m_strip_trail_spacer;
	// Current structural placement of the category list, so update_layout()
	// only reparents on an actual transition: 1 = vertical sidebar,
	// 2 = horizontal strip, 3 = hidden (sidebar disabled).
	int m_sidebar_struct;
	GtkBox* m_category_buttons;
	CategoryButton* m_default_button;
	// NOTE: ignore_hidden=FALSE keeps the sidebar at the widest button's
	// width even while some buttons are hidden during an Apps↔Places switch.
	GtkSizeGroup* m_category_width_group;
	// Forces the two Apps/Places mode buttons to equal width in every layout
	// and preset, surviving the icon↔text child swap (supported behavior).
	GtkSizeGroup* m_mode_button_size_group;

	GdkRectangle m_geometry;
	bool m_layout_ltr;
	bool m_layout_categories_horizontal;
	CompositionSidebar m_layout_sidebar_position;
	bool m_layout_sidebar_enabled;
	unsigned int m_layout_available_session_actions;
	MenuComposition m_composition;
	MenuLayoutSnapshot m_layout_snapshot;
	ThemeSurfacePalette m_surface_palette;
	ThemeLayoutMetrics m_layout_metrics;
	guint m_style_refresh_source;
	bool m_style_refresh_running;
	int m_profile_shape;
	bool m_supports_alpha;
	// Theme-derived separator colour (luminance-nudged from the menu background),
	// computed once in update_background_css() and reused by on_draw_event so the
	// single window border and secondary-row divider cannot diverge in colour.
	GdkRGBA m_separator_rgba = { 0.0, 0.0, 0.0, 1.0 };
	// Cached signature of the last shape mask applied to the toplevel window, so
	// apply_window_shape() re-masks only when the rounded silhouette changes.
	// Initialised to -1 so the first draw always applies a shape.
	int m_shape_width = -1;
	int m_shape_height = -1;
	int m_shape_radius = -1;
	bool m_shape_composited = false;
	bool m_child_has_focus;
	bool m_resizing;
	InteractiveResize::Transaction m_resize_transaction;
	GdkMonitor* m_resize_monitor;
	gulong m_resize_monitor_notify_slot;
	gulong m_resize_monitor_removed_slot;
};

}

#endif // WHISKERMENU_WINDOW_H
