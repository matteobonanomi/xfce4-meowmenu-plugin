/*
 * Headless tests for the locale-independent CSS alpha formatter declared in
 * panel-plugin/core/opacity-model.h.
 *
 * Design (per the feature's regression-coverage strategy):
 *   1. Always-run assertions (C1.1..C1.5) — meowmenu_format_css_alpha() emits a
 *      fixed-3-decimal, dot-separated rendering for representative alphas under
 *      whatever locale the test process happens to run in. This is the primary,
 *      environment-independent guarantee and is the floor the suite enforces.
 *   2. Opportunistic comma-decimal exercise (C1.6) — if a comma-decimal locale
 *      can be constructed, it is activated *thread-locally* via uselocale() and
 *      the always-run assertions are re-checked, proving the formatter is immune
 *      to an active comma LC_NUMERIC. Process-global locale state is never
 *      touched (the fix and its test must not mutate it; the panel hosts other
 *      plugins in the same process). The original locale is always restored
 *      (C1.8).
 *   3. Never a silent skip (C1.7) — when no comma-decimal locale is installed,
 *      an explicit notice is printed and the always-run assertions still gate
 *      the run, so a missing locale can never be mistaken for a passing
 *      comma-locale check.
 *
 * No GTK/GDK types are used; the pure formatter is linked directly.
 */

#include "core/opacity-model.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale.h>

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// Assert one alpha formats to exactly its required dot-decimal text, and that
// the output universally contains '.' and never ',' (C1.5).
void check_alpha(double alpha, const char* expected)
{
	char out[MEOWMENU_CSS_ALPHA_BUFSZ];
	meowmenu_format_css_alpha(alpha, out);
	CHECK(std::strcmp(out, expected) == 0);
	CHECK(std::strchr(out, '.') != nullptr);
	CHECK(std::strchr(out, ',') == nullptr);
}

// C1.1..C1.5: the always-run dot-output contract for representative alphas.
void assert_dot_output()
{
	check_alpha(0.0,   "0.000"); // C1.1
	check_alpha(1.0,   "1.000"); // C1.2
	check_alpha(0.6,   "0.600"); // C1.3
	check_alpha(0.123, "0.123"); // C1.4
}

} // namespace

int main()
{
	// (1) Always-run guarantee — holds under whatever locale this process uses.
	assert_dot_output();

	// (2) Opportunistic comma-decimal exercise, scoped thread-locally so it
	// never leaks into the process or sibling tests (C1.6 / C1.8).
	static const char* const comma_locales[] = {
		"it_IT.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8",
		"es_ES.UTF-8", "pt_BR.UTF-8", "nl_NL.UTF-8",
	};

	locale_t comma = (locale_t) 0;
	const char* comma_name = nullptr;
	for (size_t i = 0; i < sizeof(comma_locales) / sizeof(comma_locales[0]); ++i)
	{
		// Build a locale that overrides only LC_NUMERIC; (locale_t)0 is the base.
		comma = newlocale(LC_NUMERIC_MASK, comma_locales[i], (locale_t) 0);
		if (comma != (locale_t) 0)
		{
			comma_name = comma_locales[i];
			break;
		}
	}

	if (comma != (locale_t) 0)
	{
		std::printf("comma-locale exercise: using %s\n", comma_name);
		// Activate thread-locally and re-run the dot assertions; the formatter
		// must be byte-for-byte identical despite the active comma LC_NUMERIC.
		locale_t previous = uselocale(comma);
		assert_dot_output();
		// C1.8: restore the thread's original locale, then release the temp.
		uselocale(previous);
		freelocale(comma);
	}
	else
	{
		// C1.7: explicit, never a silent skip. The always-run assertions above
		// remain the enforced floor.
		std::printf("comma-locale exercise skipped — locale not installed\n");
	}

	if (g_failures != 0)
	{
		std::fprintf(stderr, "%d check(s) failed\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("all opacity-css-format checks passed\n");
	return EXIT_SUCCESS;
}
