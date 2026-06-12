/*
 * Headless tests for the pure icon-grid cell-width arithmetic declared in
 * panel-plugin/ui/grid-cell-metrics.h. No GTK types are used.
 *
 * Asserts that the per-cell minimum/natural width depends only on padding,
 * icon size, and the stretch flag — never on label text (INV-6) — across the
 * full IconSize pixel range and several densities, and reproduces the original
 * inline renderer formula exactly.
 */

#include "ui/grid-cell-metrics.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace WhiskerMenu;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// Reference reproduction of the original inline renderer arithmetic, kept
// separate so the helper is checked against an independent copy of the formula.
void expected(int pad, int size, bool stretch, int* min_out, int* nat_out)
{
	int width = (pad * 2) + size;
	if (stretch)
	{
		width += 76 - (size / 4);
		*min_out = width;
		*nat_out = (width * 2) - 1;
	}
	else
	{
		*min_out = width;
		*nat_out = width;
	}
}

// Non-stretch cells: minimum == natural == padding*2 + size, for every shipped
// IconSize pixel value and a spread of densities (padding).
void non_stretch_matches_formula()
{
	const int sizes[] = { 1, 16, 24, 32, 48, 64, 96, 128 };
	const int pads[]  = { 0, 2, 4, 6 };   // high/medium/low density paddings
	for (int size : sizes)
	{
		for (int pad : pads)
		{
			GridCellWidth c = meow_grid_cell_width(pad, size, false);
			int m, n;
			expected(pad, size, false, &m, &n);
			CHECK(c.minimum == m);
			CHECK(c.natural == n);
			CHECK(c.minimum == c.natural);          // non-stretch: equal
			CHECK(c.minimum == (pad * 2) + size);   // label-independent
		}
	}
}

// Stretch cells: minimum is the size-tapered widened base; natural is 2*min-1.
void stretch_matches_formula()
{
	const int sizes[] = { 16, 24, 32, 48, 64, 96, 128 };
	const int pads[]  = { 0, 2, 4, 6 };
	for (int size : sizes)
	{
		for (int pad : pads)
		{
			GridCellWidth c = meow_grid_cell_width(pad, size, true);
			int m, n;
			expected(pad, size, true, &m, &n);
			CHECK(c.minimum == m);
			CHECK(c.natural == n);
			CHECK(c.natural == (c.minimum * 2) - 1);
		}
	}
}

// INV-6: the formula has no label-text input at all, so at equal padding + size
// + stretch the width is the same regardless of which mode produced the cell.
void label_independent()
{
	for (bool stretch : { false, true })
	{
		GridCellWidth apps   = meow_grid_cell_width(4, 48, stretch);
		GridCellWidth places = meow_grid_cell_width(4, 48, stretch);
		CHECK(apps.minimum == places.minimum);
		CHECK(apps.natural == places.natural);
	}
}

} // namespace

int main()
{
	non_stretch_matches_formula();
	stretch_matches_formula();
	label_independent();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_grid_cell_metrics: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_grid_cell_metrics: ok\n");
	return EXIT_SUCCESS;
}
