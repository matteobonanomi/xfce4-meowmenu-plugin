/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_CORE_MENU_COMPOSITION_H
#define MEOWMENU_CORE_MENU_COMPOSITION_H

#include "layout-mode.h"

#include <vector>

namespace WhiskerMenu
{

enum class PrimaryEdge
{
	Top,
	Bottom
};

enum class CompositionSidebar
{
	Hidden,
	Left,
	Right,
	Horizontal
};

enum class MenuDirection
{
	LeftToRight,
	RightToLeft
};

enum class MenuBand
{
	Primary,
	Results,
	HorizontalSidebar,
	Secondary
};

enum class MenuSlot
{
	Profile,
	SidebarReserve,
	AppsPlaces,
	Search,
	Session
};

enum class MenuControlLocation
{
	Hidden,
	PrimaryRow,
	SecondaryRow,
	Sidebar // Only a vertical Left/Right sidebar owns Apps/Places.
};

enum class MenuColumnRole
{
	None,
	FullWidth,
	Sidebar,
	Results,
	MiddleResults,
	Outer
};

enum class MenuAlignment
{
	None,
	Fill,
	LogicalLeading,
	LogicalTrailing
};

enum class MenuSurfaceRole
{
	None,
	Chrome,
	Content,
	FullScreen
};

/* MenuCompositionInput:
 *
 * One immutable snapshot of stored layout intent and current action
 * availability. The resolver clamps action availability to a present/absent
 * decision and does not mutate persistent settings.
 */
struct MenuCompositionInput
{
	LayoutMode layout_mode;
	PrimaryEdge primary_edge;
	CompositionSidebar sidebar;
	bool show_profile;
	bool show_session;
	unsigned int available_session_actions;
	bool places_enabled;
	MenuDirection direction;
};

/* MenuComposition:
 *
 * Complete GTK-independent instructions for one menu layout pass. Slot lists
 * are stored in physical left-to-right order; logical leading/trailing roles
 * are mirrored by the resolver for right-to-left interfaces. Explicit Left
 * and Right sidebars remain on their selected physical sides. SidebarReserve
 * is a non-interactive header column used only above a visible windowed
 * vertical sidebar when Profile is hidden.
 */
struct MenuComposition
{
	CompositionSidebar sidebar;
	PrimaryEdge primary_edge;
	PrimaryEdge horizontal_sidebar_edge;
	bool horizontal_sidebar_visible;
	bool secondary_visible;
	bool effective_profile;
	bool effective_session;

	std::vector<MenuBand> vertical_bands;
	std::vector<MenuSlot> primary_slots;
	std::vector<MenuSlot> secondary_slots;

	MenuControlLocation profile_location;
	MenuControlLocation search_location;
	MenuControlLocation apps_places_location;
	MenuControlLocation session_location;

	MenuColumnRole profile_column;
	MenuColumnRole search_column;
	MenuColumnRole apps_places_column;
	MenuColumnRole session_column;

	MenuAlignment profile_alignment;
	MenuAlignment search_alignment;
	MenuAlignment apps_places_alignment;
	MenuAlignment session_alignment;

	MenuSurfaceRole baseline_surface;
	MenuSurfaceRole primary_surface;
	MenuSurfaceRole profile_surface;
	MenuSurfaceRole search_surface;
	MenuSurfaceRole results_surface;
	MenuSurfaceRole sidebar_surface;
	MenuSurfaceRole horizontal_sidebar_surface;
	MenuSurfaceRole secondary_surface;
};

struct MenuSurfaceRectangle
{
	int x;
	int y;
	int width;
	int height;
	bool visible;
};

struct MenuChromeGeometry
{
	MenuSurfaceRectangle vertical;
	MenuSurfaceRectangle band;
	MenuSurfaceRectangle separator;
	MenuSurfaceRectangle secondary_separator;
};

struct MenuContentMargins
{
	int top;
	int bottom;
};

/* meow_resolve_menu_composition:
 * @input: supported layout intent and current Session action availability.
 *
 * Resolves every Docked, Centered, and Full Screen composition from one total
 * value function. The result owns no widgets and is safe to compare or test
 * before a live GTK relayout.
 *
 * Returns: a complete composition by value.
 */
MenuComposition meow_resolve_menu_composition(const MenuCompositionInput& input);

/* meow_resolve_chrome_geometry:
 * @composition: resolved semantic surface and band ordering.
 * @window_width: allocated launcher width.
 * @window_height: allocated launcher height.
 * @region_gap: resolved theme spacing between adjacent major regions.
 * @profile: allocated Profile block in launcher coordinates.
 * @sidebar: allocated vertical sidebar in launcher coordinates.
 * @horizontal: allocated Horizontal navigation in launcher coordinates.
 * @secondary: allocated Apps/Places and Session row in launcher coordinates.
 *
 * Extends semantic Chrome through the outer layout inset and the gap beside a
 * secondary row, so auxiliary controls have equal space toward the launcher
 * edge and toward the Content boundary. The allocated outer inset is reused
 * when available; @region_gap is its safe pre-allocation fallback. Horizontal
 * and secondary bands on the same edge are returned as one continuous
 * rectangle. A visible secondary band also returns one one-pixel separator on
 * its Content edge. Horizontal navigation with a visible secondary band
 * returns a second one-pixel separator in the existing allocation gap. The two
 * boundary positions balance the live Horizontal and secondary control centres
 * between their visible edges without changing either allocation.
 *
 * Returns: at most one vertical column, one full-width edge band, and its
 * Content-edge separator.
 */
MenuChromeGeometry meow_resolve_chrome_geometry(
		const MenuComposition& composition, int window_width, int window_height,
		int region_gap,
		const MenuSurfaceRectangle& profile,
		const MenuSurfaceRectangle& sidebar,
		const MenuSurfaceRectangle& horizontal,
		const MenuSurfaceRectangle& secondary);

/* meow_resolve_contents_frame_margins:
 * @composition: resolved vertical band order.
 * @frame_inset: transparent resize-handle thickness at each window edge.
 *
 * Adds the frame inset to the Results-side gap only when the secondary Chrome
 * band directly neighbors the Results/Horizontal content block. This makes the
 * native Results viewport end at the painted, optically centred boundary.
 *
 * Returns: non-negative top and bottom margins for the contents stack.
 */
MenuContentMargins meow_resolve_contents_frame_margins(
		const MenuComposition& composition, int frame_inset);

struct MenuLayoutSnapshotInput
{
	MenuCompositionInput composition;
	bool category_names_visible;
	bool selector_icons_requested;
	int category_icon_px;
	int search_icon_px;
	int session_icon_px;
};

struct MenuLayoutSnapshot
{
	MenuLayoutSnapshotInput input;
	MenuComposition composition;
};

/* meow_resolve_layout_snapshot:
 * @input: complete stored and ephemeral state for one presentation.
 *
 * Captures the composition and selector metric inputs in one immutable value so
 * every show and live relayout can reconcile without a second cache predicate.
 *
 * Returns: a complete snapshot by value.
 */
MenuLayoutSnapshot meow_resolve_layout_snapshot(
		const MenuLayoutSnapshotInput& input);

/* meow_layout_snapshot_equal:
 * @first: first complete snapshot.
 * @second: second complete snapshot.
 *
 * Compares every raw and resolved field used by the presentation transaction.
 *
 * Returns: true only when repeated reconciliation has identical inputs and
 * outputs.
 */
bool meow_layout_snapshot_equal(const MenuLayoutSnapshot& first,
		const MenuLayoutSnapshot& second);

}

#endif // MEOWMENU_CORE_MENU_COMPOSITION_H
