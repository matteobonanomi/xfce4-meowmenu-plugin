/*
 * Headless tests for classify_key / is_printable_for_search declared in
 * panel-plugin/core/window-keyboard.h. Fabricates GdkEventKey values
 * directly; no display server required.
 *
 * Covers contracts/key-routing.md §"Test plan".
 */

#include "core/window-keyboard.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using WhiskerMenu::Keyboard::classify_key;
using WhiskerMenu::Keyboard::is_printable_for_search;
using WhiskerMenu::Keyboard::KeyClass;
using WhiskerMenu::Keyboard::ActivationDebounce;

namespace
{

int g_failures = 0;

const char* class_name(KeyClass c)
{
	switch (c)
	{
	case KeyClass::ImeComposition:  return "ImeComposition";
	case KeyClass::ModifierOnly:    return "ModifierOnly";
	case KeyClass::FunctionUtility: return "FunctionUtility";
	case KeyClass::Printable:       return "Printable";
	}
	return "?";
}

#define EQC(actual, expected) do { \
		KeyClass _a = (actual); \
		KeyClass _e = (expected); \
		if (_a != _e) { \
			std::fprintf(stderr, "FAIL %s:%d: got %s expected %s\n", \
			             __FILE__, __LINE__, class_name(_a), class_name(_e)); \
			++g_failures; \
		} \
	} while (0)

GdkEventKey make_event(guint keyval, guint state = 0, bool is_modifier = false)
{
	GdkEventKey e;
	std::memset(&e, 0, sizeof(e));
	e.type = GDK_KEY_PRESS;
	e.keyval = keyval;
	e.state = state;
	e.is_modifier = is_modifier ? 1 : 0;
	return e;
}

void letter_is_printable()
{
	GdkEventKey e = make_event(GDK_KEY_a);
	EQC(classify_key(&e), KeyClass::Printable);
}

void space_is_printable()
{
	// the documented behavior: Space must extend the query, not toggle Mode.
	GdkEventKey e = make_event(GDK_KEY_space);
	EQC(classify_key(&e), KeyClass::Printable);
}

void shift_letter_is_printable()
{
	GdkEventKey e = make_event(GDK_KEY_A, GDK_SHIFT_MASK);
	EQC(classify_key(&e), KeyClass::Printable);
}

void ctrl_letter_not_printable()
{
	GdkEventKey e = make_event(GDK_KEY_a, GDK_CONTROL_MASK);
	KeyClass c = classify_key(&e);
	// Ctrl+letter must NOT be routed; either FunctionUtility (because
	// Ctrl held disables printable test) or ModifierOnly is acceptable.
	if (c == KeyClass::Printable)
	{
		std::fprintf(stderr, "FAIL %s:%d: ctrl_letter classified Printable\n",
		             __FILE__, __LINE__);
		++g_failures;
	}
}

void f5_is_function()
{
	GdkEventKey e = make_event(GDK_KEY_F5);
	EQC(classify_key(&e), KeyClass::FunctionUtility);
}

void delete_is_function()
{
	GdkEventKey e = make_event(GDK_KEY_Delete);
	EQC(classify_key(&e), KeyClass::FunctionUtility);
}

void arrow_keys_are_function()
{
	const guint arrows[] = { GDK_KEY_Up, GDK_KEY_Down, GDK_KEY_Left, GDK_KEY_Right };
	for (guint kv : arrows)
	{
		GdkEventKey e = make_event(kv);
		EQC(classify_key(&e), KeyClass::FunctionUtility);
	}
}

void home_end_pageup_pagedown_function()
{
	const guint navs[] = { GDK_KEY_Home, GDK_KEY_End, GDK_KEY_Page_Up, GDK_KEY_Page_Down };
	for (guint kv : navs)
	{
		GdkEventKey e = make_event(kv);
		EQC(classify_key(&e), KeyClass::FunctionUtility);
	}
}

void menu_key_is_function()
{
	GdkEventKey e = make_event(GDK_KEY_Menu);
	EQC(classify_key(&e), KeyClass::FunctionUtility);
}

