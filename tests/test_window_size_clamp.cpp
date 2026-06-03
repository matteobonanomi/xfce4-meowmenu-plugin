/*
 * Headless tests for the pure window default-size clamp declared in
 * panel-plugin/core/window-size-clamp.h. No GTK/GDK types are used; only the
 * integer logical sizes the helper consumes are fabricated.
 *
 * Asserts the normative decision table (C1..C7) in
 * .specify/specs/019-hidpi-audit/contracts/size-clamp.md. The cases pin
 * behaviour, not the exact edge-margin fraction.
 */

#include "core/window-size-clamp.h"

#include <cstdio>
#include <cstdlib>

using meow::clamp_default_size;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// C1: desired fits comfortably in a large work area -> unchanged. This is the
// 1x / large-screen no-op that guarantees the standard-DPI layout never moves.
void c1_fits_unchanged()
{
	int w = 0, h = 0;
	clamp_default_size(820, 600, 3840, 2160, &w, &h);
	CHECK(w == 820);
	CHECK(h == 600);
}

// C2: unresolved monitor reports 0x0 -> "no constraint" -> unchanged.
void c2_unresolved_monitor_unchanged()
{
	int w = 0, h = 0;
	clamp_default_size(820, 600, 0, 0, &w, &h);
	CHECK(w == 820);
	CHECK(h == 600);
}

// C3: oversized against a small work area -> shrunk to fit on both axes,
// staying within the work area and never collapsing below 1.
void c3_oversized_shrinks_both_axes()
{
	int w = 0, h = 0;
	clamp_default_size(820, 600, 640, 480, &w, &h);
	CHECK(w <= 640);
	CHECK(h <= 480);
	CHECK(w >= 1);
	CHECK(h >= 1);
}

// C4: only the constrained axis shrinks; the comfortably-fitting axis is
// passed through unchanged.
void c4_per_axis()
{
	int w = 0, h = 0;
	clamp_default_size(820, 600, 800, 2160, &w, &h);
	CHECK(w <= 800);
	CHECK(h == 600);
}

// C5: a negative work-area dimension is treated as "no constraint" -> unchanged.
void c5_negative_workarea_unchanged()
{
	int w = 0, h = 0;
	clamp_default_size(820, 600, -1, -1, &w, &h);
	CHECK(w == 820);
	CHECK(h == 600);
}

// C6: even a 1x1 work area must never collapse the window to zero.
void c6_never_zero()
{
	int w = 0, h = 0;
	clamp_default_size(820, 600, 1, 1, &w, &h);
	CHECK(w >= 1);
	CHECK(h >= 1);
}

// C7: shrink-only invariant across a spread of work areas -> the output never
// exceeds the desired size, which is what makes the change a no-op at 1x.
void c7_shrink_only_invariant()
{
	const int desired_w = 820, desired_h = 600;
	const int work[][2] = {
		{ 3840, 2160 }, { 0, 0 }, { 640, 480 }, { 800, 2160 },
		{ -1, -1 }, { 1, 1 }, { 1024, 768 }, { 820, 600 },
	};
	for (const auto& a : work)
	{
		int w = 0, h = 0;
		clamp_default_size(desired_w, desired_h, a[0], a[1], &w, &h);
		CHECK(w <= desired_w);
		CHECK(h <= desired_h);
	}
}

} // namespace

int main()
{
	c1_fits_unchanged();
	c2_unresolved_monitor_unchanged();
	c3_oversized_shrinks_both_axes();
	c4_per_axis();
	c5_negative_workarea_unchanged();
	c6_never_zero();
	c7_shrink_only_invariant();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_window_size_clamp: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_window_size_clamp: ok\n");
	return EXIT_SUCCESS;
}
