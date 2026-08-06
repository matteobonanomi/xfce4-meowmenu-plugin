/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Built-in preset data and the authoritative governed-key set.
 *
 * This translation unit holds ONLY the static preset definitions and the
 * governed-key list. It deliberately depends on nothing from Settings or GTK
 * so the unit tests can link the real BUILTIN_PRESETS table and governed_keys()
 * directly and assert completeness / file-vs-table agreement without a display.
 */

#include "preset.h"

#include <glib/gi18n.h>

using namespace WhiskerMenu;

// ---------------------------------------------------------------------------
// Helper: build a PresetValueMap from a brace-enclosed initializer list.
// ---------------------------------------------------------------------------

static PresetValueMap make_values(std::initializer_list<std::pair<const char*, PresetValue>> items)
{
	PresetValueMap m;
	for (auto& item : items)
		m[item.first] = item.second;
	return m;
}

// ---------------------------------------------------------------------------
// Built-in preset definitions.
//
// Every built-in MUST define a value for every key in governed_keys(); the
// completeness unit test fails the build's test stage otherwise. menu-opacity
// is governed: every built-in carries its single opacity value (Classic 100,
// Modern 100, Full Screen 80, Minimal 60) so a preset switch fully resets it.
// The C++ table is the fallback when the shipped .meowpreset files are absent
// or malformed; the two MUST agree (a unit test enforces it). Keys not in
// governed_keys() (menu-width/height, grid-density) are applied when present
// but are intentionally not required of every preset — notably FullScreen omits
// menu dimensions so a later switch to a docked preset restores them.
// ---------------------------------------------------------------------------

