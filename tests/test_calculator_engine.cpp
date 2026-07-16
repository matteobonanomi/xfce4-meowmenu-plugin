#include "calculator/calculator-engine.h"

#include <glib.h>

using namespace WhiskerMenu;

static void test_registry()
{
	g_assert_cmpint(static_cast<int>(calculator_engine_from_id("bc")), ==,
			static_cast<int>(CalculatorEngine::Bc));
	g_assert_cmpint(static_cast<int>(calculator_engine_from_id("unknown")), ==,
			static_cast<int>(CalculatorEngine::None));
	g_assert_cmpstr(calculator_engine_descriptor(CalculatorEngine::Qalculate).executable,
			==, "qalc");
	g_assert_cmpint(static_cast<int>(calculator_engine_descriptor(CalculatorEngine::None).input),
			==, static_cast<int>(CalculatorInput::Disabled));
	g_assert_cmpstr(calculator_engine_descriptor(CalculatorEngine::GnomeCalculator).fallback_icon_name,
			==, "accessories-calculator");
}

static void test_candidates()
{
	std::string expression;
	g_assert_true(calculator_query_is_candidate(CalculatorEngine::Bc, " 2 + 2 ", expression));
	g_assert_cmpstr(expression.c_str(), ==, "2 + 2");
	g_assert_true(calculator_query_is_candidate(CalculatorEngine::Bc, " = 42", expression));
	g_assert_cmpstr(expression.c_str(), ==, "42");
	g_assert_false(calculator_query_is_candidate(CalculatorEngine::Bc, "42", expression));
	g_assert_false(calculator_query_is_candidate(CalculatorEngine::Bc, "=", expression));
	g_assert_false(calculator_query_is_candidate(CalculatorEngine::None, "2+2", expression));
	g_assert_false(calculator_query_is_candidate(CalculatorEngine::Bc, "10 m to cm", expression));
	g_assert_true(calculator_query_is_candidate(CalculatorEngine::Qalculate, "10 m to cm", expression));
	g_assert_cmpstr(expression.c_str(), ==, "10 m to cm");

	const CalculatorQueryCandidate forced = calculator_classify_query(
			CalculatorEngine::Bc, "  ==2+2  ");
	g_assert_true(forced.forced);
	g_assert_cmpstr(forced.expression.c_str(), ==, "=2+2");
}

static void test_boundaries()
{
	g_assert_false(calculator_query_is_safe("a\nb"));
	g_assert_false(calculator_query_is_safe(std::string("a\0b", 3)));
	g_assert_false(calculator_query_is_safe(std::string(4097, '1')));
	g_assert_true(calculator_query_is_safe("sqrt(9)"));
}

static void test_adapters()
{
	const std::vector<std::string> bc = calculator_engine_argv(
			CalculatorEngine::Bc, "/tmp/bc", "2+2", 4);
	g_assert_cmpuint(bc.size(), ==, 3);
	g_assert_cmpstr(bc[0].c_str(), ==, "/tmp/bc");
	g_assert_cmpstr(bc[1].c_str(), ==, "-q");
	g_assert_cmpstr(bc[2].c_str(), ==, "-l");
	g_assert_cmpstr(calculator_engine_stdin(CalculatorEngine::Bc, "2+2", 4).c_str(),
			==, "scale=20\n2+2\n");

	const std::vector<std::string> qalc = calculator_engine_argv(
			CalculatorEngine::Qalculate, "/tmp/qalc", "2+2", 10);
	g_assert_cmpstr(qalc.front().c_str(), ==, "/tmp/qalc");
	g_assert_cmpstr(qalc[qalc.size() - 2].c_str(), ==, "--");
	g_assert_cmpstr(qalc.back().c_str(), ==, "2+2");
	g_assert_cmpstr(calculator_engine_stdin(CalculatorEngine::Qalculate, "2+2", 4).c_str(),
			==, "");
}

int main(int argc, char** argv)
{
	g_test_init(&argc, &argv, nullptr);
	g_test_add_func("/calculator/engine/registry", test_registry);
	g_test_add_func("/calculator/engine/candidates", test_candidates);
	g_test_add_func("/calculator/engine/boundaries", test_boundaries);
	g_test_add_func("/calculator/engine/adapters", test_adapters);
	return g_test_run();
}
