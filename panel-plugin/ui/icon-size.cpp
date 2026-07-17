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

#include "icon-size.h"

#include "settings.h"

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

IconSize::IconSize(Settings* settings, const gchar* property, const int size) :
	m_settings(settings),
	m_property(property),
	m_default(CLAMP(size, NONE, Largest)),
	m_size(m_default)
{
}

//-----------------------------------------------------------------------------

int IconSize::get_size() const
{
	return pixels_for(static_cast<Size>(m_size));
}

//-----------------------------------------------------------------------------

int IconSize::pixels_for(Size alias)
{
	int pixels = 0;
	switch (alias)
	{
		case NONE:     pixels =   1; break;
		case Smallest: pixels =  16; break;
		case Smaller:  pixels =  24; break;
		case Small:    pixels =  32; break;
		case Normal:   pixels =  48; break;
		case Large:    pixels =  64; break;
		case Larger:   pixels =  96; break;
		case Largest:  pixels = 128; break;
		default:       pixels =   0; break;
	}
	return pixels;
}

//-----------------------------------------------------------------------------

std::vector<std::string> IconSize::get_strings()
{
	return {
		_("None"),
		_("Very Small"),
		_("Smaller"),
		_("Small"),
		_("Normal"),
		_("Large"),
		_("Larger"),
		_("Very Large")
	};
}

//-----------------------------------------------------------------------------

void IconSize::load(XfceRc* rc, bool is_default)
{
	set(xfce_rc_read_int_entry(rc, m_property + 1, m_size), !is_default);

	if (is_default)
	{
		m_default = m_size;
	}
}

//-----------------------------------------------------------------------------

bool IconSize::load(const gchar* property, const GValue* value)
{
	if (g_strcmp0(m_property, property) != 0)
	{
		return false;
	}

	set(G_VALUE_HOLDS_INT(value) ? g_value_get_int(value) : m_default, false);

	return true;
}

//-----------------------------------------------------------------------------

void IconSize::set(int size, bool store)
{
	size = CLAMP(size, NONE, Largest);
	if (m_size == size)
	{
		return;
	}

	m_size = size;

	if (store && m_settings->channel)
	{
		m_settings->begin_property_update();
		xfconf_channel_set_int(m_settings->channel, m_property, m_size);
		m_settings->end_property_update();
	}
}

//-----------------------------------------------------------------------------