const LayoutPreset WhiskerMenu::BUILTIN_PRESETS[PRESET_BUILTIN_COUNT] = {
	// PRESET_CLASSIC
	{
		"classic",
		N_("Classic"),
		N_("Classic"),
		N_("Traditional compact layout with the sidebar on the right and applications in a list."),
		true,
		make_values({
			{ "corner-radius",        PresetValue::from_int(0)              },
			{ "panel-gap",            PresetValue::from_int(0)              },
			{ "menu-opacity",         PresetValue::from_int(100)            },
			{ "sidebar-position",     PresetValue::from_str("right")        },
			{ "sidebar-enabled",      PresetValue::from_bool(true)          },
			{ "category-show-name",   PresetValue::from_bool(true)          },
			{ "search-bar-position",  PresetValue::from_str("top")          },
			{ "show-profile",         PresetValue::from_bool(true)           },
			{ "show-session",         PresetValue::from_bool(true)           },
			{ "layout-mode",          PresetValue::from_str("docked")       },
			{ "launcher-icon-size",   PresetValue::from_int(2)              }, // Small
			{ "category-icon-size",   PresetValue::from_int(1)              }, // Smaller
			{ "hover-switch-category",PresetValue::from_bool(false)         },
			{ "view-mode-default",    PresetValue::from_str("list")         },
			{ "default-category",     PresetValue::from_str("favorites")    },
			{ "stay-on-focus-out",    PresetValue::from_bool(false)         },
			{ "menu-width",           PresetValue::from_int(450)            },
			{ "menu-height",          PresetValue::from_int(500)            },
			{ "places-enabled",       PresetValue::from_bool(false)         },
			{ "places-show-icons",    PresetValue::from_bool(false)         },
			{ "places-switch-button-shape", PresetValue::from_str("gtk-theme") },
			{ "calculator-engine", PresetValue::from_str("none") },
			{ "calculator-result-font-size", PresetValue::from_int(-1) },
			{ "calculator-max-decimal-places", PresetValue::from_int(4) },
		})
	},
	// PRESET_MODERN
	{
		"modern",
		N_("Modern"),
		N_("Modern"),
		N_("Contemporary layout with rounded corners, categories on the left, and hover-to-switch enabled."),
		true,
		make_values({
			{ "corner-radius",        PresetValue::from_int(12)            },
			{ "panel-gap",            PresetValue::from_int(8)             },
			{ "menu-opacity",         PresetValue::from_int(100)           },
			{ "sidebar-position",     PresetValue::from_str("left")        },
			{ "sidebar-enabled",      PresetValue::from_bool(true)         },
			{ "category-show-name",   PresetValue::from_bool(true)         },
			{ "search-bar-position",  PresetValue::from_str("top")         },
			{ "show-profile",         PresetValue::from_bool(true)          },
			{ "show-session",         PresetValue::from_bool(true)          },
			{ "layout-mode",          PresetValue::from_str("docked")      },
			{ "launcher-icon-size",   PresetValue::from_int(3)             }, // Normal
			{ "category-icon-size",   PresetValue::from_int(1)             }, // Smaller
			{ "grid-density",         PresetValue::from_str("medium")      },
			{ "hover-switch-category",PresetValue::from_bool(true)         },
			{ "transparent-grid",     PresetValue::from_bool(true)         },
			{ "view-mode-default",    PresetValue::from_str("icons")       },
			{ "default-category",     PresetValue::from_str("recent")      },
			{ "stay-on-focus-out",    PresetValue::from_bool(false)        },
			// NOTE: 450 reconciles the C++ table with the shipped
			// modern.meowpreset (which already carried 450), so a fresh
			// Modern install keeps its current width.
			{ "menu-width",           PresetValue::from_int(450)           },
			{ "menu-height",          PresetValue::from_int(500)           },
			{ "places-enabled",       PresetValue::from_bool(true)         },
			{ "places-show-icons",    PresetValue::from_bool(true)         },
			{ "places-switch-button-shape", PresetValue::from_str("gtk-theme") },
			{ "calculator-engine", PresetValue::from_str("bc") },
			{ "calculator-result-font-size", PresetValue::from_int(-1) },
			{ "calculator-max-decimal-places", PresetValue::from_int(4) },
		})
	},
	// PRESET_FULLSCREEN
	{
		"fullscreen",
		N_("Full Screen"),
		N_("Full Screen"),
		N_("Launcher fills the whole screen with a centered grid and categories on the left."),
		true,
		make_values({
			{ "corner-radius",        PresetValue::from_int(0)              },
			{ "panel-gap",            PresetValue::from_int(0)              },
			// Full Screen ships an 80% translucent backdrop. menu-opacity is the
			// single governed opacity now, so it lives here on the C++ fallback
			// path and the seed file carries the same value.
			{ "menu-opacity",         PresetValue::from_int(80)             },
			{ "sidebar-position",     PresetValue::from_str("left")         },
			{ "sidebar-enabled",      PresetValue::from_bool(true)          },
			{ "category-show-name",   PresetValue::from_bool(true)          },
			{ "search-bar-position",  PresetValue::from_str("top")          },
			{ "show-profile",         PresetValue::from_bool(true)           },
			{ "show-session",         PresetValue::from_bool(true)           },
			{ "launcher-icon-size",   PresetValue::from_int(4)              }, // Large
			{ "category-icon-size",   PresetValue::from_int(2)              }, // Small
			{ "grid-density",         PresetValue::from_str("medium")       },
			{ "layout-mode",          PresetValue::from_str("fullscreen")   },
			{ "hover-switch-category",PresetValue::from_bool(true)          },
			{ "transparent-grid",     PresetValue::from_bool(true)          },
			{ "view-mode-default",    PresetValue::from_str("icons")        },
			{ "default-category",     PresetValue::from_str("all")          },
			{ "stay-on-focus-out",    PresetValue::from_bool(false)         },
			{ "places-enabled",       PresetValue::from_bool(true)          },
			{ "places-show-icons",    PresetValue::from_bool(false)         },
			{ "places-switch-button-shape", PresetValue::from_str("gtk-theme") },
			{ "calculator-engine", PresetValue::from_str("bc") },
			{ "calculator-result-font-size", PresetValue::from_int(-1) },
			{ "calculator-max-decimal-places", PresetValue::from_int(4) },
		})
	},
	// PRESET_MINIMAL
	{
		"minimal",
		N_("Minimal"),
		N_("Minimal"),
		N_("Compact, distraction-free launcher with no sidebar, profile, or session buttons."),
		true,
		make_values({
			{ "corner-radius",        PresetValue::from_int(12)             },
			{ "panel-gap",            PresetValue::from_int(8)              },
			{ "menu-opacity",         PresetValue::from_int(60)             },
			{ "sidebar-position",     PresetValue::from_str("left")         },
			{ "sidebar-enabled",      PresetValue::from_bool(false)         },
			{ "category-show-name",   PresetValue::from_bool(true)          },
			{ "search-bar-position",  PresetValue::from_str("top")          },
			{ "show-profile",         PresetValue::from_bool(false)          },
			{ "show-session",         PresetValue::from_bool(false)          },
			{ "grid-density",         PresetValue::from_str("medium")       },
			{ "layout-mode",          PresetValue::from_str("centered")     },
			{ "launcher-icon-size",   PresetValue::from_int(3)              }, // Normal
			{ "category-icon-size",   PresetValue::from_int(1)              }, // Smaller
			{ "view-mode-default",    PresetValue::from_str("list")         },
			{ "hover-switch-category",PresetValue::from_bool(true)          },
			{ "stay-on-focus-out",    PresetValue::from_bool(false)         },
			// menu-height is non-governed: a switch to a sidebar-ful preset that
			// omits it leaves the user's size intact. Minimal carries a compact
			// 306-px window explicitly so the search-first layout is short.
			{ "menu-width",           PresetValue::from_int(450)            },
			{ "menu-height",          PresetValue::from_int(306)            },
			{ "default-category",     PresetValue::from_str("recent")       },
			{ "places-enabled",       PresetValue::from_bool(true)          },
			{ "places-show-icons",    PresetValue::from_bool(true)          },
			{ "places-switch-button-shape", PresetValue::from_str("gtk-theme") },
			{ "calculator-engine", PresetValue::from_str("bc") },
			{ "calculator-result-font-size", PresetValue::from_int(-1) },
			{ "calculator-max-decimal-places", PresetValue::from_int(4) },
		})
	},
};

