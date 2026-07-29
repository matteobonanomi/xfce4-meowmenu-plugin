/*
 * Pure dependency-regime, helper-selection, tuple, and quoting regressions.
 */

#include "config/xfce-helpers.h"

#include <glib.h>

#include <cstdio>
#include <string>

using namespace WhiskerMenu;

namespace
{

int failures = 0;

#define CHECK(condition) do { \
		if (!(condition)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", \
					__FILE__, __LINE__, #condition); \
			++failures; \
		} \
	} while (0)

void check_command_argument(const std::string& command, const char* expected)
{
	gint argc = 0;
	gchar** argv = nullptr;
	GError* error = nullptr;
	CHECK(g_shell_parse_argv(command.c_str(), &argc, &argv, &error));
	CHECK(error == nullptr);
	CHECK(argc >= 1);
	if (argc >= 1)
	{
		CHECK(g_strcmp0(argv[argc - 1], expected) == 0);
	}
	g_clear_error(&error);
	g_strfreev(argv);
}

void test_regime_selectors()
{
	CHECK(std::string(xfce_opener(XfceDependencyRegime::Legacy)) == "exo-open");
	CHECK(std::string(xfce_opener(XfceDependencyRegime::Successor)) == "xfce-open");
	CHECK(std::string(xfce_desktop_item_editor(XfceDependencyRegime::Legacy))
			== "exo-desktop-item-edit");
	CHECK(std::string(xfce_desktop_item_editor(XfceDependencyRegime::Successor))
			== "xfce-desktop-item-edit");
	CHECK(std::string(xfce_icon_chooser_family(XfceDependencyRegime::Legacy))
			== "exo-icon-chooser");
	CHECK(std::string(xfce_icon_chooser_family(XfceDependencyRegime::Successor))
			== "xfce-icon-chooser");
#if defined(MEOWMENU_XFCE_SUCCESSOR) && MEOWMENU_XFCE_SUCCESSOR
	CHECK(current_xfce_dependency_regime() == XfceDependencyRegime::Successor);
#else
	CHECK(current_xfce_dependency_regime() == XfceDependencyRegime::Legacy);
#endif
}

void test_historical_registry()
{
	std::size_t count = 0;
	const HistoricalSearchAction* actions = historical_search_actions(&count);
	CHECK(actions != nullptr);
	CHECK(count == 5);

	for (std::size_t i = 0; i < count; ++i)
	{
		const HistoricalSearchAction& action = actions[i];
		CHECK(effective_search_action_command(
				XfceDependencyRegime::Legacy,
				action.pattern,
				action.legacy_command,
				action.is_regex) == action.legacy_command);
		CHECK(effective_search_action_command(
				XfceDependencyRegime::Successor,
				action.pattern,
				action.legacy_command,
				action.is_regex) == action.successor_command);

		CHECK(effective_search_action_command(
				XfceDependencyRegime::Successor,
				std::string(action.pattern) + "x",
				action.legacy_command,
				action.is_regex) == action.legacy_command);
		CHECK(effective_search_action_command(
				XfceDependencyRegime::Successor,
				action.pattern,
				std::string(action.legacy_command) + " ",
				action.is_regex) == std::string(action.legacy_command) + " ");
		CHECK(effective_search_action_command(
				XfceDependencyRegime::Successor,
				action.pattern,
				action.legacy_command,
				!action.is_regex) == action.legacy_command);
	}

	CHECK(effective_search_action_command(
			XfceDependencyRegime::Successor,
			"-",
			"catfish --path=~ --start %s",
			false) == "catfish --path=~ --start %s");
	CHECK(effective_search_action_command(
			XfceDependencyRegime::Successor,
			"custom",
			"sh -c 'echo exo-open'",
			false) == "sh -c 'echo exo-open'");
}

void test_quoted_commands()
{
	const char* path = "/tmp/Meow '$(touch nope)' `literal` (folder) \\";
	for (XfceDependencyRegime regime :
			{XfceDependencyRegime::Legacy, XfceDependencyRegime::Successor})
	{
		std::string file_command = build_file_manager_command(regime, path);
		std::string terminal_command = build_terminal_command(regime, path);
		std::string help_command = build_help_command(
				regime, "https://example.invalid/a path?q='$value'");
		CHECK(file_command.find(xfce_opener(regime)) == 0);
		CHECK(terminal_command.find(xfce_opener(regime)) == 0);
		CHECK(help_command.find(xfce_opener(regime)) == 0);
		check_command_argument(file_command, path);
		check_command_argument(terminal_command, path);
		check_command_argument(help_command, "https://example.invalid/a path?q='$value'");
	}
}

void test_complete_interaction_matrix()
{
	for (XfceDependencyRegime regime :
			{XfceDependencyRegime::Legacy, XfceDependencyRegime::Successor})
	{
		std::size_t count = 0;
		const HistoricalSearchAction* actions = historical_search_actions(&count);
		CHECK(count == 5);
		CHECK(std::string(xfce_icon_chooser_family(regime)).find(
				regime == XfceDependencyRegime::Legacy ? "exo-" : "xfce-") == 0);
		CHECK(build_help_command(regime, "https://example.invalid").find(
				xfce_opener(regime)) == 0);
		for (std::size_t i = 0; i < count; ++i)
		{
			CHECK(effective_search_action_command(
					regime,
					actions[i].pattern,
					actions[i].legacy_command,
					actions[i].is_regex).find(
							xfce_opener(regime)) == 0);
		}
		CHECK(std::string(xfce_desktop_item_editor(regime)).find(
				regime == XfceDependencyRegime::Legacy ? "exo-" : "xfce-") == 0);
		CHECK(build_file_manager_command(regime, "/tmp/path").find(
				xfce_opener(regime)) == 0);
		CHECK(build_terminal_command(regime, "/tmp/path").find(
				xfce_opener(regime)) == 0);
	}

	std::size_t count = 0;
	const HistoricalSearchAction* actions = historical_search_actions(&count);
	for (std::size_t i = 0; i < count; ++i)
	{
		CHECK(std::string(actions[i].successor_command).find("exo-")
				== std::string::npos);
	}
}

}

int main()
{
	test_regime_selectors();
	test_historical_registry();
	test_quoted_commands();
	test_complete_interaction_matrix();
	return failures == 0 ? 0 : 1;
}
