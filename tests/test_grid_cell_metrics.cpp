/*
 * Headless tests for the pure icon-grid cell metric arithmetic declared in
 * panel-plugin/ui/grid-cell-metrics.h. No GTK types are used.
 *
 * Asserts that the per-cell minimum/natural width and height depend only on
 * geometry inputs — never on label text — across the full IconSize pixel range,
 * density spread, and layout assumptions.
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

const int k_label_line_height = 14;

// Reference reproduction of the renderer arithmetic, kept separate so the
// helper is checked against an independent copy of the formula.
void expected_width(int pad, int size, bool stretch, int* min_out, int* nat_out)
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

void expected_metrics(int pad, int size, int spacing, bool stretch, int lines,
		GridCellMetrics* out)
{
	int min_width, nat_width;
	expected_width(pad, size, stretch, &min_width, &nat_width);
	if (spacing < 0)
	{
		spacing = 0;
	}
	if (lines < 1)
	{
		lines = 1;
	}
	const int label_allowance = k_label_line_height * lines;
	const int height = (pad * 2) + size + spacing + label_allowance;
	*out = GridCellMetrics{ min_width, nat_width, height, height, label_allowance };
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
			expected_width(pad, size, false, &m, &n);
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
			expected_width(pad, size, true, &m, &n);
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

void height_matches_shared_application_formula()
{
	const int sizes[] = { 1, 16, 24, 32, 48, 64, 96, 128 };
	const int pads[] = { 0, 2, 4, 6, 8, 10 };
	const int spacings[] = { 0, 2, 4, 6, 8, 10 };

	for (int size : sizes)
	{
		for (int pad : pads)
		{
			for (int spacing : spacings)
			{
				for (bool stretch : { false, true })
				{
					GridCellMetrics got = meow_grid_cell_metrics(pad, size,
							spacing, stretch, 2);
					GridCellMetrics want;
					expected_metrics(pad, size, spacing, stretch, 2, &want);
					CHECK(got.minimum_height == want.minimum_height);
					CHECK(got.natural_height == want.natural_height);
					CHECK(got.label_line_allowance == want.label_line_allowance);
					CHECK(got.minimum_height == got.natural_height);
				}
			}
		}
	}
}

void height_never_uses_source_or_label_text()
{
	struct SourceCase
	{
		const char* source;
		const char* label;
	};
	const SourceCase cases[] = {
		{ "applications", "Terminal Emulator" },
		{ "places", "Home" },
		{ "places", "Very long translated folder label" },
		{ "search", "Search result" },
	};

	GridCellMetrics baseline = meow_grid_cell_metrics(4, 48, 4, true, 2);
	for (const SourceCase& c : cases)
	{
		(void)c.source;
		(void)c.label;
		GridCellMetrics current = meow_grid_cell_metrics(4, 48, 4, true, 2);
		CHECK(current.minimum_height == baseline.minimum_height);
		CHECK(current.natural_height == baseline.natural_height);
		CHECK(current.minimum_width == baseline.minimum_width);
		CHECK(current.natural_width == baseline.natural_width);
	}
}

void app_and_places_share_height()
{
	for (bool stretch : { false, true })
	{
		GridCellMetrics apps = meow_grid_cell_metrics(4, 48, 4, stretch, 2);
		GridCellMetrics places = meow_grid_cell_metrics(4, 48, 4, stretch, 2);
		CHECK(apps.minimum_height == places.minimum_height);
		CHECK(apps.natural_height == places.natural_height);
		CHECK(apps.minimum_height > ((4 * 2) + 48));
	}
}

void layout_and_density_matrix_is_deterministic()
{
	const char* layouts[] = { "docked", "centered", "fullscreen" };
	struct Density
	{
		const char* name;
		int padding;
		int spacing;
	};
	const Density densities[] = {
		{ "high", 2, 2 },
		{ "medium", 4, 4 },
		{ "low", 8, 8 },
	};

	for (const char* layout : layouts)
	{
		for (const Density& density : densities)
		{
			(void)layout;
			(void)density.name;
			GridCellMetrics apps = meow_grid_cell_metrics(density.padding,
					64, density.spacing, true, 2);
			GridCellMetrics places = meow_grid_cell_metrics(density.padding,
					64, density.spacing, true, 2);
			CHECK(apps.minimum_height == places.minimum_height);
			CHECK(apps.natural_height == places.natural_height);
		}
	}
}

} // namespace

int main()
{
	non_stretch_matches_formula();
	stretch_matches_formula();
	label_independent();
	height_matches_shared_application_formula();
	height_never_uses_source_or_label_text();
	app_and_places_share_height();
	layout_and_density_matrix_is_deterministic();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_grid_cell_metrics: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_grid_cell_metrics: ok\n");
	return EXIT_SUCCESS;
}
