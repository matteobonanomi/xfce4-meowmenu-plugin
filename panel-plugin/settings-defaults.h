/*
 * Copyright (C) 2026 Matteo Bonanomi
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
 */

#ifndef WHISKERMENU_SETTINGS_DEFAULTS_H
#define WHISKERMENU_SETTINGS_DEFAULTS_H

#include <cstddef>
#include <cctype>
#include <cstring>

typedef struct _XfconfChannel XfconfChannel;

namespace WhiskerMenu
{

/* should_apply_fresh_preset:
 * @marker: true when the profile has previously completed initialization.
 * @empty_channel: true when no plugin settings existed at load time.
 *
 * Keeps first-install preset application separate from upgrade migration. An
 * existing profile is never reset merely because its marker needs backfilling.
 *
 * Returns: true only for a genuinely new profile.
 */
inline bool should_apply_fresh_preset(bool marker, bool empty_channel)
{
	return !marker && empty_channel;
}

constexpr int COMPOSITION_RESET_GENERATION = 1;
constexpr const char* COMPOSITION_RESET_GENERATION_KEY =
	"/migration/composition-reset-generation";
constexpr const char* COMPOSITION_RESET_STATE_KEY =
	"/migration/composition-reset-state";

enum class PreStableResetDecision
{
	Fresh,
	Reset,
	Load,
};

/* decide_pre_stable_reset:
 * @generation: persisted reset generation, or zero when absent.
 * @state: persisted lifecycle state, or an empty string when absent.
 * @initialized: whether the instance had completed older initialization.
 * @non_lifecycle_properties: number of stored instance descendants excluding
 *   the reset lifecycle keys and the bare registration value.
 *
 * Classifies one instance without consulting product version strings. Pending
 * and incomplete generations retry, initialized/nonempty instances reset, and
 * a genuinely empty instance receives fresh defaults.
 *
 * Returns: the startup action for this instance.
 */
inline PreStableResetDecision decide_pre_stable_reset(int generation,
	const char* state,
	bool initialized,
	std::size_t non_lifecycle_properties)
{
	const char* stored_state = state ? state : "";
	if (generation == COMPOSITION_RESET_GENERATION
			&& std::strcmp(stored_state, "complete") == 0)
		return PreStableResetDecision::Load;
	if (generation == COMPOSITION_RESET_GENERATION
			&& std::strcmp(stored_state, "pending") == 0)
		return PreStableResetDecision::Reset;
	return (initialized || non_lifecycle_properties > 0)
		? PreStableResetDecision::Reset
		: PreStableResetDecision::Fresh;
}

/* inspect_pre_stable_reset:
 * @channel: property-base-anchored panel Xfconf channel.
 * @property_base: concrete per-instance base such as
 *   "/plugins/meowmenu-7".
 *
 * Reads only lifecycle and existence state. A malformed base fails closed and
 * returns Load so no destructive operation can be attempted.
 *
 * Returns: the startup action for this instance.
 */
PreStableResetDecision inspect_pre_stable_reset(XfconfChannel* channel,
	const char* property_base);

inline bool valid_meowmenu_property_base(const char* property_base)
{
	static const char prefix[] = "/plugins/meowmenu-";
	if (!property_base || std::strncmp(property_base, prefix,
			sizeof(prefix) - 1) != 0)
		return false;
	const char* id = property_base + sizeof(prefix) - 1;
	if (!*id)
		return false;
	for (const char* p = id; *p; ++p)
	{
		if (!std::isdigit(static_cast<unsigned char>(*p)))
			return false;
	}
	return true;
}

/* complete_pre_stable_reset:
 * @channel: property-base-anchored panel Xfconf channel.
 *
 * Verifies the required Modern state and records completion last. Call only
 * after defaults have been persisted.
 *
 * Returns: true when the required values were readable and completion was
 * persisted.
 */
bool complete_pre_stable_reset(XfconfChannel* channel);

}

/* settings-defaults: private translation unit for the schema-version
 * defaults table and the forward schema migration that seeds it. The
 * declarations live alongside Settings (settings.h) because they are
 * member functions; this header exists only so the implementation file
 * remains a recognised compilation unit with a matching header.
 *
 * The Xfconf legacy-import path used on first launch from Whisker lives
 * in migration.cpp and is intentionally NOT consolidated here.
 */

#endif // WHISKERMENU_SETTINGS_DEFAULTS_H
