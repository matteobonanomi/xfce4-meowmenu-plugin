/*
 * Domain agreement coverage for persisted layout strings.
 */

#include <cassert>
#include <cstring>

#include "core/layout-mode.h"
#include "core/sidebar-layout.h"

using namespace WhiskerMenu;

namespace
{

LayoutMode legacy_layout_mode(const char* value)
{
	if (value && std::strcmp(value, "fullscreen") == 0)
		return LayoutMode::FullScreen;
	if (value && std::strcmp(value, "centered") == 0)
		return LayoutMode::Centered;
	return LayoutMode::Docked;
}

SidebarPosition legacy_sidebar_position(const char* value)
{
	if (value && std::strcmp(value, "right") == 0)
		return SidebarPosition::Right;
	if (value && std::strcmp(value, "horizontal") == 0)
		return SidebarPosition::Horizontal;
	return SidebarPosition::Left;
}

/* layout_mode_domain_agrees:
 *
 * Verifies that every stored or fallback value retains its established mode
 * and the resulting toplevel-width and viewport-cap decisions.
 */
void layout_mode_domain_agrees()
{
	const char* values[] = {
		"docked", "centered", "fullscreen", "", "FULLSCREEN",
		"unknown", nullptr
	};

	for (const char* value : values)
	{
		const LayoutMode expected = legacy_layout_mode(value);
		const LayoutMode actual = layout_mode_from_key(value);
		assert(actual == expected);

		const int requested_width = 517;
		const int workarea_width = 1919;
		const int toplevel_width = actual == LayoutMode::FullScreen
				? workarea_width : requested_width;
		const int viewport_cap = actual == LayoutMode::FullScreen
				? meow_fullscreen_main_column(workarea_width).width : -1;
		assert(toplevel_width == (expected == LayoutMode::FullScreen
				? 1919 : 517));
		assert(viewport_cap == (expected == LayoutMode::FullScreen
				? 1281 : -1));
	}
}

/* sidebar_position_domain_agrees:
 *
 * Verifies that all stored and fallback values retain their established
 * physical position and visible category presentation.
 */
void sidebar_position_domain_agrees()
{
	const char* values[] = {
		"left", "right", "horizontal", "", "top", "bottom",
		"hidden", "RIGHT", "unknown", nullptr
	};

	for (const char* value : values)
	{
		const SidebarPosition expected = legacy_sidebar_position(value);
		const SidebarPosition actual = meow_parse_sidebar_position(value);
		assert(actual == expected);

		SidebarLayoutState state = {};
		state.sidebar_enabled = true;
		state.position = actual;
		state.category_show_name = true;
		const SidebarPresentation presentation =
				meow_compute_sidebar_presentation(state);
		assert(presentation.sidebar_visible);
		assert(presentation.categories_horizontal
				== (expected == SidebarPosition::Horizontal));
		assert(presentation.effective_show_category_names
				== (expected != SidebarPosition::Horizontal));

		state.sidebar_enabled = false;
		const SidebarPresentation hidden =
				meow_compute_sidebar_presentation(state);
		assert(!hidden.sidebar_visible);
		assert(!hidden.categories_horizontal);
		assert(hidden.effective_show_category_names);
		assert(hidden.show_default_category_heading);
	}
}

}

int main()
{
	layout_mode_domain_agrees();
	sidebar_position_domain_agrees();
	return 0;
}
