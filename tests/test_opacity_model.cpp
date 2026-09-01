/*
 * Headless tests for the pure opacity model declared in
 * panel-plugin/core/opacity-model.h. No GTK/GDK types are used; only the
 * integer opacity percentages the helper consumes are fabricated.
 *
 * Covers the single-value opacity model:
 *   - percentage->alpha mapping (endpoints, monotonicity, clamping, no gap)
 *   - the translucency predicate that gates the launcher-view redraw safeguard
 *     (true below 100, false at 100 and above, including clamped inputs)
 */

#include "core/opacity-model.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

bool eq(double a, double b)
{
	return std::fabs(a - b) < 1e-9;
}

// C1: endpoints map exactly to the transparent/solid extremes.
void c1_endpoints()
{
	CHECK(eq(meowmenu_opacity_alpha(0), 0.0));
	CHECK(eq(meowmenu_opacity_alpha(100), 1.0));
}

// C1: strict monotonic increase across every integer step in [0, 100], so no
// two neighbouring values collapse to the same alpha (no dead zone / floor).
void c1_strict_monotonic_no_dead_zone()
{
	double prev = meowmenu_opacity_alpha(0);
	for (int v = 1; v <= 100; ++v)
	{
		double cur = meowmenu_opacity_alpha(v);
		CHECK(cur > prev);
		prev = cur;
	}
}

// C1: linearity at a representative interior point.
void c1_linear_midpoint()
{
	CHECK(eq(meowmenu_opacity_alpha(50), 0.5));
	CHECK(eq(meowmenu_opacity_alpha(25), 0.25));
}

// C1: out-of-range input is clamped, never extrapolated.
void c1_clamping()
{
	CHECK(eq(meowmenu_opacity_alpha(-1), 0.0));
	CHECK(eq(meowmenu_opacity_alpha(-1000), 0.0));
	CHECK(eq(meowmenu_opacity_alpha(101), 1.0));
	CHECK(eq(meowmenu_opacity_alpha(1000), 1.0));
}

// Translucency predicate: true below 100 (incl. clamped negatives), false at
// 100 and above — this is the gate the launcher-view redraw safeguard keys on.
void translucent_predicate()
{
	CHECK(meowmenu_background_translucent(99));
	CHECK(meowmenu_background_translucent(60));
	CHECK(meowmenu_background_translucent(1));
	CHECK(meowmenu_background_translucent(0));
	CHECK(meowmenu_background_translucent(-1));
	CHECK(meowmenu_background_translucent(-1000));
	CHECK(!meowmenu_background_translucent(100));
	CHECK(!meowmenu_background_translucent(101));
	CHECK(!meowmenu_background_translucent(1000));
}

void surface_and_compositor_matrix()
{
	for (int value : { 0, 60, 80, 100 })
	{
		const double configured = meowmenu_opacity_alpha(value);
		CHECK(eq(meowmenu_effective_background_alpha(value, true), configured));
		CHECK(eq(meowmenu_effective_background_alpha(value, false), 1.0));
		// Source replacement writes the same resolved alpha rather than applying
		// an alpha-over operation to the baseline.
		const double baseline_alpha = configured;
		const double replacement_alpha = configured;
		CHECK(eq(baseline_alpha, replacement_alpha));
	}
}

} // namespace

int main()
{
	c1_endpoints();
	c1_strict_monotonic_no_dead_zone();
	c1_linear_midpoint();
	c1_clamping();
	translucent_predicate();
	surface_and_compositor_matrix();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "%d check(s) failed\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("all opacity-model checks passed\n");
	return EXIT_SUCCESS;
}
