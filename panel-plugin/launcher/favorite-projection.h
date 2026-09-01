/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WHISKERMENU_FAVORITE_PROJECTION_H
#define WHISKERMENU_FAVORITE_PROJECTION_H

#include <string>
#include <vector>

namespace WhiskerMenu
{

/* favorite_resolved_projection:
 * @stored: durable desktop identifiers in their user-defined order.
 * @available: identifiers present in the current application publication.
 *
 * Builds the visible favourite order without changing durable configuration.
 * Empty and unresolved identifiers remain solely in @stored.
 *
 * Returns: the resolved identifiers in stored order.
 */
std::vector<std::string> favorite_resolved_projection(
		const std::vector<std::string>& stored,
		const std::vector<std::string>& available);

/* favorite_merge_resolved_order:
 * @stored: durable desktop identifiers, including unresolved entries.
 * @resolved_order: complete new order of the currently resolved identifiers.
 * @available: identifiers present in the current application publication.
 *
 * Replaces resolved slots only. Unresolved identifiers retain their exact
 * values, positions, and relative order.
 *
 * Returns: the merged durable order, or @stored when @resolved_order is not a
 * complete permutation of the current resolved projection.
 */
std::vector<std::string> favorite_merge_resolved_order(
		const std::vector<std::string>& stored,
		const std::vector<std::string>& resolved_order,
		const std::vector<std::string>& available);

/* favorite_append_exact:
 * @stored: durable identifiers to update.
 * @desktop_id: exact non-empty identifier to append.
 *
 * Returns: true when a new identifier was appended.
 */
bool favorite_append_exact(std::vector<std::string>& stored,
		const std::string& desktop_id);

/* favorite_remove_exact:
 * @stored: durable identifiers to update.
 * @desktop_id: exact identifier to remove.
 *
 * Returns: true when a matching identifier was removed.
 */
bool favorite_remove_exact(std::vector<std::string>& stored,
		const std::string& desktop_id);

}

#endif // WHISKERMENU_FAVORITE_PROJECTION_H
