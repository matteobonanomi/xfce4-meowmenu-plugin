/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_CORE_WINDOW_KEYBOARD_H
#define MEOWMENU_CORE_WINDOW_KEYBOARD_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gdk/gdk.h>

namespace WhiskerMenu
{
namespace Keyboard
{

/* Physical direction is deliberately independent of text direction. */
enum class PhysicalDirection : unsigned
{
	Up,
	Down,
	Left,
	Right,
};

/* Regions that may participate in directional focus movement. */
enum class NavigationRegion : unsigned
{
	Search,
	Results,
	Sidebar,
	SessionControls,
};

enum class MenuState : unsigned
{
	Browsing,
	Searching,
};

/* A rectangle in the menu toplevel's integer coordinate space. */
struct NavigationRect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;

	NavigationRect() = default;
	NavigationRect(int x_value, int y_value, int width_value, int height_value) :
		x(x_value), y(y_value), width(width_value), height(height_value)
	{
	}

	bool is_valid() const;
	std::int64_t center_x2() const;
	std::int64_t center_y2() const;
};

enum class FocusTargetKind : unsigned
{
	SearchEntry,
	ResultItem,
	CalculatorBanner,
	CategoryButton,
	SessionButton,
	ModeSwitch,
	Decorative,
};

/* Event-local description of a target collected from the live hierarchy. */
struct FocusTarget
{
	std::size_t target_id = 0;
	NavigationRegion region = NavigationRegion::Search;
	FocusTargetKind kind = FocusTargetKind::Decorative;
	NavigationRect rectangle;
	unsigned visual_ordinal = 0;
	bool usable = false;
	bool selected = false;
};

/* Result of applying internal-first routing to one ordinary arrow. */
enum class NavigationDecisionKind : unsigned
{
	InternalMove,
	CrossRegionMove,
	NoOp,
	NotRouted,
};

struct NavigationDecision
{
	NavigationDecisionKind kind = NavigationDecisionKind::NoOp;
	std::size_t target_id = 0;

	NavigationDecision() = default;
	NavigationDecision(NavigationDecisionKind kind_value,
			std::size_t target_value) :
		kind(kind_value), target_id(target_value)
	{
	}
};

static constexpr std::size_t NO_TARGET = static_cast<std::size_t>(-1);

/* normalize_direction:
 * @keyval: ordinary or keypad arrow keysym.
 * @direction: output physical direction; may be NULL.
 *
 * Converts both keyboard arrow representations without applying modifier
 * policy. Returns false for every non-directional keysym.
 */
bool normalize_direction(guint keyval, PhysicalDirection* direction);

/* is_directional_key:
 * @event: key event to classify.
 *
 * Returns true only for an unmodified ordinary or keypad arrow. Modified
 * arrows remain available to text editing, toolkit navigation, or the
 * desktop shortcut layer.
 */
bool is_directional_key(const GdkEventKey* event);

/* target_is_eligible:
 * @target: event-local target description.
 * @state: current menu state.
 *
 * Applies the pure portion of the live-target eligibility contract. GTK
 * collection code supplies `usable`; this helper adds region and kind rules.
 */
bool target_is_eligible(const FocusTarget& target, MenuState state);

/* choose_spatial_target:
 * @origin: focused target rectangle.
 * @direction: physical direction being pressed.
 * @targets: candidates collected for this event.
 * @rtl: whether horizontal visual tie order is reversed.
 *
 * Scores only targets strictly ahead of the origin by forward separation,
 * perpendicular alignment, visual row order, physical leading-to-trailing
 * order, and finally the supplied stable ordinal. Returns NO_TARGET when no
 * candidate is usable in the pressed half-plane.
 */
std::size_t choose_spatial_target(const NavigationRect& origin,
		PhysicalDirection direction,
		const std::vector<FocusTarget>& targets,
		bool rtl,
		MenuState state = MenuState::Browsing);

/* decide_navigation:
 * @origin: current focus target.
 * @internal: usable candidates owned by the current region.
 * @external: usable candidates owned by other regions.
 *
 * Internal movement always wins. A missing target is an explicit consumed
 * no-op, while callers that did not classify an ordinary arrow use
 * NavigationDecisionKind::NotRouted directly.
 */
NavigationDecision decide_navigation(const FocusTarget& origin,
		PhysicalDirection direction,
		const std::vector<FocusTarget>& internal,
		const std::vector<FocusTarget>& external,
		bool rtl,
		MenuState state = MenuState::Browsing);

/* Direct mode-switch action for bare Tab. */
enum class TabAction : unsigned
{
	ToggleMode,
	Inert,
};

TabAction tab_action(bool places_available);

enum class EscState : unsigned
{
	ContextMenuOpen = 0,
	ResizeInProgress = 1,
	QueryNonEmpty = 2,
	MenuOpen = 3,
};

enum class EscAction : unsigned
{
	CloseContextMenu,
	CancelResize,
	ClearQuery,
	CloseMenu,
};

EscState classify_esc_state(bool context_menu_open,
		bool resize_in_progress,
		bool query_non_empty);
EscAction esc_action(EscState state);

enum class KeyClass : unsigned
{
	ImeComposition,
	ModifierOnly,
	FunctionUtility,
	Printable,
};

KeyClass classify_key(const GdkEventKey* event);
bool is_printable_for_search(const GdkEventKey* event);

/* is_search_text_event:
 * @event: key event received outside the search entry.
 *
 * Identifies committed text and in-progress input-method events that GTK's
 * search entry must own. Modifier-only and shortcut events remain excluded.
 */
bool is_search_text_event(const GdkEventKey* event);

/* is_query_space_key:
 * @keyval: ordinary or keypad space keysym.
 *
 * Returns true for the space keys that the menu turns into one literal query
 * insertion before any button, view, or input-method default binding runs.
 */
bool is_query_space_key(guint keyval);

struct ActivationDebounce
{
	gint64 last_at = 0;
	gint64 threshold_us = 250 * 1000;

	bool accept(gint64 now)
	{
		if (now - last_at >= threshold_us)
		{
			last_at = now;
			return true;
		}
		return false;
	}
};

} // namespace Keyboard
} // namespace WhiskerMenu

#endif // MEOWMENU_CORE_WINDOW_KEYBOARD_H
