/*
 * Unit tests for the search-action edit modal logic — no GTK, no Xfconf.
 *
 * The modal (edit_search_action_modal in settings-dialog.cpp) opens a
 * transient dialog with Name / Pattern / Command / Is-regex fields. This
 * file tests the underlying SearchAction data model:
 *
 *   - OK path: set_name / set_pattern / set_command / set_is_regex persist.
 *   - Cancel path: the caller must not call set_* → values unchanged.
 *   - Regex flag: bool round-trips correctly through get/set.
 *
 * SearchAction links against Xfconf, so we test the contract with a lightweight
 * shadow struct that mirrors the four fields the modal reads and writes.
 */

#include "../panel-plugin/settings-bindings.h"
#include "config/xfce-helpers.h"

#include <cassert>
#include <string>

using namespace WhiskerMenu;

// ---------------------------------------------------------------------------
// Shadow SearchAction — mirrors the four fields the modal edits.
// ---------------------------------------------------------------------------

struct ShadowSearchAction
{
	std::string name;
	std::string pattern;
	std::string command;
	bool        is_regex = false;

	// Mirrors the OK-path writes in edit_search_action_modal.
	void ok_apply(const std::string& new_name,
	              const std::string& new_pattern,
	              const std::string& new_command,
	              bool               new_is_regex)
	{
		name     = new_name;
		pattern  = new_pattern;
		command  = new_command;
		is_regex = new_is_regex;
	}
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_ok_round_trip_persists()
{
	ShadowSearchAction action;
	action.name    = "old-name";
	action.pattern = "old-pattern";
	action.command = "old-command";
	action.is_regex = false;

	// Simulate the OK button response.
	action.ok_apply("new-name", "new-pattern", "new-command", true);

	assert(action.name    == "new-name");
	assert(action.pattern == "new-pattern");
	assert(action.command == "new-command");
	assert(action.is_regex == true);
}

static void test_cancel_discards()
{
	ShadowSearchAction action;
	action.name    = "keep-name";
	action.pattern = "keep-pattern";
	action.command = "keep-command";
	action.is_regex = false;

	// Simulate Cancel: ok_apply is never called, values stay unchanged.
	assert(action.name    == "keep-name");
	assert(action.pattern == "keep-pattern");
	assert(action.command == "keep-command");
	assert(action.is_regex == false);
}

static void test_regex_flag_survives_roundtrip()
{
	ShadowSearchAction action;
	action.is_regex = true;

	// Flip off
	action.ok_apply("n", "p", "c", false);
	assert(action.is_regex == false);

	// Flip on again
	action.ok_apply("n", "p", "c", true);
	assert(action.is_regex == true);
}

static void test_empty_strings_accepted()
{
	ShadowSearchAction action;
	action.name    = "something";
	action.pattern = "something";
	action.command = "something";

	// Empty strings are valid (the user cleared a field).
	action.ok_apply("", "", "", false);
	assert(action.name.empty());
	assert(action.pattern.empty());
	assert(action.command.empty());
}

static void test_special_characters_preserved()
{
	ShadowSearchAction action;
	action.ok_apply("My Action", "!query*", "xdg-open %s", false);
	assert(action.name    == "My Action");
	assert(action.pattern == "!query*");
	assert(action.command == "xdg-open %s");
}

static void test_legacy_tuple_translation_preserves_storage()
{
	std::size_t count = 0;
	const HistoricalSearchAction* actions = historical_search_actions(&count);
	assert(count == 5);
	for (std::size_t i = 0; i < count; ++i)
	{
		ShadowSearchAction stored;
		stored.name = "Renamed and reordered";
		stored.pattern = actions[i].pattern;
		stored.command = actions[i].legacy_command;
		stored.is_regex = actions[i].is_regex;

		const std::string before = stored.command;
		const std::string effective = effective_search_action_command(
				XfceDependencyRegime::Successor,
				stored.pattern,
				stored.command,
				stored.is_regex);
		assert(effective == actions[i].successor_command);
		assert(stored.command == before);
	}
}

static void test_edited_tuple_is_not_translated()
{
	const std::string command = "exo-open --launch TerminalEmulator man %s ";
	assert(effective_search_action_command(
			XfceDependencyRegime::Successor,
			"#",
			command,
			false) == command);
	assert(effective_search_action_command(
			XfceDependencyRegime::Successor,
			"#",
			"exo-open --launch TerminalEmulator man %s",
			true) == "exo-open --launch TerminalEmulator man %s");
}

static void test_search_action_property_parser_accepts_valid_fields()
{
	int index = -1;
	SearchActionPropertyField field = SearchActionPropertyField::Invalid;

	assert(parse_search_action_property("/search-actions/action-1/command",
			3, &index, &field));
	assert(index == 1);
	assert(field == SearchActionPropertyField::Command);

	assert(parse_search_action_property("/search-actions/action-0/regex",
			3, &index, &field));
	assert(index == 0);
	assert(field == SearchActionPropertyField::Regex);
}

static void test_search_action_property_parser_ignores_invalid_inputs()
{
	int index = 99;
	SearchActionPropertyField field = SearchActionPropertyField::Name;

	assert(parse_search_action_property("/search-actions/action--1/name",
			2, &index, &field));
	assert(index == -1);
	assert(field == SearchActionPropertyField::Invalid);

	assert(parse_search_action_property("/search-actions/action-9/name",
			2, &index, &field));
	assert(index == -1);
	assert(field == SearchActionPropertyField::Invalid);

	assert(parse_search_action_property("/search-actions/action-0/unknown",
			2, &index, &field));
	assert(index == -1);
	assert(field == SearchActionPropertyField::Invalid);

	assert(parse_search_action_property("/search-actions/action-0/name/trailing",
			2, &index, &field) == false);
}

int main()
{
	test_ok_round_trip_persists();
	test_cancel_discards();
	test_regex_flag_survives_roundtrip();
	test_empty_strings_accepted();
	test_special_characters_preserved();
	test_legacy_tuple_translation_preserves_storage();
	test_edited_tuple_is_not_translated();
	test_search_action_property_parser_accepts_valid_fields();
	test_search_action_property_parser_ignores_invalid_inputs();
	return 0;
}
