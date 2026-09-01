/*
 * Headless tests for the pure window-frame helpers declared in
 * panel-plugin/core/window-frame.h. The tests use only plain geometry, Cairo,
 * bool render flags, and the CSS contract so this target remains display-free.
 *
 * Pins the radius clamp range [0,24] (contract C2/C7) and the composited
 * rounded-border predicate (contract C3/C4/C7): the single rounded stroke is
 * emitted iff (!is_fullscreen && supports_alpha).
 */

#include "core/window-frame.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

using meow::meowmenu_clamp_corner_radius;
using meow::meowmenu_frameless_launcher_css;
using meow::meowmenu_list_selection_css;
using meow::meowmenu_frame_draws_border;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// Radius below the range clamps up to 0; above the range clamps down to 24;
// in-range values are the identity. This is the [0,24] bound the draw path and
// the live handler must share.
void radius_clamped_to_range()
{
	CHECK(meowmenu_clamp_corner_radius(-1) == 0);
	CHECK(meowmenu_clamp_corner_radius(-100) == 0);
	CHECK(meowmenu_clamp_corner_radius(0) == 0);
	CHECK(meowmenu_clamp_corner_radius(12) == 12);
	CHECK(meowmenu_clamp_corner_radius(24) == 24);
	CHECK(meowmenu_clamp_corner_radius(25) == 24);
	CHECK(meowmenu_clamp_corner_radius(1000) == 24);
}

// The clamp is monotonic: stepping the request up never lowers the result.
void radius_clamp_is_monotonic()
{
	int prev = meowmenu_clamp_corner_radius(-5);
	for (int r = -5; r <= 30; ++r)
	{
		const int cur = meowmenu_clamp_corner_radius(r);
		CHECK(cur >= prev);
		CHECK(cur >= 0 && cur <= 24);
		prev = cur;
	}
}

// The composited rounded-border stroke is emitted only docked + composited.
void border_predicate_truth_table()
{
	CHECK(meowmenu_frame_draws_border(/*is_fullscreen=*/false, /*supports_alpha=*/true)  == true);
	CHECK(meowmenu_frame_draws_border(/*is_fullscreen=*/true,  /*supports_alpha=*/true)  == false);
	CHECK(meowmenu_frame_draws_border(/*is_fullscreen=*/false, /*supports_alpha=*/false) == false);
	CHECK(meowmenu_frame_draws_border(/*is_fullscreen=*/true,  /*supports_alpha=*/false) == false);
}

// The scrollbar container owns the theme-drawn edge beside the trough. Both
// nodes must be neutralised, while the slider remains entirely theme-owned.
void frameless_launcher_css_targets_scrollbar_chrome()
{
	const char* css = meowmenu_frameless_launcher_css();
	CHECK(std::strstr(css,
			"scrolledwindow.launchers-pane scrollbar,") != nullptr);
	CHECK(std::strstr(css,
			"scrolledwindow.launchers-pane scrollbar trough") != nullptr);
	CHECK(std::strstr(css, "scrollbar slider") == nullptr);
	CHECK(std::strstr(css, "border: none") != nullptr);
	CHECK(std::strstr(css, "background-color: transparent") != nullptr);
}

void list_selection_uses_theme_tokens()
{
	const char* css = meowmenu_list_selection_css();
	CHECK(std::strstr(css, "treeview.launchers.view:selected") != nullptr);
	CHECK(std::strstr(css, "@theme_selected_bg_color") != nullptr);
	CHECK(std::strstr(css, "@theme_selected_fg_color") != nullptr);
	CHECK(std::strstr(css, "iconview") == nullptr);
}

} // namespace

int main()
{
	radius_clamped_to_range();
	radius_clamp_is_monotonic();
	border_predicate_truth_table();
	frameless_launcher_css_targets_scrollbar_chrome();
	list_selection_uses_theme_tokens();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_window_frame: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_window_frame: ok\n");
	return EXIT_SUCCESS;
}
