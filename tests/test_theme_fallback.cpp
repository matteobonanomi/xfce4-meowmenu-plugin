/* Production-linked theme fallback and palette checks. */

#include "core/theme-fallback.h"

#include <cstdio>
#include <cmath>

using namespace WhiskerMenu;

namespace
{

int failures = 0;

#define CHECK(value) do { \
	if (!(value)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); \
		++failures; \
	} \
} while (0)

GdkRGBA rgb(double value)
{
	return GdkRGBA{value, value, value, 1.0};
}

void test_fallback_decision()
{
	const GdkRGBA dark = rgb(0.12);
	const GdkRGBA light = rgb(0.961);
	const GdkRGBA white_text = rgb(0.93);
	const GdkRGBA black_text = rgb(0.08);
	const GdkRGBA dark_result = meow_choose_background_fallback(true,
			&white_text, false);
	const GdkRGBA light_result = meow_choose_background_fallback(true,
			&black_text, false);
	CHECK(std::fabs(dark_result.red - dark.red) < 1e-9);
	CHECK(std::fabs(light_result.red - light.red) < 1e-9);
	CHECK(meow_choose_background_fallback(false, nullptr, true).red == dark.red);
}

void test_palette_fallback()
{
	const GdkRGBA fallback = rgb(0.12);
	const ThemeSurfacePalette palette = meow_resolve_surface_palette(
			false, nullptr, false, nullptr, fallback);
	CHECK(palette.source == ThemePaletteSource::DarkFallback);
	CHECK(palette.distinguishable);
	CHECK(palette.fullscreen.red == fallback.red);
	CHECK(meow_resolve_layout_metrics(nullptr, 0).region_gap_px == 6);
	CHECK(meow_resolve_boundary_gap(true, true, 8) == 8);
	CHECK(meow_resolve_boundary_gap(false, true, 8) == 0);
}

}

int main()
{
	test_fallback_decision();
	test_palette_fallback();
	return failures == 0 ? 0 : 1;
}
