#include "search/calculator-result.h"

#include <atk/atk.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>

using namespace WhiskerMenu;

namespace
{

GtkWidget* value_label(CalculatorResult& result)
{
	GtkWidget* content = gtk_bin_get_child(GTK_BIN(result.get_focus_widget()));
	GList* children = gtk_container_get_children(GTK_CONTAINER(content));
	GtkWidget* label = GTK_WIDGET(g_list_last(children)->data);
	g_list_free(children);
	return label;
}

double label_scale(GtkWidget* label)
{
	PangoAttrList* attrs = gtk_label_get_attributes(GTK_LABEL(label));
	if (!attrs)
		return 1.0;
	PangoAttrIterator* iterator = pango_attr_list_get_iterator(attrs);
	PangoAttribute* attr = pango_attr_iterator_get(iterator, PANGO_ATTR_SCALE);
	const double scale = attr ? reinterpret_cast<PangoAttrFloat*>(attr)->value : 1.0;
	pango_attr_iterator_destroy(iterator);
	return scale;
}

void flush_events()
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, false);
}

void test_states_and_activation()
{
	GtkWidget* widget = nullptr;
	{
		CalculatorResult result;
		widget = result.get_widget();
		g_object_ref_sink(widget);
		g_assert_cmpint(static_cast<int>(result.state()), ==,
				static_cast<int>(CalculatorResultState::Hidden));
		g_assert_false(result.is_visible());

		result.set_pending();
		g_assert_cmpint(static_cast<int>(result.state()), ==,
				static_cast<int>(CalculatorResultState::Pending));
		g_assert_false(result.is_visible());

		int activations = 0;
		result.set_activate_callback([&]() { ++activations; });
		result.set_result("bc", "accessories-calculator", "accessories-calculator",
				"1234567890.1234", -1);
		g_assert_true(result.is_visible());
		g_assert_true(result.is_activatable());
		g_assert_cmpstr(result.value().c_str(), ==, "1234567890.1234");
		gchar* tooltip = gtk_widget_get_tooltip_text(result.get_widget());
		g_assert_cmpstr(tooltip, ==, "1234567890.1234");
		g_free(tooltip);
		AtkObject* accessible = gtk_widget_get_accessible(result.get_focus_widget());
		g_assert_cmpstr(atk_object_get_name(accessible), ==, "bc: 1234567890.1234");
		g_assert_cmpstr(atk_object_get_description(accessible), ==, "1234567890.1234");
		result.activate();
		g_assert_cmpint(activations, ==, 1);

		result.set_missing_bc();
		g_assert_true(result.is_visible());
		g_assert_false(result.is_activatable());
		g_assert_cmpstr(result.value().c_str(), ==, "");
		result.activate();
		g_assert_cmpint(activations, ==, 1);

		result.clear();
		g_assert_false(result.is_visible());
	}
	gtk_widget_destroy(widget);
	g_object_unref(widget);
}

void test_one_line_typography()
{
	GtkWidget* widget = nullptr;
	{
		CalculatorResult result;
		widget = result.get_widget();
		g_object_ref_sink(widget);
		for (int size = -1; size <= 6; ++size)
		{
			result.set_result("Qalculate", "qalculate", "accessories-calculator",
					"12345678901234567890.1234567890 m/s", size);
			GtkWidget* label = value_label(result);
			g_assert_cmpint(gtk_label_get_ellipsize(GTK_LABEL(label)), ==,
					PANGO_ELLIPSIZE_MIDDLE);
			g_assert_cmpint(gtk_label_get_lines(GTK_LABEL(label)), ==, 1);
			if (size < 0)
				g_assert_null(gtk_label_get_attributes(GTK_LABEL(label)));
			else
				g_assert_nonnull(gtk_label_get_attributes(GTK_LABEL(label)));
		}
	}
	gtk_widget_destroy(widget);
	g_object_unref(widget);
}

