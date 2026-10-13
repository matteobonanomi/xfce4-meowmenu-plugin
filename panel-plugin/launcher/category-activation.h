/*
 * Copyright (C) 2026 MeowMenu Contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_CATEGORY_ACTIVATION_H
#define MEOWMENU_CATEGORY_ACTIVATION_H

#include <glib.h>

namespace WhiskerMenu
{

class CategoryActivation
{
public:
	static guint hover_delay_ms() { return 150; }

	void note_keyboard_navigation() { m_hover_suppressed_until_motion = true; }
	void note_pointer_motion() { m_hover_suppressed_until_motion = false; }

	bool hover_is_suppressed() const
	{
		return m_hover_suppressed_until_motion;
	}

	bool hover_should_schedule(bool hover_enabled, bool active) const
	{
		return hover_enabled && !active;
	}

	bool hover_should_activate(bool pointer_inside) const
	{
		return !m_hover_suppressed_until_motion && pointer_inside;
	}

	bool focus_should_activate(bool hover_enabled, bool active) const
	{
		return hover_enabled && !m_hover_suppressed_until_motion && !active;
	}

	bool keyboard_should_activate(bool hover_enabled) const
	{
		return hover_enabled;
	}

private:
	bool m_hover_suppressed_until_motion = false;
};

}

#endif // MEOWMENU_CATEGORY_ACTIVATION_H
