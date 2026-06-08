/*
 * Headless tests for the pure opacity model declared in
 * panel-plugin/core/opacity-model.h. No GTK/GDK types are used; only the
 * integer opacity percentages the helper consumes are fabricated.
 *
 * Asserts the rendering contract (C1..C3, C6) in
 * .specify/specs/026-opacity-bugfix/contracts/opacity-rendering.md:
 *   C1 — percentage->alpha mapping (endpoints, monotonicity, clamping, no gap)
 *   C2 — per-mode region alpha assignment table
 *   C3 — no compounding (each region's alpha is its single governing value)
 *   C6 — default-appearance invariance (all controls at 100 -> active regions 1.0)
 */

#include "core/opacity-model.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

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

// C2: docked mode -> transparent window, each region owns its own alpha.
void c2_docked_assignment()
{
	OpacityRegionAlphas a = meowmenu_region_alphas(false, 80, 70, 100);
	CHECK(eq(a.window, 0.0));
	CHECK(eq(a.categories, 0.8));
	CHECK(eq(a.apps, 0.7));
}

// C2: full-screen mode -> single window alpha, both regions transparent.
void c2_fullscreen_assignment()
{
	OpacityRegionAlphas a = meowmenu_region_alphas(true, 80, 70, 50);
	CHECK(eq(a.window, 0.5));
	CHECK(eq(a.categories, 0.0));
	CHECK(eq(a.apps, 0.0));
}

// C2: in full-screen the docked controls do not leak into any region, so the
// whole surface reads at exactly alpha(full_screen_opacity).
void c2_fullscreen_ignores_docked_controls()
{
	OpacityRegionAlphas a = meowmenu_region_alphas(true, 0, 100, 30);
	CHECK(eq(a.window, 0.3));
	CHECK(eq(a.categories, 0.0));
	CHECK(eq(a.apps, 0.0));
}

// C3: no compounding — each active region's alpha equals exactly its single
// governing value's alpha, never 1-(1-a)(1-b).
void c3_no_compounding()
{
	OpacityRegionAlphas a = meowmenu_region_alphas(false, 50, 50, 100);
	// If categories compounded onto apps the apps alpha would be
	// 1-(1-0.5)(1-0.5) = 0.75; the contract requires the bare 0.5.
	CHECK(eq(a.apps, 0.5));
	CHECK(eq(a.categories, 0.5));
}

// C3: apps_opacity 0 in docked reaches a true transparent floor regardless of
// the categories value — the reported "minimum limit" defect must be gone.
void c3_apps_zero_is_true_zero()
{
	OpacityRegionAlphas a = meowmenu_region_alphas(false, 100, 0, 100);
	CHECK(eq(a.apps, 0.0));
	// The categories floor must not bleed into the apps region.
	CHECK(eq(a.window, 0.0));
}

// C6: with every control at 100 the active-mode regions are fully solid in
// both modes (default-appearance invariance).
void c6_default_invariance()
{
	OpacityRegionAlphas docked = meowmenu_region_alphas(false, 100, 100, 100);
	CHECK(eq(docked.categories, 1.0));
	CHECK(eq(docked.apps, 1.0));

	OpacityRegionAlphas full = meowmenu_region_alphas(true, 100, 100, 100);
	CHECK(eq(full.window, 1.0));
}

} // namespace

int main()
{
	c1_endpoints();
	c1_strict_monotonic_no_dead_zone();
	c1_linear_midpoint();
	c1_clamping();
	c2_docked_assignment();
	c2_fullscreen_assignment();
	c2_fullscreen_ignores_docked_controls();
	c3_no_compounding();
	c3_apps_zero_is_true_zero();
	c6_default_invariance();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "%d check(s) failed\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("all opacity-model checks passed\n");
	return EXIT_SUCCESS;
}
