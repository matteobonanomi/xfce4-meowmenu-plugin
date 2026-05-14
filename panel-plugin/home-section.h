/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_HOME_SECTION_H
#define WHISKERMENU_HOME_SECTION_H

#include "places-section.h"

namespace WhiskerMenu
{

class HomeSection : public PlacesSection
{
public:
	HomeSection();
	~HomeSection() override;

	std::vector<PlacesItem*> get_items(int max) override;
	void clear_items() override;
	const gchar* get_icon_name() const override { return "user-home"; }
	const gchar* get_display_name() const override;

private:
	std::vector<PlacesItem*> m_items;
};

}

#endif // WHISKERMENU_HOME_SECTION_H
