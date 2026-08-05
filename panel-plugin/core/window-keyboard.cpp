/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "window-keyboard.h"

#include <algorithm>
#include <tuple>

#include <gdk/gdkkeysyms.h>

namespace WhiskerMenu
{
namespace Keyboard
{

bool NavigationRect::is_valid() const
{
	return width > 0 && height > 0;
}

std::int64_t NavigationRect::center_x2() const
{
	return static_cast<std::int64_t>(x) * 2 + width;
}

std::int64_t NavigationRect::center_y2() const
{
	return static_cast<std::int64_t>(y) * 2 + height;
}

bool normalize_direction(guint keyval, PhysicalDirection* direction)
{
	PhysicalDirection normalized;
	switch (keyval)
	{
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
		normalized = PhysicalDirection::Up;
		break;
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
		normalized = PhysicalDirection::Down;
		break;
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left:
		normalized = PhysicalDirection::Left;
		break;
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right:
		normalized = PhysicalDirection::Right;
		break;
	default:
		return false;
	}

	if (direction)
		*direction = normalized;
	return true;
}

namespace
{

GdkModifierType navigation_modifiers()
{
	return static_cast<GdkModifierType>(
			GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK
			| GDK_SUPER_MASK | GDK_META_MASK | GDK_HYPER_MASK);
}

bool direction_is_forward(const NavigationRect& origin,
		const NavigationRect& candidate, PhysicalDirection direction,
		std::int64_t* forward, std::int64_t* perpendicular)
{
	const std::int64_t dx = candidate.center_x2() - origin.center_x2();
	const std::int64_t dy = candidate.center_y2() - origin.center_y2();
	switch (direction)
	{
	case PhysicalDirection::Up:
		if (dy >= 0)
			return false;
		*forward = -dy;
		*perpendicular = std::llabs(dx);
		return true;
	case PhysicalDirection::Down:
		if (dy <= 0)
			return false;
		*forward = dy;
		*perpendicular = std::llabs(dx);
		return true;
	case PhysicalDirection::Left:
		if (dx >= 0)
			return false;
		*forward = -dx;
		*perpendicular = std::llabs(dy);
		return true;
	case PhysicalDirection::Right:
		if (dx <= 0)
			return false;
		*forward = dx;
		*perpendicular = std::llabs(dy);
		return true;
	}
	return false;
}

bool is_bare_modifier_keyval(guint keyval)
{
	switch (keyval)
	{
	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
	case GDK_KEY_Super_L:
	case GDK_KEY_Super_R:
	case GDK_KEY_Hyper_L:
	case GDK_KEY_Hyper_R:
	case GDK_KEY_Meta_L:
	case GDK_KEY_Meta_R:
	case GDK_KEY_ISO_Level3_Shift:
	case GDK_KEY_ISO_Level5_Shift:
	case GDK_KEY_Caps_Lock:
	case GDK_KEY_Num_Lock:
		return true;
	default:
		return false;
	}
}

bool is_function_utility_keyval(guint keyval)
{
	if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12)
		return true;

