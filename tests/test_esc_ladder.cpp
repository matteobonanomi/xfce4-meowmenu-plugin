/*
 * Headless tests for the progressive Esc state machine declared in
 * panel-plugin/core/window-keyboard.h.
 *
 * Covers contracts/esc-ladder.md §"Test plan".
 */

#include "core/window-keyboard.h"

#include <cstdio>
#include <cstdlib>

using WhiskerMenu::Keyboard::classify_esc_state;
using WhiskerMenu::Keyboard::esc_action;
using WhiskerMenu::Keyboard::EscAction;
using WhiskerMenu::Keyboard::EscState;

namespace
{

int g_failures = 0;

const char* state_name(EscState s)
{
	switch (s)
	{
	case EscState::ContextMenuOpen:  return "ContextMenuOpen";
	case EscState::ResizeInProgress: return "ResizeInProgress";
	case EscState::QueryNonEmpty:    return "QueryNonEmpty";
	case EscState::MenuOpen:         return "MenuOpen";
	}
	return "?";
}

const char* action_name(EscAction a)
{
	switch (a)
	{
	case EscAction::CloseContextMenu: return "CloseContextMenu";
	case EscAction::CancelResize:     return "CancelResize";
	case EscAction::ClearQuery:       return "ClearQuery";
	case EscAction::CloseMenu:        return "CloseMenu";
	}
	return "?";
}

#define EQS(actual, expected) do { \
		EscState _a = (actual); \
		EscState _e = (expected); \
		if (_a != _e) { \
			std::fprintf(stderr, "FAIL %s:%d: state got %s expected %s\n", \
			             __FILE__, __LINE__, state_name(_a), state_name(_e)); \
			++g_failures; \
		} \
	} while (0)

#define EQA(actual, expected) do { \
		EscAction _a = (actual); \
		EscAction _e = (expected); \
		if (_a != _e) { \
			std::fprintf(stderr, "FAIL %s:%d: action got %s expected %s\n", \
			             __FILE__, __LINE__, action_name(_a), action_name(_e)); \
			++g_failures; \
		} \
	} while (0)

void context_menu_wins()
{
	EQS(classify_esc_state(true, true, true), EscState::ContextMenuOpen);
	EQA(esc_action(EscState::ContextMenuOpen), EscAction::CloseContextMenu);
}

void resize_wins_over_query()
{
	EQS(classify_esc_state(false, true, true), EscState::ResizeInProgress);
	EQA(esc_action(EscState::ResizeInProgress), EscAction::CancelResize);
}

void query_wins_over_close()
{
	EQS(classify_esc_state(false, false, true), EscState::QueryNonEmpty);
	EQA(esc_action(EscState::QueryNonEmpty), EscAction::ClearQuery);
}

void close_when_idle()
{
	EQS(classify_esc_state(false, false, false), EscState::MenuOpen);
	EQA(esc_action(EscState::MenuOpen), EscAction::CloseMenu);
}

void four_press_progression()
{
	bool ctx = true, resize = true, query = true;

	EscState s1 = classify_esc_state(ctx, resize, query);
	EQS(s1, EscState::ContextMenuOpen);
	EQA(esc_action(s1), EscAction::CloseContextMenu);
	ctx = false;

	EscState s2 = classify_esc_state(ctx, resize, query);
	EQS(s2, EscState::ResizeInProgress);
	EQA(esc_action(s2), EscAction::CancelResize);
	resize = false;

	EscState s3 = classify_esc_state(ctx, resize, query);
	EQS(s3, EscState::QueryNonEmpty);
	EQA(esc_action(s3), EscAction::ClearQuery);
	query = false;

	EscState s4 = classify_esc_state(ctx, resize, query);
	EQS(s4, EscState::MenuOpen);
	EQA(esc_action(s4), EscAction::CloseMenu);
}

} // namespace

int main()
{
	context_menu_wins();
	resize_wins_over_query();
	query_wins_over_close();
	close_when_idle();
	four_press_progression();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_esc_ladder: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_esc_ladder: ok\n");
	return EXIT_SUCCESS;
}
