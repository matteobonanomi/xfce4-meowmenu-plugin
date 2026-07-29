#include "search/calculator-result.h"

#include <atk/atk.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>

using namespace WhiskerMenu;

namespace
{

GtkWidget* result_content(CalculatorResult& result)
{
	GtkWidget* clip = gtk_bin_get_child(GTK_BIN(result.get_focus_widget()));
	GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(clip));
	return gtk_bin_get_child(GTK_BIN(viewport));
}

GtkWidget* value_label(CalculatorResult& result)
{
	GtkWidget* content = result_content(result);
	GList* children = gtk_container_get_children(GTK_CONTAINER(content));
	GtkWidget* allocation = GTK_WIDGET(g_list_last(children)->data);
	g_list_free(children);
	if (GTK_IS_LABEL(allocation))
		return allocation;
	GList* overlays = gtk_container_get_children(GTK_CONTAINER(allocation));
	GtkWidget* label = GTK_WIDGET(g_list_last(overlays)->data);
	g_list_free(overlays);
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

GtkWidget* engine_label(CalculatorResult& result)
{
	GtkWidget* content = result_content(result);
	GList* children = gtk_container_get_children(GTK_CONTAINER(content));
	GtkWidget* engine = GTK_WIDGET(children->next->data);
	g_list_free(children);
	return engine;
}

void flush_events()
{
	while (g_main_context_pending(nullptr))
		g_main_context_iteration(nullptr, false);
}

void test_auto_preset_mapping()
{
	g_assert_cmpint(calculator_auto_font_size("minimal"), ==, 3);
	g_assert_cmpint(calculator_auto_font_size("modern"), ==, 4);
	g_assert_cmpint(calculator_auto_font_size("classic"), ==, 4);
	g_assert_cmpint(calculator_auto_font_size("fullscreen"), ==, 5);
	g_assert_cmpint(calculator_auto_font_size("saved-custom"), ==, 3);
	g_assert_cmpint(calculator_auto_font_size(""), ==, 3);
	g_assert_cmpint(calculator_auto_font_size(nullptr), ==, 3);
}

void test_grid_banner_stays_compact()
{
	g_assert_cmpint(calculator_result_height(37, false), ==, 37);
	g_assert_cmpint(calculator_result_height(64, false), ==, 64);
	g_assert_cmpint(calculator_result_height(192, true), ==, 192);
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
			result.set_presentation_metrics(37, 24, false, 5);
			result.set_result("Qalculate", "qalculate", "accessories-calculator",
					"12345678901234567890.1234567890 m/s", size);
			GtkWidget* label = value_label(result);
			g_assert_cmpint(gtk_label_get_ellipsize(GTK_LABEL(label)), ==,
					PANGO_ELLIPSIZE_MIDDLE);
			g_assert_cmpint(gtk_label_get_lines(GTK_LABEL(label)), ==, 1);
			g_assert_nonnull(gtk_label_get_attributes(GTK_LABEL(label)));
			const double expected[] = {
				PANGO_SCALE_XX_SMALL, PANGO_SCALE_X_SMALL, PANGO_SCALE_SMALL,
				PANGO_SCALE_MEDIUM, PANGO_SCALE_LARGE, PANGO_SCALE_X_LARGE,
				PANGO_SCALE_XX_LARGE
			};
			g_assert_cmpfloat_with_epsilon(label_scale(label),
					expected[size < 0 ? 5 : size], 0.0001);
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
		g_assert_false(gtk_widget_compute_expand(result.get_widget(),
				GTK_ORIENTATION_VERTICAL));
		int minimum = 0;
		int natural = 0;
		gtk_widget_get_preferred_height(result.get_widget(), &minimum, &natural);
		g_assert_cmpint(minimum, ==, 37);
		g_assert_cmpint(natural, ==, 37);

		result.set_presentation_metrics(192, 48, true);
		gtk_widget_get_preferred_height(result.get_widget(), &minimum, &natural);
		g_assert_cmpint(minimum, ==, 192);
		g_assert_cmpint(natural, ==, 192);
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
		result.set_presentation_metrics(37, 24, false, 4);
		result.set_result("bc", "accessories-calculator", "accessories-calculator",
				"4", -1);
		g_assert_cmpfloat_with_epsilon(label_scale(value_label(result)),
				PANGO_SCALE_LARGE, 0.0001);

		gtk_box_pack_start(GTK_BOX(container), result.get_widget(), false, false, 0);
		gtk_box_pack_start(GTK_BOX(container), gtk_label_new(nullptr), true, true, 0);
		gtk_widget_show_all(window);
		flush_events();

		GtkWidget* value = value_label(result);
		GtkWidget* engine = engine_label(result);
		g_assert_cmpint(gtk_widget_get_allocated_width(value), >, 1);
		g_assert_nonnull(gtk_label_get_attributes(GTK_LABEL(value)));
		g_assert_cmpfloat_with_epsilon(label_scale(value), PANGO_SCALE_LARGE, 0.0001);
		g_assert_cmpfloat_with_epsilon(label_scale(engine),
				PANGO_SCALE_LARGE * 0.85, 0.0001);
		int banner_minimum = 0;
		int banner_natural = 0;
		gtk_widget_get_preferred_height(result.get_widget(), &banner_minimum, &banner_natural);
		g_assert_cmpint(banner_minimum, ==, 37);
		g_assert_cmpint(banner_natural, ==, 37);
		g_assert_cmpint(gtk_widget_get_allocated_height(result.get_widget()), ==, 37);
	}

	gtk_widget_destroy(window);
	g_object_unref(window);
	flush_events();
}

void test_missing_guidance_has_no_auto_emphasis()
{
	GtkWidget* widget = nullptr;
	{
		CalculatorResult result;
		widget = result.get_widget();
		g_object_ref_sink(widget);
		result.set_presentation_metrics(37, 24, false, 5);
		result.set_missing_bc();
		g_assert_null(gtk_label_get_attributes(GTK_LABEL(value_label(result))));
		g_assert_null(gtk_label_get_attributes(GTK_LABEL(engine_label(result))));
	}
	gtk_widget_destroy(widget);
	g_object_unref(widget);
}

void test_auto_restyles_existing_result()
{
	GtkWidget* widget = nullptr;
	{
		CalculatorResult result;
		widget = result.get_widget();
		g_object_ref_sink(widget);
		result.set_presentation_metrics(37, 24, false, 3);
		result.set_result("bc", "accessories-calculator", "accessories-calculator",
				"4", -1);
		g_assert_cmpfloat_with_epsilon(label_scale(value_label(result)),
				PANGO_SCALE_MEDIUM, 0.0001);
		result.set_presentation_metrics(37, 24, false, 5);
		g_assert_cmpfloat_with_epsilon(label_scale(value_label(result)),
				PANGO_SCALE_X_LARGE, 0.0001);
	}
	gtk_widget_destroy(widget);
	g_object_unref(widget);
}

}

int main(int argc, char** argv)
{
	test_auto_preset_mapping();
	test_grid_banner_stays_compact();
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
	g_test_add_func("/calculator/result/missing-guidance", test_missing_guidance_has_no_auto_emphasis);
	g_test_add_func("/calculator/result/auto-restyle", test_auto_restyles_existing_result);
	const int status = g_test_run();
	// Release Pango's process-global Fontconfig map after all GTK test objects.
	PangoFontMap* font_map = pango_cairo_font_map_get_default();
	pango_fc_font_map_shutdown(PANGO_FC_FONT_MAP(font_map));
	return status;
}