	switch (keyval)
	{
	case GDK_KEY_Insert:
	case GDK_KEY_KP_Insert:
	case GDK_KEY_Delete:
	case GDK_KEY_KP_Delete:
	case GDK_KEY_Print:
	case GDK_KEY_Scroll_Lock:
	case GDK_KEY_Pause:
	case GDK_KEY_Menu:
	case GDK_KEY_Home:
	case GDK_KEY_KP_Home:
	case GDK_KEY_End:
	case GDK_KEY_KP_End:
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
	case GDK_KEY_Tab:
	case GDK_KEY_ISO_Left_Tab:
	case GDK_KEY_KP_Tab:
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_Escape:
	case GDK_KEY_BackSpace:
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left:
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right:
		return true;
	default:
		return false;
	}
}

bool altgr_text_event(const GdkEventKey* event, guint32 unichar)
{
	if (!(event->state & GDK_MOD1_MASK)
			|| (event->state & (GDK_CONTROL_MASK | GDK_SUPER_MASK
					| GDK_META_MASK | GDK_HYPER_MASK)))
		return false;

	/* AltGr layouts normally expose a non-ASCII keysym. Keeping ordinary
	 * ASCII Alt shortcuts blocked avoids stealing application accelerators. */
	return unichar >= 0x80;
}

} // namespace

bool is_directional_key(const GdkEventKey* event)
{
	PhysicalDirection ignored;
	return event && !(event->state & navigation_modifiers())
			&& normalize_direction(event->keyval, &ignored);
}

bool target_is_eligible(const FocusTarget& target, MenuState state)
{
	if (!target.usable || !target.rectangle.is_valid())
		return false;
	if (target.region == NavigationRegion::Sidebar
			&& state == MenuState::Searching)
		return false;
	if (target.kind == FocusTargetKind::ModeSwitch
			|| target.kind == FocusTargetKind::Decorative)
		return false;
	return true;
}

std::size_t choose_spatial_target(const NavigationRect& origin,
		PhysicalDirection direction,
		const std::vector<FocusTarget>& targets,
		bool rtl,
		MenuState state)
{
	if (!origin.is_valid())
		return NO_TARGET;

	std::size_t best = NO_TARGET;
	std::tuple<std::int64_t, std::int64_t, std::int64_t,
			std::int64_t, unsigned> best_score;
	for (std::size_t i = 0; i < targets.size(); ++i)
	{
		const FocusTarget& candidate = targets[i];
		if (!target_is_eligible(candidate, state))
			continue;

		std::int64_t forward = 0;
		std::int64_t perpendicular = 0;
		if (!direction_is_forward(origin, candidate.rectangle, direction,
				&forward, &perpendicular))
			continue;

		const std::int64_t visual_y = candidate.rectangle.center_y2();
		const std::int64_t visual_x = candidate.rectangle.center_x2();
		const std::int64_t ordered_x = rtl ? -visual_x : visual_x;
		const auto score = std::make_tuple(forward, perpendicular,
				visual_y, ordered_x, candidate.visual_ordinal);
		if (best == NO_TARGET || score < best_score)
		{
			best = i;
			best_score = score;
		}
	}
	return best;
}

NavigationDecision decide_navigation(const FocusTarget& origin,
		PhysicalDirection direction,
		const std::vector<FocusTarget>& internal,
		const std::vector<FocusTarget>& external,
		bool rtl,
		MenuState state)
{
	const std::size_t internal_target = choose_spatial_target(
			origin.rectangle, direction, internal, rtl, state);
	if (internal_target != NO_TARGET)
		return {NavigationDecisionKind::InternalMove,
				internal[internal_target].target_id};

	const std::size_t external_target = choose_spatial_target(
			origin.rectangle, direction, external, rtl, state);
	if (external_target != NO_TARGET)
		return {NavigationDecisionKind::CrossRegionMove,
				external[external_target].target_id};

	return {NavigationDecisionKind::NoOp, 0};
}

TabAction tab_action(bool places_available)
{
	return places_available ? TabAction::ToggleMode : TabAction::Inert;
}

EscState classify_esc_state(bool context_menu_open,
		bool resize_in_progress, bool query_non_empty)
{
	if (context_menu_open)
		return EscState::ContextMenuOpen;
	if (resize_in_progress)
		return EscState::ResizeInProgress;
	if (query_non_empty)
		return EscState::QueryNonEmpty;
	return EscState::MenuOpen;
}

EscAction esc_action(EscState state)
{
	switch (state)
	{
	case EscState::ContextMenuOpen:  return EscAction::CloseContextMenu;
	case EscState::ResizeInProgress: return EscAction::CancelResize;
	case EscState::QueryNonEmpty:    return EscAction::ClearQuery;
	case EscState::MenuOpen:         return EscAction::CloseMenu;
	}
	return EscAction::CloseMenu;
}

KeyClass classify_key(const GdkEventKey* event)
{
	if (!event)
		return KeyClass::FunctionUtility;
	if (event->state & GDK_MODIFIER_RESERVED_25_MASK)
		return KeyClass::ImeComposition;
	if (event->is_modifier || is_bare_modifier_keyval(event->keyval))
		return KeyClass::ModifierOnly;
	if (is_function_utility_keyval(event->keyval))
		return KeyClass::FunctionUtility;

	const guint32 unichar = gdk_keyval_to_unicode(event->keyval);
	if (unichar == 0 || unichar < 0x20 || unichar == 0x7f)
		return KeyClass::FunctionUtility;

	const GdkModifierType shortcut = static_cast<GdkModifierType>(
			GDK_CONTROL_MASK | GDK_SUPER_MASK | GDK_META_MASK | GDK_HYPER_MASK);
	if (event->state & shortcut)
		return KeyClass::FunctionUtility;
	if ((event->state & GDK_MOD1_MASK)
			&& !altgr_text_event(event, unichar))
		return KeyClass::FunctionUtility;
	return KeyClass::Printable;
}

bool is_printable_for_search(const GdkEventKey* event)
{
	return classify_key(event) == KeyClass::Printable;
}

bool is_search_text_event(const GdkEventKey* event)
{
	if (!event)
		return false;
	const KeyClass key_class = classify_key(event);
	return key_class == KeyClass::Printable
			|| key_class == KeyClass::ImeComposition;
}

bool is_query_space_key(guint keyval)
{
	return keyval == GDK_KEY_space || keyval == GDK_KEY_KP_Space;
}

} // namespace Keyboard
} // namespace WhiskerMenu
