/*
 * Headless tests for the theme-aware background fallback chooser declared in
 * panel-plugin/core/theme-fallback.h. No GTK widgets are instantiated; only
 * GdkRGBA values and the prefer-dark boolean are fabricated.
 *
 * Asserts the normative decision table for contrast-preserving fallback colors.
 */

#include "core/theme-fallback.h"

#include <cstdio>
#include <cstdlib>

using WhiskerMenu::meow_choose_background_fallback;
using WhiskerMenu::meow_relative_luminance;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// Exact fallback constants the chooser must return (guards invariant C2).
const GdkRGBA DARK  = { 0.12,  0.12,  0.12,  1.0 }; // rgb(31,31,31)
const GdkRGBA LIGHT = { 0.961, 0.961, 0.961, 1.0 }; // rgb(245,245,245)

bool rgba_equal(const GdkRGBA& a, const GdkRGBA& b)
{
	return a.red == b.red && a.green == b.green
	    && a.blue == b.blue && a.alpha == b.alpha;
}

GdkRGBA make_rgb(double r, double g, double b)
{
	GdkRGBA c = { r, g, b, 1.0 };
	return c;
}

// Decision-table row 2: light text → dark background.
void light_text_gives_dark()
{
	GdkRGBA fg = make_rgb(238.0 / 255.0, 238.0 / 255.0, 238.0 / 255.0);
	CHECK(rgba_equal(meow_choose_background_fallback(TRUE, &fg, FALSE), DARK));
}

// Decision-table row 3: dark text → light background.
void dark_text_gives_light()
{
	GdkRGBA fg = make_rgb(34.0 / 255.0, 34.0 / 255.0, 34.0 / 255.0);
	CHECK(rgba_equal(meow_choose_background_fallback(TRUE, &fg, FALSE), LIGHT));
}

// Decision-table row 4: no fg + prefer-dark TRUE → dark.
void no_fg_prefer_dark_gives_dark()
{
	CHECK(rgba_equal(meow_choose_background_fallback(FALSE, nullptr, TRUE), DARK));
}

// Decision-table row 5: no fg + prefer-dark FALSE/unknown → dark (the documented behavior).
void no_fg_no_prefer_gives_dark()
{
	CHECK(rgba_equal(meow_choose_background_fallback(FALSE, nullptr, FALSE), DARK));
}

// Luminance ordering sanity plus the documented Y == 0.5 tie behaviour.
void luminance_ordering()
{
	GdkRGBA white = make_rgb(1.0, 1.0, 1.0);
	GdkRGBA black = make_rgb(0.0, 0.0, 0.0);
	CHECK(meow_relative_luminance(&white) > 0.5);
	CHECK(meow_relative_luminance(&black) < 0.5);

	// A grey whose luminance lands exactly on the boundary classifies as
	// "light text" → dark background (the Y >= 0.5 tie). Luminance weights
	// sum to 1.0, so an equal-component grey at 0.5 yields Y == 0.5.
	GdkRGBA boundary = make_rgb(0.5, 0.5, 0.5);
	CHECK(meow_relative_luminance(&boundary) == 0.5);
	CHECK(rgba_equal(meow_choose_background_fallback(TRUE, &boundary, FALSE), DARK));
}

// Invariant C2: the returned colours are exactly the two constants, alpha 1.0.
void returns_exact_constants()
{
	GdkRGBA light_fg = make_rgb(0.9, 0.9, 0.9);
	GdkRGBA dark_fg  = make_rgb(0.1, 0.1, 0.1);
	GdkRGBA d = meow_choose_background_fallback(TRUE, &light_fg, FALSE);
	GdkRGBA l = meow_choose_background_fallback(TRUE, &dark_fg, FALSE);

	CHECK(d.red == 0.12 && d.green == 0.12 && d.blue == 0.12 && d.alpha == 1.0);
	CHECK(l.red == 0.961 && l.green == 0.961 && l.blue == 0.961 && l.alpha == 1.0);
}

} // namespace

int main()
{
	light_text_gives_dark();
	dark_text_gives_light();
	no_fg_prefer_dark_gives_dark();
	no_fg_no_prefer_gives_dark();
	luminance_ordering();
	returns_exact_constants();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_theme_fallback: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_theme_fallback: ok\n");
	return EXIT_SUCCESS;
}
