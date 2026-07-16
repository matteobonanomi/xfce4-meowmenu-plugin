#include "search/calculator-result.h"

#include <glib.h>

using namespace WhiskerMenu;

namespace
{

void test_success_suppresses_fallbacks_only()
{
	CalculatorResult result;
	g_assert_false(result.suppresses_fallbacks());

	result.set_pending();
	g_assert_false(result.suppresses_fallbacks());

	result.set_missing_bc();
	g_assert_false(result.suppresses_fallbacks());

	result.set_result("bc", "accessories-calculator", "accessories-calculator",
			"4", -1);
	g_assert_true(result.suppresses_fallbacks());

	result.clear();
	g_assert_false(result.suppresses_fallbacks());
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
	g_test_add_func("/calculator/search-results/success-suppresses-fallbacks",
			test_success_suppresses_fallbacks_only);
	return g_test_run();
}
