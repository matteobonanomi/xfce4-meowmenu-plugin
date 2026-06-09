/*
 * Headless tests for the pure decision logic behind Centered layout mode:
 *   - the /layout-mode classifier (core/user-session-layout.h), and
 *   - the centred-origin geometry helper (core/window-size-clamp.h).
 *
 * No display is required: GdkRectangle is a plain integer struct and neither
 * helper touches a widget or Xfconf. Covers the classifier contract (C-1,
 * FR-013), exact-centre placement (SC-001), oversize clamp-and-centre
 * (FR-012), and the no-drift invariant across a resize (SC-003).
 */

#include "core/user-session-layout.h"
#include "core/window-size-clamp.h"

#include <gdk/gdk.h>

#include <cstdio>
#include <cstdlib>

using WhiskerMenu::LayoutMode;
using WhiskerMenu::layout_mode_from_key;
using meow::centered_origin;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// The centre drift the X11 placement guarantee allows (SC-001/SC-003). Integer
// halving can shift the computed centre by at most 1px per axis.
const int kTolerance = 2;

// Centre of the actually-placed window: the helper clamps the size to the
// monitor, so the rendered extent (and thus its centre) uses the clamped size.
int win_center_x(const GdkRectangle& m, int w)
{
	int x, y; centered_origin(m, w, w, &x, &y);
	int cw = w < m.width ? w : m.width;
	return x + cw / 2;
}
int win_center_y(const GdkRectangle& m, int h)
{
	int x, y; centered_origin(m, h, h, &x, &y);
	int ch = h < m.height ? h : m.height;
	return y + ch / 2;
}

// Classifier: each known value maps to its mode; everything else → Docked.
void classifier_known_values()
{
	CHECK(layout_mode_from_key("docked")     == LayoutMode::Docked);
	CHECK(layout_mode_from_key("centered")   == LayoutMode::Centered);
	CHECK(layout_mode_from_key("fullscreen") == LayoutMode::FullScreen);
}

// C-1 / FR-013: unknown, empty, and NULL values all classify as Docked and the
// classifier never mutates anything (it is a pure read-time mapping).
void classifier_unknown_is_docked()
{
	CHECK(layout_mode_from_key("CENTERED")    == LayoutMode::Docked); // case-sensitive
	CHECK(layout_mode_from_key("floating")    == LayoutMode::Docked);
	CHECK(layout_mode_from_key("")            == LayoutMode::Docked);
	CHECK(layout_mode_from_key(nullptr)       == LayoutMode::Docked);
}

// SC-001: on a monitor whose origin is NOT (0,0), the window centre coincides
// with the monitor centre within tolerance — i.e. the origin offset is honored.
void centered_on_offset_monitor()
{
	GdkRectangle m = { 1920, 0, 2560, 1440 }; // second monitor, right of the first
	const int monitor_cx = m.x + m.width / 2;
	const int monitor_cy = m.y + m.height / 2;

	int x = 0, y = 0;
	centered_origin(m, 800, 600, &x, &y);
	CHECK(abs((x + 800 / 2) - monitor_cx) <= kTolerance);
	CHECK(abs((y + 600 / 2) - monitor_cy) <= kTolerance);

	// Stays within the monitor bounds.
	CHECK(x >= m.x);
	CHECK(y >= m.y);
	CHECK(x + 800 <= m.x + m.width);
	CHECK(y + 600 <= m.y + m.height);
}

// FR-012: a window larger than the monitor is clamped to fit and stays centred
// and fully on-screen (no negative origin, no overflow past the far edge).
void oversize_clamps_and_stays_on_screen()
{
	GdkRectangle m = { 100, 50, 1280, 720 };
	int x = 0, y = 0;
	centered_origin(m, 4000, 3000, &x, &y);

	CHECK(x >= m.x);
	CHECK(y >= m.y);
	CHECK(x <= m.x + m.width);   // clamped width == monitor.width → x == m.x
	CHECK(y <= m.y + m.height);
	// Clamped to exactly the monitor: top-left lands on the monitor origin.
	CHECK(x == m.x);
	CHECK(y == m.y);
}

// SC-003: the centre is a pure function of size + monitor. Sweeping the size up
// and down (as an interactive resize would) never moves the centre off the
// monitor centre — there is no accumulation of drag deltas, hence no drift.
void no_drift_across_resize()
{
	GdkRectangle m = { 1920, 0, 2560, 1440 };
	const int monitor_cx = m.x + m.width / 2;
	const int monitor_cy = m.y + m.height / 2;

	const int sizes[] = { 300, 500, 800, 1200, 800, 400, 200, 1500, 600 };
	for (int s : sizes)
	{
		CHECK(abs(win_center_x(m, s) - monitor_cx) <= kTolerance);
		CHECK(abs(win_center_y(m, s) - monitor_cy) <= kTolerance);
	}
}

} // namespace

int main()
{
	classifier_known_values();
	classifier_unknown_is_docked();
	centered_on_offset_monitor();
	oversize_clamps_and_stays_on_screen();
	no_drift_across_resize();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_centered_geometry: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_centered_geometry: ok\n");
	return EXIT_SUCCESS;
}
