/*
 * Headless test for the shared Properties-dialog switch factory declared in
 * panel-plugin/ui/properties/common.h.
 *
 * It guards the alignment invariant that make_form_switch() must satisfy: a
 * GtkSwitch requesting halign = START / valign = CENTER, inactive on creation.
 * That invariant is the machine-checkable cause of compact (non-stretched)
 * rendering; the rendered pixel width of a themed widget cannot be asserted
 * headlessly and stays a manual cross-distro step.
 */

#include "ui/properties/common.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <cstdio>

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

} // namespace

int main()
{
	// Building a GtkSwitch creates its style context, which GTK 3 cannot do
	// without a display connection. Skip cleanly on a headless host (e.g. a CI
	// runner with no X server) instead of aborting; CI supplies a virtual
	// display so the alignment invariant below is still exercised there.
	if (!gtk_init_check(nullptr, nullptr))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77; // meson exitcode protocol: 77 marks the test skipped
	}

	GtkWidget* w = make_form_switch();

	CHECK(GTK_IS_SWITCH(w));
	CHECK(gtk_widget_get_halign(w) == GTK_ALIGN_START);
	CHECK(gtk_widget_get_valign(w) == GTK_ALIGN_CENTER);
	CHECK(gtk_switch_get_active(GTK_SWITCH(w)) == FALSE);

	// The factory returns a floating ref the caller owns; sink and drop it.
	g_object_ref_sink(w);
	g_object_unref(w);

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_properties_switch: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_properties_switch: ok\n");
	return EXIT_SUCCESS;
}
