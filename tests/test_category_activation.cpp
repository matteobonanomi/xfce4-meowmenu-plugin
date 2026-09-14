/*
 * Production-linked category activation and hover-source lifetime coverage.
 */

#include <cassert>
#include <cstdio>
#include "core/window-keyboard.h"
#include "core/plugin.h"
#include "core/window.h"
#include "launcher/applications-page.h"
#include "launcher/category-activation.h"
#include "launcher/category-button.h"
#include "private-xfconf-fixture.h"
#include "settings.h"

using namespace WhiskerMenu;

namespace
{

/* wait_for_active:
 * @button: borrowed button expected to activate.
 * @timeout_us: maximum wait in monotonic microseconds.
 *
 * Drives the default main context until activation or timeout.
 *
 * Returns: true when the button became active.
 */
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

/* drain_for:
 * @duration_us: minimum time to drive the default context.
 *
 * Allows a cancelled timeout's former delivery window to elapse.
 */
void drain_for(gint64 duration_us)
{
	const gint64 deadline = g_get_monotonic_time() + duration_us;
	while (g_get_monotonic_time() < deadline)
	{
		while (g_main_context_pending(nullptr))
			g_main_context_iteration(nullptr, FALSE);
		g_usleep(1000);
	}
}

/* emit_enter:
 * @widget: category button receiving a synthetic pointer entry.
 *
 * Drives the production signal connection without GTK pointer movement.
 */
void emit_enter(GtkWidget* widget)
{
	GdkEventCrossing event = {};
	event.type = GDK_ENTER_NOTIFY;
	gboolean handled = FALSE;
	g_signal_emit_by_name(widget, "enter-notify-event", &event, &handled);
}

/* emit_focus:
 * @widget: category button receiving a synthetic focus event.
 *
 * Drives the production focus-activation connection directly.
 */
void emit_focus(GtkWidget* widget)
{
	GdkEventFocus event = {};
	event.type = GDK_FOCUS_CHANGE;
	event.in = TRUE;
	gboolean handled = FALSE;
	g_signal_emit_by_name(widget, "focus-in-event", &event, &handled);
}

/* wait_for_publication:
 * @applications: production page whose initial graph must settle.
 *
 * Prevents unrelated asynchronous publication from sharing the timer window
 * used by the hover lifetime assertions.
 *
 * Returns: true when a complete publication became available.
 */
bool wait_for_publication(ApplicationsPage* applications)
{
	const gint64 deadline = g_get_monotonic_time() + (5 * G_USEC_PER_SEC);
	while (!applications->has_publication()
			&& g_get_monotonic_time() < deadline)
	{
		g_main_context_iteration(nullptr, TRUE);
	}
	return applications->has_publication();
}

}

static int run_test(int argc, char** argv)
{
	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	XfcePanelPlugin* host = XFCE_PANEL_PLUGIN(g_object_new(
			XFCE_TYPE_PANEL_PLUGIN,
			"name", "meowmenu",
			"display-name", "MeowMenu",
			"comment", "Category activation test host",
			"unique-id", 9006,
			nullptr));
	assert(host);
	Plugin* plugin = new Plugin(host);
	ApplicationsPage* applications = plugin->get_window()->get_applications();
	assert(wait_for_publication(applications));
	CategoryActivation& first = *applications->get_category_activation();
	CategoryActivation second;
	assert(!first.hover_should_schedule(false, false));
	assert(!first.hover_should_schedule(true, true));
	assert(first.hover_should_schedule(true, false));
	assert(first.keyboard_should_activate(true));
	assert(!first.keyboard_should_activate(false));
	first.note_keyboard_navigation();
	assert(first.hover_is_suppressed());
	assert(!first.hover_should_activate(true));
	assert(!first.focus_should_activate(true, false));
	assert(!second.hover_is_suppressed());
	assert(second.hover_should_activate(true));
	first.note_pointer_motion();
	assert(!first.hover_is_suppressed());
	assert(first.focus_should_activate(true, false));
	assert(CategoryActivation::hover_delay_ms() == 150);

	Keyboard::ActivationDebounce debounce;
	assert(debounce.accept(1000000));
	assert(!debounce.accept(1249999));
	assert(debounce.accept(1250000));

	Settings* settings = plugin->get_settings();
	settings->category_hover_activate = true;
	GIcon* icon = g_themed_icon_new("applications-other");
	CategoryButton* anchor = new CategoryButton(settings, &first, icon, "Anchor");

	CategoryButton* hover = new CategoryButton(settings, &first, icon, "Hover");
	hover->join_group(anchor);
	anchor->set_active(true);
	GtkWidget* hover_widget = hover->get_widget();
	gtk_widget_set_state_flags(hover_widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
	emit_enter(hover_widget);
	assert(!hover->get_active());
	assert(wait_for_active(GTK_TOGGLE_BUTTON(hover_widget), 500 * 1000));
	delete hover;

	CategoryButton* focus = new CategoryButton(settings, &first, icon, "Focus");
	focus->join_group(anchor);
	anchor->set_active(true);
	first.note_keyboard_navigation();
	emit_focus(focus->get_widget());
	assert(!focus->get_active());
	first.note_pointer_motion();
	emit_focus(focus->get_widget());
	assert(focus->get_active());
	delete focus;

	CategoryButton* pending = new CategoryButton(settings, &first, icon, "Pending");
	pending->join_group(anchor);
	anchor->set_active(true);
	gtk_widget_set_state_flags(pending->get_widget(), GTK_STATE_FLAG_PRELIGHT, FALSE);
	emit_enter(pending->get_widget());
	delete pending;
	drain_for(2 * CategoryActivation::hover_delay_ms() * 1000);

	CategoryButton* commit = new CategoryButton(settings, &first, icon, "Commit");
	commit->join_group(anchor);
	anchor->set_active(true);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(commit->get_widget()), true);
	assert(commit->get_active());
	delete commit;
	delete anchor;

	g_object_unref(icon);
	std::puts("test_category_activation: ok");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