void test_banner_ordering()
{
	GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	g_object_ref_sink(container);
	{
		CalculatorResult result;
		GtkWidget* message = gtk_label_new("No applications found");
		GtkWidget* ordinary = gtk_tree_view_new();
		gtk_box_pack_start(GTK_BOX(container), result.get_widget(), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(container), message, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(container), ordinary, TRUE, TRUE, 0);
		GList* children = gtk_container_get_children(GTK_CONTAINER(container));
		g_assert_true(children->data == result.get_widget());
		g_assert_true(children->next->data == message);
		g_assert_true(children->next->next->data == ordinary);
		g_list_free(children);
	}
	gtk_widget_destroy(container);
	g_object_unref(container);
}

void test_presentation_metrics()
{
	GtkWidget* widget = nullptr;
	{
		CalculatorResult result;
		widget = result.get_widget();
		g_object_ref_sink(widget);
		result.set_result("bc", "accessories-calculator", "accessories-calculator",
				"4", -1);
		result.set_presentation_metrics(37, 24, false);
		int minimum = 0;
		int natural = 0;
		gtk_widget_get_preferred_height(result.get_widget(), &minimum, &natural);
		g_assert_cmpint(minimum, ==, 37);
		g_assert_cmpint(natural, ==, 37);

		result.set_presentation_metrics(64, 48, true);
		gtk_widget_get_preferred_height(result.get_widget(), &minimum, &natural);
		g_assert_cmpint(minimum, ==, 64);
		g_assert_cmpint(natural, ==, 64);
	}
	gtk_widget_destroy(widget);
	g_object_unref(widget);
}

void test_auto_typography_waits_for_banner_allocation()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_object_ref_sink(window);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 80);
	GtkWidget* container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), container);
	{
		CalculatorResult result;
		result.set_presentation_metrics(37, 24, false);
		result.set_result("bc", "accessories-calculator", "accessories-calculator",
				"4", -1);
		g_assert_null(gtk_label_get_attributes(GTK_LABEL(value_label(result))));

		gtk_box_pack_start(GTK_BOX(container), result.get_widget(), false, false, 0);
		gtk_box_pack_start(GTK_BOX(container), gtk_label_new(nullptr), true, true, 0);
		gtk_widget_show_all(window);
		flush_events();

		GtkWidget* value = value_label(result);
		GtkWidget* content = gtk_bin_get_child(GTK_BIN(result.get_focus_widget()));
		GList* children = gtk_container_get_children(GTK_CONTAINER(content));
		GtkWidget* engine = GTK_WIDGET(children->next->data);
		g_list_free(children);
		g_assert_cmpint(gtk_widget_get_allocated_width(value), >, 1);
		g_assert_nonnull(gtk_label_get_attributes(GTK_LABEL(value)));
		g_assert_cmpfloat(label_scale(value), >=, 1.0);
		g_assert_cmpfloat(label_scale(engine), <, label_scale(value));
		int banner_minimum = 0;
		int banner_natural = 0;
		gtk_widget_get_preferred_height(result.get_widget(), &banner_minimum, &banner_natural);
		g_assert_cmpint(banner_minimum, ==, 37);
		g_assert_cmpint(banner_natural, ==, 37);
	}

	gtk_widget_destroy(window);
	g_object_unref(window);
	flush_events();
}

}

int main(int argc, char** argv)
{
	if (!gtk_init_check(&argc, &argv))
	{
		g_test_message("SKIP: GTK display unavailable");
		return 0;
	}
	g_test_init(&argc, &argv, nullptr);
	g_test_add_func("/calculator/result/states-activation", test_states_and_activation);
	g_test_add_func("/calculator/result/typography", test_one_line_typography);
	g_test_add_func("/calculator/result/ordering", test_banner_ordering);
	g_test_add_func("/calculator/result/presentation-metrics", test_presentation_metrics);
	g_test_add_func("/calculator/result/auto-typography-allocation",
			test_auto_typography_waits_for_banner_allocation);
	const int status = g_test_run();
	// Release Pango's process-global Fontconfig map after all GTK test objects.
	PangoFontMap* font_map = pango_cairo_font_map_get_default();
	pango_fc_font_map_shutdown(PANGO_FC_FONT_MAP(font_map));
	return status;
}
