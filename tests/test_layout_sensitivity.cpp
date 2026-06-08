/*
 * Headless test for the pure Layout-control sensitivity matrix declared in
 * core/user-session-layout.h. Asserts control_enabled() returns exactly the
 * FR-006 / data-model E-4 table for all 15 (control, mode) combinations.
 * No display, no Xfconf, no widgets.
 */

#include "core/user-session-layout.h"

#include <cstdio>
#include <cstdlib>

using WhiskerMenu::LayoutMode;
using WhiskerMenu::LayoutControl;
using WhiskerMenu::control_enabled;

namespace
{

int g_failures = 0;

#define CHECK_EQ(control, mode, expected) do { \
		bool got = control_enabled(control, mode); \
		if (got != (expected)) { \
			std::fprintf(stderr, "FAIL %s:%d: control_enabled(%s, %s) = %d, expected %d\n", \
				__FILE__, __LINE__, #control, #mode, got, (expected)); \
			++g_failures; \
		} \
	} while (0)

// The full 5x3 FR-006 matrix, transcribed cell by cell so a regression in any
// single (control, mode) pair is pinpointed.
void matrix_matches_fr006()
{
	// MenuWidth: on / on / off
	CHECK_EQ(LayoutControl::MenuWidth,  LayoutMode::Docked,     true);
	CHECK_EQ(LayoutControl::MenuWidth,  LayoutMode::Centered,   true);
	CHECK_EQ(LayoutControl::MenuWidth,  LayoutMode::FullScreen, false);

	// MenuHeight: on / on / off
	CHECK_EQ(LayoutControl::MenuHeight, LayoutMode::Docked,     true);
	CHECK_EQ(LayoutControl::MenuHeight, LayoutMode::Centered,   true);
	CHECK_EQ(LayoutControl::MenuHeight, LayoutMode::FullScreen, false);

	// PanelGap: on / off / off
	CHECK_EQ(LayoutControl::PanelGap,   LayoutMode::Docked,     true);
	CHECK_EQ(LayoutControl::PanelGap,   LayoutMode::Centered,   false);
	CHECK_EQ(LayoutControl::PanelGap,   LayoutMode::FullScreen, false);

	// CornerRadius: on / on / off
	CHECK_EQ(LayoutControl::CornerRadius, LayoutMode::Docked,     true);
	CHECK_EQ(LayoutControl::CornerRadius, LayoutMode::Centered,   true);
	CHECK_EQ(LayoutControl::CornerRadius, LayoutMode::FullScreen, false);

	// FullScreenOpacity: off / off / on
	CHECK_EQ(LayoutControl::FullScreenOpacity, LayoutMode::Docked,     false);
	CHECK_EQ(LayoutControl::FullScreenOpacity, LayoutMode::Centered,   false);
	CHECK_EQ(LayoutControl::FullScreenOpacity, LayoutMode::FullScreen, true);
}

} // namespace

int main()
{
	matrix_matches_fr006();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_layout_sensitivity: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_layout_sensitivity: ok\n");
	return EXIT_SUCCESS;
}
