#include "calculator/calculator-evaluator.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <functional>
#include <string>
#include <unistd.h>
#include <vector>

using namespace WhiskerMenu;

#ifndef MEOWMENU_CALCULATOR_STUB_PATH
#error "MEOWMENU_CALCULATOR_STUB_PATH must be defined"
#endif

namespace
{

std::string g_stub_dir;
std::string g_old_path;

bool wait_until(const std::function<bool()>& predicate, int timeout_ms)
{
	const gint64 deadline = g_get_monotonic_time() + timeout_ms * 1000;
	while (!predicate() && g_get_monotonic_time() < deadline)
	{
		while (g_main_context_iteration(nullptr, FALSE))
		{
		}
		g_usleep(1000);
	}
	while (g_main_context_iteration(nullptr, FALSE))
	{
	}
	return predicate();
}

void set_mode(const char* mode, const char* output = "4\n")
{
	g_setenv("MEOWMENU_CALCULATOR_STUB_MODE", mode, TRUE);
	g_setenv("MEOWMENU_CALCULATOR_STUB_OUTPUT", output, TRUE);
}

CalculatorEvaluation evaluate_and_wait(CalculatorEvaluator& evaluator,
		CalculatorEngine engine, const std::string& expression,
		int decimals = 4, unsigned int generation = 1)
{
	bool done = false;
	CalculatorEvaluation evaluation = { CalculatorEvaluationState::Failed,
			CalculatorEngine::None, std::string(), std::string(), 0, 0 };
	evaluator.evaluate(engine, expression, decimals, generation,
			[&](const CalculatorEvaluation& result)
			{
				evaluation = result;
				done = true;
			});
	g_assert_true(wait_until([&]() { return done; }, 5000));
	return evaluation;
}

void test_success_and_bc_transport()
{
	set_mode("success", "4\n");
	CalculatorEvaluator evaluator;
	const CalculatorEvaluation result = evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2", 4, 7);
	g_assert_cmpint(static_cast<int>(result.state), ==,
			static_cast<int>(CalculatorEvaluationState::Success));
	g_assert_cmpstr(result.value.c_str(), ==, "4");
	g_assert_cmpstr(result.expression.c_str(), ==, "2+2");
	g_assert_cmpint(result.maximum_decimals, ==, 4);
	g_assert_cmpuint(result.generation, ==, 7);
}

void test_direct_no_shell_transport()
{
	const std::string sentinel = g_stub_dir + G_DIR_SEPARATOR_S + "shell-ran";
	g_remove(sentinel.c_str());
	set_mode("success", "4\n");
	CalculatorEvaluator evaluator;
	const CalculatorEvaluation result = evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2; touch " + sentinel);
	g_assert_cmpint(static_cast<int>(result.state), ==,
			static_cast<int>(CalculatorEvaluationState::Success));
	g_assert_false(g_file_test(sentinel.c_str(), G_FILE_TEST_EXISTS));
}

void test_failures()
{
	CalculatorEvaluator evaluator;
	set_mode("failure");
	g_assert_cmpint(static_cast<int>(evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2").state), ==,
			static_cast<int>(CalculatorEvaluationState::Failed));
	set_mode("stderr");
	g_assert_cmpint(static_cast<int>(evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2").state), ==,
			static_cast<int>(CalculatorEvaluationState::Failed));
	set_mode("success", "not numeric\n");
	g_assert_cmpint(static_cast<int>(evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2").state), ==,
			static_cast<int>(CalculatorEvaluationState::Failed));
}

void test_output_overflow()
{
	set_mode("overflow");
	CalculatorEvaluator evaluator;
	const CalculatorEvaluation result = evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2");
	g_assert_cmpint(static_cast<int>(result.state), ==,
			static_cast<int>(CalculatorEvaluationState::Failed));
}

void test_timeout()
{
	set_mode("delay");
	CalculatorEvaluator evaluator;
	const gint64 start = g_get_monotonic_time();
	const CalculatorEvaluation result = evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2");
	const gint64 elapsed_ms = (g_get_monotonic_time() - start) / 1000;
	g_assert_cmpint(static_cast<int>(result.state), ==,
			static_cast<int>(CalculatorEvaluationState::TimedOut));
	g_assert_cmpint(elapsed_ms, >=, 1800);
	g_assert_cmpint(elapsed_ms, <, 3500);
}

void test_replacement_cancels_previous()
{
	std::vector<CalculatorEvaluation> results;
	CalculatorEvaluator evaluator;
	set_mode("delay");
	evaluator.evaluate(CalculatorEngine::Bc, "1+1", 4, 1,
			[&](const CalculatorEvaluation& result) { results.push_back(result); });
	set_mode("success", "4\n");
	evaluator.evaluate(CalculatorEngine::Bc, "2+2", 4, 2,
			[&](const CalculatorEvaluation& result) { results.push_back(result); });
	g_assert_true(wait_until([&]() { return results.size() == 2; }, 5000));
	bool saw_cancelled = false;
	bool saw_current = false;
	for (const auto& result : results)
	{
		if (result.generation == 1)
			saw_cancelled = result.state == CalculatorEvaluationState::Cancelled;
		if (result.generation == 2)
			saw_current = result.state == CalculatorEvaluationState::Success
					&& result.value == "4";
	}
	g_assert_true(saw_cancelled);
	g_assert_true(saw_current);
}

void test_teardown_silences_callback()
{
	set_mode("delay");
	bool called = false;
	auto* evaluator = new CalculatorEvaluator();
	evaluator->evaluate(CalculatorEngine::Bc, "2+2", 4, 1,
			[&](const CalculatorEvaluation&) { called = true; });
	delete evaluator;
	wait_until([]() { return false; }, 300);
	g_assert_false(called);
}

void test_unavailable_and_invalid_input()
{
	CalculatorEvaluator evaluator;
	const std::string path = g_getenv("PATH") ? g_getenv("PATH") : "";
	g_setenv("PATH", "/nonexistent", TRUE);
	const CalculatorEvaluation unavailable = evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "2+2");
	g_assert_cmpint(static_cast<int>(unavailable.state), ==,
			static_cast<int>(CalculatorEvaluationState::Unavailable));
	g_setenv("PATH", path.c_str(), TRUE);
	const CalculatorEvaluation invalid = evaluate_and_wait(evaluator,
			CalculatorEngine::Bc, "bad\ninput");
	g_assert_cmpint(static_cast<int>(invalid.state), ==,
			static_cast<int>(CalculatorEvaluationState::Failed));
}

void setup_stub_path()
{
	GError* error = nullptr;
	gchar* directory = g_dir_make_tmp("meowmenu-calculator-XXXXXX", &error);
	g_assert_no_error(error);
	g_assert_nonnull(directory);
	g_stub_dir = directory;
	g_free(directory);
	for (const char* name : { "bc", "qalc", "gcalccmd" })
	{
		const std::string link = g_stub_dir + G_DIR_SEPARATOR_S + name;
		g_assert_cmpint(symlink(MEOWMENU_CALCULATOR_STUB_PATH, link.c_str()), ==, 0);
	}
	g_old_path = g_getenv("PATH") ? g_getenv("PATH") : "";
	g_setenv("PATH", g_stub_dir.c_str(), TRUE);
}

void teardown_stub_path()
{
	g_setenv("PATH", g_old_path.c_str(), TRUE);
	for (const char* name : { "bc", "qalc", "gcalccmd" })
		g_remove((g_stub_dir + G_DIR_SEPARATOR_S + name).c_str());
	g_rmdir(g_stub_dir.c_str());
}

}

int main(int argc, char** argv)
{
	g_test_init(&argc, &argv, nullptr);
	setup_stub_path();
	g_test_add_func("/calculator/evaluator/success-bc-transport", test_success_and_bc_transport);
	g_test_add_func("/calculator/evaluator/direct-no-shell", test_direct_no_shell_transport);
	g_test_add_func("/calculator/evaluator/failures", test_failures);
	g_test_add_func("/calculator/evaluator/overflow", test_output_overflow);
	g_test_add_func("/calculator/evaluator/timeout", test_timeout);
	g_test_add_func("/calculator/evaluator/replacement", test_replacement_cancels_previous);
	g_test_add_func("/calculator/evaluator/teardown", test_teardown_silences_callback);
	g_test_add_func("/calculator/evaluator/unavailable-invalid", test_unavailable_and_invalid_input);
	const int result = g_test_run();
	teardown_stub_path();
	return result;
}
