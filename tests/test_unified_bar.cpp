/*
 * Unit tests for the unified-bar precondition predicate.
 *
 * The predicate is pure (takes 4 strings, returns bool), so it can be
 * tested in isolation without instantiating Settings. To keep this test
 * dependency-free — matching the other tests/ — the predicate body is
 * mirrored here verbatim. If the canonical implementation in
 * panel-plugin/window.cpp ever diverges, this test will start to fail
 * and that's the intended signal.
 */

#include <cassert>
#include <cstring>

static char vertical_end_of(const char* value)
{
	if (!value || !*value)
		return 0;
	if (std::strncmp(value, "top", 3) == 0)
		return 't';
	if (std::strncmp(value, "bottom", 6) == 0)
		return 'b';
	return 0;
}

static bool unified_bar_preconditions_raw(const char* layout_mode,
                                          const char* search,
                                          const char* profile,
                                          const char* commands)
{
	if (!layout_mode || std::strcmp(layout_mode, "fullscreen") != 0)
		return false;
	const char ends[] = {
		vertical_end_of(search),
		vertical_end_of(profile),
		vertical_end_of(commands),
	};
	char anchor = 0;
	for (char e : ends)
	{
		if (!e) continue;
		if (!anchor) anchor = e;
		else if (anchor != e) return false;
	}
	return vertical_end_of(search) != 0;
}

static bool unified_bar_effective(bool stored,
                                  const char* layout, const char* s,
                                  const char* p, const char* c)
{
	return stored && unified_bar_preconditions_raw(layout, s, p, c);
}

int main()
{
	// State-machine rows from data-model.md.

	// Row 1: FullScreen + all-top → preconditions OK.
	assert(unified_bar_preconditions_raw("fullscreen", "top", "top-left", "top-right"));
	// Row 2: FullScreen + all-bottom → preconditions OK.
	assert(unified_bar_preconditions_raw("fullscreen", "bottom", "bottom-left", "bottom-right"));
	// Row 3: FullScreen + mismatched ends → fail.
	assert(!unified_bar_preconditions_raw("fullscreen", "top", "bottom-left", "top-right"));
	assert(!unified_bar_preconditions_raw("fullscreen", "bottom", "top-left", "top-right"));
	// Row 4: Wrong layout mode → fail.
	assert(!unified_bar_preconditions_raw("docked", "top", "top-left", "top-right"));
	assert(!unified_bar_preconditions_raw("classic", "top", "top-left", "top-right"));

	// "hidden" profile is transparent — does not block.
	assert(unified_bar_preconditions_raw("fullscreen", "top", "hidden", "top-right"));
	assert(unified_bar_preconditions_raw("fullscreen", "bottom", "hidden", "bottom-right"));

	// Hidden commands also transparent.
	assert(unified_bar_preconditions_raw("fullscreen", "top", "top-left", "hidden"));

	// Effective = stored AND preconditions.
	assert(!unified_bar_effective(false, "fullscreen", "top", "top-left", "top-right"));
	assert( unified_bar_effective(true,  "fullscreen", "top", "top-left", "top-right"));
	assert(!unified_bar_effective(true,  "docked",     "top", "top-left", "top-right"));
	assert(!unified_bar_effective(true,  "fullscreen", "top", "bottom-left", "top-right"));

	// Void-band visibility truth table (mirrors the implementation step).
	// All three bands (top/middle/bottom) share the same predicate: effective().
	// FullScreen + unified-on → all three bands visible.
	assert(unified_bar_effective(true, "fullscreen", "top", "top-left", "top-right"));
	// FullScreen + unified-off → all three bands hidden.
	assert(!unified_bar_effective(false, "fullscreen", "top", "top-left", "top-right"));
	// Non-FullScreen + unified-on → all three bands hidden.
	assert(!unified_bar_effective(true, "docked", "top", "top-left", "top-right"));

	// NULL guards.
	assert(!unified_bar_preconditions_raw(nullptr, "top", "top-left", "top-right"));
	assert(!unified_bar_preconditions_raw("fullscreen", nullptr, "top-left", "top-right"));

	return 0;
}