void shift_alone_modifier()
{
	GdkEventKey e = make_event(GDK_KEY_Shift_L, 0, true);
	EQC(classify_key(&e), KeyClass::ModifierOnly);
}

void iso_level3_modifier()
{
	GdkEventKey e = make_event(GDK_KEY_ISO_Level3_Shift);
	EQC(classify_key(&e), KeyClass::ModifierOnly);
}

void ime_composition_marked()
{
	GdkEventKey e = make_event(GDK_KEY_a, GDK_MODIFIER_RESERVED_25_MASK);
	EQC(classify_key(&e), KeyClass::ImeComposition);
}

void control_keys_not_printable()
{
	// Tab, Return, Escape, Backspace all map to control codepoints
	// through gdk_keyval_to_unicode (0x09, 0x0d, 0x1b, 0x08). They
	// must NOT be classified Printable — each has a dedicated
	// dispatch path and routing them into the entry would corrupt
	// the query (the documented behavior, the documented behavior, the documented behavior, the documented behavior).
	const guint controls[] = {
		GDK_KEY_Tab, GDK_KEY_ISO_Left_Tab,
		GDK_KEY_Return, GDK_KEY_KP_Enter,
		GDK_KEY_Escape,
		GDK_KEY_BackSpace,
	};
	for (guint kv : controls)
	{
		GdkEventKey e = make_event(kv);
		KeyClass c = classify_key(&e);
		if (c == KeyClass::Printable)
		{
			std::fprintf(stderr, "FAIL %s:%d: keyval %u classified Printable\n",
			             __FILE__, __LINE__, kv);
			++g_failures;
		}
	}
}

void non_latin_printable()
{
	// CJK "中" — U+4E2D. GDK encodes Unicode keysyms as
	// 0x01000000 | codepoint (see gdk_keyval_to_unicode in GDK).
	GdkEventKey e = make_event(0x01000000u | 0x4E2Du);
	EQC(classify_key(&e), KeyClass::Printable);
}

void null_event_safe()
{
	// Defensive: classify_key MUST be a total function and tolerate
	// a NULL pointer without crashing.
	(void)classify_key(nullptr);
	(void)is_printable_for_search(nullptr);
}

void convenience_predicate_matches()
{
	GdkEventKey letter = make_event(GDK_KEY_a);
	if (!is_printable_for_search(&letter))
	{
		std::fprintf(stderr, "FAIL %s:%d: is_printable_for_search(a) false\n",
		             __FILE__, __LINE__);
		++g_failures;
	}
	GdkEventKey f5 = make_event(GDK_KEY_F5);
	if (is_printable_for_search(&f5))
	{
		std::fprintf(stderr, "FAIL %s:%d: is_printable_for_search(F5) true\n",
		             __FILE__, __LINE__);
		++g_failures;
	}
}

void calculator_navigation_keys_remain_utility()
{
	for (guint keyval : { GDK_KEY_Up, GDK_KEY_Down, GDK_KEY_Tab,
			GDK_KEY_Return, GDK_KEY_KP_Enter })
	{
		GdkEventKey event = make_event(keyval);
		EQC(classify_key(&event), KeyClass::FunctionUtility);
	}
}

void held_enter_activates_once_per_debounce_window()
{
	ActivationDebounce debounce;
	assert(debounce.accept(300000));
	assert(!debounce.accept(310000));
	assert(!debounce.accept(549999));
	assert(debounce.accept(550000));
}

} // namespace

int main()
{
	letter_is_printable();
	space_is_printable();
	shift_letter_is_printable();
	ctrl_letter_not_printable();
	f5_is_function();
	delete_is_function();
	arrow_keys_are_function();
	home_end_pageup_pagedown_function();
	menu_key_is_function();
	shift_alone_modifier();
	iso_level3_modifier();
	ime_composition_marked();
	control_keys_not_printable();
	non_latin_printable();
	null_event_safe();
	convenience_predicate_matches();
	calculator_navigation_keys_remain_utility();
	held_enter_activates_once_per_debounce_window();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_key_routing: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_key_routing: ok\n");
	return EXIT_SUCCESS;
}
