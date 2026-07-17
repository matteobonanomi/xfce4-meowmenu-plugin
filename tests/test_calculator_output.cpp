#include "calculator/calculator-output.h"

#include <glib.h>

using namespace WhiskerMenu;

static void test_normalize()
{
	std::string value;
	g_assert_true(calculator_normalize_output("12.34560", 4, value));
	g_assert_cmpstr(value.c_str(), ==, "12.3456");
	g_assert_true(calculator_normalize_output("4\n", 4, value));
	g_assert_cmpstr(value.c_str(), ==, "4");
	g_assert_true(calculator_normalize_output("9.999", 2, value));
	g_assert_cmpstr(value.c_str(), ==, "10");
	g_assert_true(calculator_normalize_output("-0.04", 0, value));
	g_assert_cmpstr(value.c_str(), ==, "0");
	g_assert_true(calculator_normalize_output("1.2 m/s", 4, value));
	g_assert_cmpstr(value.c_str(), ==, "1.2 m/s");
	g_assert_true(calculator_normalize_output(".12500", 4, value));
	g_assert_cmpstr(value.c_str(), ==, "0.125");
	g_assert_true(calculator_normalize_output("1,2500", 4, value));
	g_assert_cmpstr(value.c_str(), ==, "1,25");
}

static void test_rejections()
{
	std::string value;
	g_assert_false(calculator_normalize_output("", 4, value));
	g_assert_false(calculator_normalize_output("1\n2", 4, value));
	g_assert_false(calculator_normalize_output("not a number", 4, value));
	g_assert_false(calculator_normalize_output("1.0", 11, value));
	g_assert_false(calculator_normalize_output(std::string("1\0", 2), 4, value));
	g_assert_false(calculator_normalize_output("NaN", 4, value));
	g_assert_false(calculator_normalize_output("inf", 4, value));
	g_assert_false(calculator_normalize_output("1+2", 4, value));
}

static void test_precision_boundaries()
{
	std::string value;
	g_assert_true(calculator_normalize_output("1.2345", 3, value));
	g_assert_cmpstr(value.c_str(), ==, "1.235");
	g_assert_true(calculator_normalize_output("-1.2345", 3, value));
	g_assert_cmpstr(value.c_str(), ==, "-1.235");
	g_assert_true(calculator_normalize_output("99.5", 0, value));
	g_assert_cmpstr(value.c_str(), ==, "100");
	g_assert_true(calculator_normalize_output("-.005", 2, value));
	g_assert_cmpstr(value.c_str(), ==, "-0.01");
	g_assert_true(calculator_normalize_output("-0.0001", 2, value));
	g_assert_cmpstr(value.c_str(), ==, "0");
	g_assert_true(calculator_normalize_output("123456789012345678901234567890.1", 10, value));
	g_assert_cmpstr(value.c_str(), ==, "123456789012345678901234567890.1");
}

static void test_engine_forms()
{
	std::string value;
	g_assert_true(calculator_normalize_output("1.23456e+10", 2, value));
	g_assert_cmpstr(value.c_str(), ==, "1.23e+10");
	g_assert_true(calculator_normalize_output("1.23456\xC3\x97" "10^6", 2, value));
	g_assert_cmpstr(value.c_str(), ==, "1.23\xC3\x97" "10^6");
	g_assert_true(calculator_normalize_output("\xE2\x89\x88 1.2345 m", 3, value));
	g_assert_cmpstr(value.c_str(), ==, "\xE2\x89\x88 1.235 m");
	g_assert_true(calculator_normalize_output("3.141592654", 10, value));
	g_assert_cmpstr(value.c_str(), ==, "3.141592654");
}

int main(int argc, char** argv)
{
	g_test_init(&argc, &argv, nullptr);
	g_test_add_func("/calculator/output/normalize", test_normalize);
	g_test_add_func("/calculator/output/rejections", test_rejections);
	g_test_add_func("/calculator/output/precision-boundaries", test_precision_boundaries);
	g_test_add_func("/calculator/output/engine-forms", test_engine_forms);
	return g_test_run();
}
