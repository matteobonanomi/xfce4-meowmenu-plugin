/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "favorite-projection.h"

#include <algorithm>
#include <unordered_set>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

std::vector<std::string> WhiskerMenu::favorite_resolved_projection(
		const std::vector<std::string>& stored,
		const std::vector<std::string>& available)
{
	const std::unordered_set<std::string> available_ids(
			available.begin(), available.end());
	std::vector<std::string> resolved;
	resolved.reserve(stored.size());
	for (const auto& desktop_id : stored)
	{
		if (!desktop_id.empty() && available_ids.count(desktop_id))
		{
			resolved.push_back(desktop_id);
		}
	}
	return resolved;
}

//-----------------------------------------------------------------------------

std::vector<std::string> WhiskerMenu::favorite_merge_resolved_order(
		const std::vector<std::string>& stored,
		const std::vector<std::string>& resolved_order,
		const std::vector<std::string>& available)
{
	const std::vector<std::string> current =
			favorite_resolved_projection(stored, available);
	std::unordered_multiset<std::string> expected(current.begin(), current.end());
	std::unordered_multiset<std::string> supplied(
			resolved_order.begin(), resolved_order.end());
	if (expected != supplied)
	{
		return stored;
	}

	const std::unordered_set<std::string> available_ids(
			available.begin(), available.end());
	std::vector<std::string> merged(stored);
	std::vector<std::string>::const_iterator next = resolved_order.begin();
	for (auto& desktop_id : merged)
	{
		if (!desktop_id.empty() && available_ids.count(desktop_id))
		{
			desktop_id = *next++;
		}
	}
	return merged;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::favorite_append_exact(std::vector<std::string>& stored,
		const std::string& desktop_id)
{
	if (desktop_id.empty()
			|| std::find(stored.begin(), stored.end(), desktop_id) != stored.end())
	{
		return false;
	}
	stored.push_back(desktop_id);
	return true;
}

//-----------------------------------------------------------------------------

bool WhiskerMenu::favorite_remove_exact(std::vector<std::string>& stored,
		const std::string& desktop_id)
{
	const auto found = std::find(stored.begin(), stored.end(), desktop_id);
	if (found == stored.end())
	{
		return false;
	}
	stored.erase(found);
	return true;
}