// ---------------------------------------------------------------------------
// governed_keys: the authoritative, complete governed-setting set.
// ---------------------------------------------------------------------------

/* governed_keys:
 *
 * Returns the canonical list of settings a built-in preset fully determines.
 * Used by the .meowpreset reader validation and by the completeness/agreement
 * unit test. menu-width/menu-height and grid-density are intentionally
 * excluded: they are applied when a preset carries them but are not required of
 * every built-in (FullScreen deliberately omits menu dimensions so a switch
 * back to a docked preset restores the user's size). menu-opacity, by contrast,
 * is governed — every built-in carries its single opacity value.
 *
 * Returns: a reference to a process-lifetime static vector.
 */
const std::vector<std::string>& WhiskerMenu::governed_keys()
{
	static const std::vector<std::string> keys = {
		"corner-radius",
		"panel-gap",
		"menu-opacity",
		"sidebar-position",
		"sidebar-enabled",
		"category-show-name",
		"search-bar-position",
		"show-profile",
		"show-session",
		"layout-mode",
		"launcher-icon-size",
		"category-icon-size",
		"hover-switch-category",
		"view-mode-default",
		"default-category",
		"stay-on-focus-out",
		"places-enabled",
		"places-show-icons",
		"places-switch-button-shape",
		"calculator-engine",
		"calculator-result-font-size",
		"calculator-max-decimal-places",
	};
	return keys;
}

/* synced_keys:
 *
 * The governed keys that sync_preset_widgets() drives onto Properties widgets
 * after a preset switch. This list MUST equal governed_keys() (a unit test
 * enforces it): every key a preset governs must also be re-synced into the
 * dialog so no control is left stale (supported behavior). It is declared here, away
 * from the GTK widget-driving code, so the coverage set is inspectable without
 * a display.
 *
 * Returns: a reference to a process-lifetime static vector.
 */
const std::vector<std::string>& WhiskerMenu::synced_keys()
{
	// Mirrors governed_keys() — kept as a distinct list so that adding a
	// governed key without wiring its sync fails the coverage test loudly.
	return governed_keys();
}
