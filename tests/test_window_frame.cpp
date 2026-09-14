/*
 * Tests for the window-frame helpers declared in
 * panel-plugin/core/window-frame.h. Pure decisions remain covered alongside
 * display-backed mapped-result lifetime behavior.
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
using meow::MappedResultFrame;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

struct PreparationState
{
	int calls;
	int ready_after;
};

/* prepare_result:
 * @data: PreparationState that selects the successful readiness attempt.
 *
 * Records each mapped-frame preparation and reports ready at the configured
 * attempt so completion and bounded retry behavior can be exercised.
 *
 * Returns: true once the configured attempt has been reached.
 */
bool prepare_result(void* data)
{
	PreparationState* state = static_cast<PreparationState*>(data);
	++state->calls;
	return state->calls >= state->ready_after;
}

void drain_events()
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, FALSE);
}

/* pump_frame:
 * @window: mapped window whose frame clock owns the callback.
 *
 * Advances one test frame and drains callbacks queued by its draw transaction.
 */
void pump_frame(GtkWidget* window)
{
	gtk_test_widget_wait_for_draw(window);
	drain_events();
}

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

/* mapped_frame_completes_and_coalesces:
 *
 * Verifies that repeated scheduling preserves the first transaction, polls
 * until readiness, and clears the pending state on completion.
 */
void mapped_frame_completes_and_coalesces()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* result = gtk_label_new("result");
	gtk_container_add(GTK_CONTAINER(window), result);
	gtk_widget_show_all(window);
	drain_events();

	PreparationState first = { 0, 3 };
	PreparationState ignored = { 0, 1 };
	MappedResultFrame frame;
	CHECK(frame.schedule(window, window, result, prepare_result, &first));
	CHECK(frame.schedule(window, window, result, prepare_result, &ignored));
	CHECK(frame.pending());
	for (int i = 0; frame.pending() && i < 12; ++i)
		pump_frame(window);
	CHECK(!frame.pending());
	CHECK(first.calls == 3);
	CHECK(ignored.calls == 0);

	gtk_widget_destroy(window);
	g_object_unref(window);
}

/* mapped_frame_stops_at_bounded_retry_count:
 *
 * Pins the eight-frame readiness bound so a permanently unready view cannot
 * retain widgets or preparation data indefinitely.
 */
void mapped_frame_stops_at_bounded_retry_count()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* result = gtk_label_new("result");
	gtk_container_add(GTK_CONTAINER(window), result);
	gtk_widget_show_all(window);
	drain_events();

	PreparationState state = { 0, 100 };
	MappedResultFrame frame;
	CHECK(frame.schedule(window, window, result, prepare_result, &state));
	for (int i = 0; frame.pending() && i < 12; ++i)
		pump_frame(window);
	CHECK(!frame.pending());
	CHECK(state.calls == 8);

	gtk_widget_destroy(window);
	g_object_unref(window);
}

/* mapped_frame_cancels_with_dependencies:
 *
 * Destroys the frame owner and result separately while work is pending. Both
 * paths must clear the callback before borrowed preparation data is invoked.
 */
void mapped_frame_cancels_with_dependencies()
{
	PreparationState owner_state = { 0, 100 };
	MappedResultFrame owner_frame;
	GtkWidget* owner = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(owner);
	GtkWidget* owner_result = gtk_label_new("owner result");
	gtk_container_add(GTK_CONTAINER(owner), owner_result);
	gtk_widget_show_all(owner);
	CHECK(owner_frame.schedule(owner, owner, owner_result,
			prepare_result, &owner_state));
	gtk_widget_destroy(owner);
	CHECK(!owner_frame.pending());
	CHECK(owner_state.calls == 0);
	g_object_unref(owner);

	PreparationState result_state = { 0, 100 };
	MappedResultFrame result_frame;
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* result = gtk_label_new("result");
	gtk_container_add(GTK_CONTAINER(window), result);
	gtk_widget_show_all(window);
	CHECK(result_frame.schedule(window, window, result,
			prepare_result, &result_state));
	gtk_widget_destroy(result);
	CHECK(!result_frame.pending());
	pump_frame(window);
	CHECK(result_state.calls == 0);
	gtk_widget_destroy(window);
	g_object_unref(window);
}

/* mapped_frame_releases_replaced_view_data:
 *
 * Cancels a pending old-view transaction before replacement, then proves the
 * same owner can schedule and complete using only the new preparation data.
 */
void mapped_frame_releases_replaced_view_data()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* old_result = gtk_label_new("old");
	gtk_container_add(GTK_CONTAINER(window), box);
	gtk_container_add(GTK_CONTAINER(box), old_result);
	gtk_widget_show_all(window);

	PreparationState old_state = { 0, 100 };
	PreparationState new_state = { 0, 1 };
	MappedResultFrame frame;
	CHECK(frame.schedule(window, window, old_result,
			prepare_result, &old_state));
	frame.cancel();
	CHECK(!frame.pending());
	gtk_widget_destroy(old_result);
	GtkWidget* new_result = gtk_label_new("new");
	gtk_container_add(GTK_CONTAINER(box), new_result);
	gtk_widget_show(new_result);
	CHECK(frame.schedule(window, window, new_result,
			prepare_result, &new_state));
	for (int i = 0; frame.pending() && i < 4; ++i)
		pump_frame(window);
	CHECK(!frame.pending());
	CHECK(old_state.calls == 0);
	CHECK(new_state.calls == 1);

	gtk_widget_destroy(window);
	g_object_unref(window);
}

} // namespace

int main(int argc, char** argv)
{
	radius_clamped_to_range();
	radius_clamp_is_monotonic();
	border_predicate_truth_table();
	frameless_launcher_css_targets_scrollbar_chrome();
	list_selection_uses_theme_tokens();

	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: mapped-frame lifecycle requires a display\n");
		return 77;
	}
	mapped_frame_completes_and_coalesces();
	mapped_frame_stops_at_bounded_retry_count();
	mapped_frame_cancels_with_dependencies();
	mapped_frame_releases_replaced_view_data();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_window_frame: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_window_frame: ok\n");
	return EXIT_SUCCESS;
}
