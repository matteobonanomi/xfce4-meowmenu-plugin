#ifndef MEOWMENU_TEST_ASSERTIONS_H
#define MEOWMENU_TEST_ASSERTIONS_H

#include <glib.h>

#include <string>

namespace meowmenu_test
{

/* add_case:
 * @path: stable GLib test path used in failure output.
 * @function: case body; it must not own process-wide teardown.
 *
 * Registers a named case without changing the assertion policy used by GLib.
 */
void add_case(const char* path, void (*function)());

/* check:
 * @condition: value that must be true for the current case.
 * @expression: source-level description shown when it is false.
 *
 * Records a failure even when the project is compiled with assertions
 * disabled. The function returns the condition so it can be used for a
 * caller's normal control flow without putting cleanup in an assertion.
 */
bool check(bool condition, const char* expression,
           const char* file, int line);

/* check_string:
 * @actual: value produced by the case; NULL is treated as an empty value.
 * @expected: exact expected value; NULL means an absent value.
 *
 * Performs an always-on string comparison and reports both values on failure.
 */
bool check_string(const char* actual, const char* expected,
                  const char* file, int line);

/* install_fatal_diagnostics:
 *
 * Makes unscoped warnings and critical messages fail the active GLib case.
 * Expected diagnostics are accepted only while an ExpectedMessageScope is
 * alive and are counted exactly once.
 */
void install_fatal_diagnostics();

class ExpectedMessageScope
{
public:
	ExpectedMessageScope(GLogLevelFlags level, const char* message_pattern);
	~ExpectedMessageScope();

	ExpectedMessageScope(const ExpectedMessageScope&) = delete;
	ExpectedMessageScope& operator=(const ExpectedMessageScope&) = delete;

	friend void install_fatal_diagnostics();
	bool accept(GLogLevelFlags level, const gchar* message);

	private:
	GLogLevelFlags level_;
	std::string pattern_;
	guint matches_;
};

}

#define MEOWMENU_TEST_CHECK(value) \
	::meowmenu_test::check((value), #value, __FILE__, __LINE__)

#define MEOWMENU_TEST_CHECK_STRING(actual, expected) \
	::meowmenu_test::check_string((actual), (expected), __FILE__, __LINE__)

#endif
