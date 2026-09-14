/*
 * Production-linked category hover arbitration and normal delivery coverage.
 */

#include "launcher/category-button.h"

#include <cassert>
#include <cstdio>

using namespace WhiskerMenu;

namespace
{

bool wait_for_active(GtkToggleButton* button, gint64 timeout_us)
{
	const gint64 deadline = g_get_monotonic_time() + timeout_us;
	while (!gtk_toggle_button_get_active(button)
			&& g_get_monotonic_time() < deadline)
	{
		g_main_context_iteration(nullptr, TRUE);
	}
	return gtk_toggle_button_get_active(button);
}

}

int main(int argc, char** argv)
{
	assert(!category_hover_should_schedule(false, false));
	assert(!category_hover_should_schedule(true, true));
	assert(category_hover_should_schedule(true, false));
	CategoryButton::suppress_hover_until_motion();
	assert(category_hover_is_suppressed());
	category_hover_note_motion();
	assert(!category_hover_is_suppressed());

	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	GtkWidget* widget = gtk_toggle_button_new();
	g_object_ref_sink(widget);
	gtk_widget_set_state_flags(widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
	schedule_category_hover(GTK_TOGGLE_BUTTON(widget));
	assert(wait_for_active(GTK_TOGGLE_BUTTON(widget), 500 * 1000));

	gtk_widget_destroy(widget);
	g_object_unref(widget);
	std::printf("test_category_activation: ok\n");
	return 0;
}
