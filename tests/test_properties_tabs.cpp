/* test_properties_tabs:
 *
 * Encodes the placement grid for every Properties-dialog Xfconf binding and
 * asserts the invariants the dialog must preserve:
 *   1. No duplication — each Xfconf key appears in exactly one row.
 *   2. No omission — every required key is on the grid.
 *   3. Exactly six tabs in the known dictionary (General, User/Session,
 *      Search Bar, Results View / app-grid, Sidebar, Places). The Places
 *      tab models the milestone-005 controls bound under the /places
 *      Xfconf prefix.
 *   4. Sane enable-when values — docked|fullscreen rows correspond to widgets
 *      whose live behaviour is driven by /layout-mode (FR-003); sibling sub-
 *      enables (ProfileVisible, ViewModeIcons/List, SidebarLeftRight,
 *      PlacesFavouritesEnabled) must NOT claim layout-mode driving.
 *   5. Placement grid is complete and has no extra rows beyond the required
 *      key set — guards against the table silently growing stale.
 *
 * This is a pure-data test: it does not link against GTK or Xfconf. The
 * placement table mirrors what panel-plugin/settings-dialog.cpp consumes
 * when building the six init_*_tab() functions. Failures (1) or (2) block
 * merge per the no-loss guarantee (SC-001 / SC-002).
 */

#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <vector>

// T013: the display-free synced-keys list (driven by sync_preset_widgets) MUST
// equal the governed-key set, so a preset switch leaves no governed control
// stale (FR-001/003). Both lists live in the Settings-free preset-builtins.cpp,
// linked directly so this assertion needs no GTK display.
#include "presets/preset.h"

namespace
{

enum class Tab
{
	General,
	UserSession,
	SearchBar,
	AppGrid,
	Sidebar,
	Places,
};

enum class EnableWhen
{
	Always,
	Docked,
	Fullscreen,
	// Sub-enables tied to a sibling key, not to /layout-mode:
	SidebarLeftRight,        // category-show-name: greyed when sidebar-position ∈ {top, bottom}
	ProfileVisible,          // profile-shape: greyed when profile-position == hidden
	ViewModeIcons,           // grid-density, launcher-icon-size: greyed when view-mode != icons
	ViewModeList,            // launcher-show-description: greyed when view-mode != list
	// /places-* sub-enables. The whole Places tab is page-level gated by
	// /places/enabled (init_places_tab in settings-dialog.cpp). The
	// favourite-sync combo additionally requires /places/favourites-enabled,
	// modelled here as PlacesFavouritesEnabled.
	PlacesEnabled,
	PlacesFavouritesEnabled,
};

struct Row
{
	const char* setting_id;   // matches Xfconf key for keyed settings
	Tab         tab;
	EnableWhen  enable_when;
	bool        layout_mode_driven; // true iff enable_when uses /layout-mode
};

// The placement grid. Order is informational; the invariant checks are
// order-independent.
const Row kPlacementGrid[] = {
	// General
	{ "current-preset-id",         Tab::General,     EnableWhen::Always,     false },
	{ "button-title-visible",      Tab::General,     EnableWhen::Always,     false },
	{ "button-title",              Tab::General,     EnableWhen::Always,     false },
	{ "button-icon-visible",       Tab::General,     EnableWhen::Always,     false },
	{ "button-icon-name",          Tab::General,     EnableWhen::Always,     false },
	{ "button-single-row",         Tab::General,     EnableWhen::Always,     false },
	{ "layout-mode",               Tab::General,     EnableWhen::Always,     false },
	{ "panel-gap",                 Tab::General,     EnableWhen::Always,     false },
	{ "menu-width",                Tab::General,     EnableWhen::Docked,     true  },
	{ "menu-height",               Tab::General,     EnableWhen::Docked,     true  },
	{ "corner-radius",             Tab::General,     EnableWhen::Always,     false },
	{ "full-screen-opacity",       Tab::General,     EnableWhen::Fullscreen, true  },
	{ "stay-on-focus-out",         Tab::General,     EnableWhen::Always,     false },

	// User / Session
	{ "profile-position",          Tab::UserSession, EnableWhen::Always,         false },
	{ "profile-shape",             Tab::UserSession, EnableWhen::ProfileVisible, false },
	{ "commands-position",         Tab::UserSession, EnableWhen::Always,         false },
	{ "confirm-session-command",   Tab::UserSession, EnableWhen::Always,         false },

	// Search Bar
	{ "search-bar-position",       Tab::SearchBar,   EnableWhen::Always, false },
	{ "fuzzy-enabled",             Tab::SearchBar,   EnableWhen::Always, false },
	{ "fuzzy-threshold",           Tab::SearchBar,   EnableWhen::Always, false },
	{ "favorites-boost-enabled",   Tab::SearchBar,   EnableWhen::Always, false },
	{ "favorites-boost-level",     Tab::SearchBar,   EnableWhen::Always, false },
	{ "frecency-alpha",            Tab::SearchBar,   EnableWhen::Always, false },

	// App Grid
	{ "view-mode",                 Tab::AppGrid,     EnableWhen::Always,        false },
	{ "grid-density",              Tab::AppGrid,     EnableWhen::ViewModeIcons, false },
	{ "launcher-icon-size",        Tab::AppGrid,     EnableWhen::ViewModeIcons, false },
	{ "launcher-show-name",        Tab::AppGrid,     EnableWhen::Always,        false },
	{ "launcher-show-tooltip",     Tab::AppGrid,     EnableWhen::Always,        false },
	{ "launcher-show-description", Tab::AppGrid,     EnableWhen::ViewModeList,  false },
	{ "apps-opacity",              Tab::AppGrid,     EnableWhen::Docked,        true  },

	// Sidebar
	{ "category-show-name",        Tab::Sidebar,     EnableWhen::SidebarLeftRight, false },
	{ "category-icon-size",        Tab::Sidebar,     EnableWhen::Always,           false },
	{ "categories-opacity",        Tab::Sidebar,     EnableWhen::Docked,           true  },
	{ "sidebar-position",          Tab::Sidebar,     EnableWhen::Always,           false },
	{ "category-hover-activate",   Tab::Sidebar,     EnableWhen::Always,           false },
	{ "sort-categories",           Tab::Sidebar,     EnableWhen::Always,           false },
	{ "default-category",          Tab::Sidebar,     EnableWhen::Always,           false },
	{ "recent-items-max",          Tab::Sidebar,     EnableWhen::Always,           false },
	{ "favorites-in-recent",       Tab::Sidebar,     EnableWhen::Always,           false },

	// Places (milestone 005). Controls bound by init_places_tab():
	//   /places/enabled                — top-level switch (Always within tab).
	//   /places/history-enabled        — page-gated by /places/enabled.
	//   /places/favourites-enabled     — page-gated by /places/enabled.
	//   /places/favourite-sync         — additionally gated by favourites-enabled.
	//   /places/max-items              — page-gated by /places/enabled.
	//   /places/remember-last-mode     — page-gated by /places/enabled.
	//   /places/show-metadata          — page-gated by /places/enabled.
	// NOTE: /places/last-mode and /places/favourites are Xfconf-backed
	// runtime state, not Properties-dialog controls, so they are NOT on the
	// placement grid (test models the dialog surface, not the schema).
	{ "places/enabled",              Tab::Places,      EnableWhen::Always,                  false },
	{ "places/history-enabled",      Tab::Places,      EnableWhen::PlacesEnabled,           false },
	{ "places/favourites-enabled",   Tab::Places,      EnableWhen::PlacesEnabled,           false },
	{ "places/favourite-sync",       Tab::Places,      EnableWhen::PlacesFavouritesEnabled, false },
	{ "places/max-items",            Tab::Places,      EnableWhen::PlacesEnabled,           false },
	{ "places/remember-last-mode",   Tab::Places,      EnableWhen::PlacesEnabled,           false },
	{ "places/show-metadata",        Tab::Places,      EnableWhen::PlacesEnabled,           false },
};

// Every Xfconf key documented in contracts/xfconf-keys.md that surfaces in
// the new dialog. Must be a superset of the placement grid's setting_ids.
const char* const kRequiredKeys[] = {
	"current-preset-id",
	"button-title", "button-title-visible",
	"button-icon-name", "button-icon-visible",
	"button-single-row",
	"layout-mode", "panel-gap",
	"menu-width", "menu-height", "corner-radius",
	"full-screen-opacity", "stay-on-focus-out",
	"profile-position", "profile-shape", "commands-position",
	"confirm-session-command",
	"search-bar-position",
	"fuzzy-enabled", "fuzzy-threshold",
	"favorites-boost-enabled", "favorites-boost-level",
	"frecency-alpha",
	"view-mode", "grid-density",
	"launcher-icon-size", "launcher-show-name",
	"launcher-show-tooltip", "launcher-show-description",
	"apps-opacity",
	"category-show-name", "category-icon-size",
	"categories-opacity", "sidebar-position",
	"category-hover-activate", "sort-categories",
	"default-category", "recent-items-max", "favorites-in-recent",
	// Places milestone-005 controls
	"places/enabled", "places/history-enabled", "places/favourites-enabled",
	"places/favourite-sync", "places/max-items",
	"places/remember-last-mode", "places/show-metadata",
};

constexpr size_t kRowCount = sizeof(kPlacementGrid) / sizeof(kPlacementGrid[0]);
constexpr size_t kRequiredCount = sizeof(kRequiredKeys) / sizeof(kRequiredKeys[0]);

}  // namespace

// Invariant 1: no Xfconf key appears in more than one placement row.
static void test_no_duplication()
{
	std::set<std::string> seen;
	for (const auto& row : kPlacementGrid)
	{
		const bool inserted = seen.insert(row.setting_id).second;
		assert(inserted && "duplicate setting_id in placement grid");
	}
}

// Invariant 2: every required Xfconf key is on the grid (no silent drops).
static void test_no_omission()
{
	std::set<std::string> placed;
	for (const auto& row : kPlacementGrid)
		placed.insert(row.setting_id);

	for (size_t i = 0; i < kRequiredCount; ++i)
	{
		assert(placed.count(kRequiredKeys[i]) == 1
				&& "required Xfconf key missing from placement grid");
	}
}

// Invariant 3: every row maps to one of the six known tabs, and all six
// tabs are represented at least once. The Places tab (milestone 005) is the
// sixth and was added alongside init_places_tab() in settings-dialog.cpp.
static void test_exactly_six_tabs()
{
	std::set<int> tabs_seen;
	for (const auto& row : kPlacementGrid)
	{
		const int t = static_cast<int>(row.tab);
		assert(t >= 0 && t <= 5 && "row.tab out of known range");
		tabs_seen.insert(t);
	}
	assert(tabs_seen.size() == 6 && "not all six tabs are populated");
}

// Invariant 4: enable-when values are consistent with the live-wiring contract.
//  - Docked / Fullscreen rows must be marked layout-mode-driven.
//  - The sibling-driven sub-enables must NOT claim layout-mode binding.
//  - Always rows must NOT claim layout-mode binding.
static void test_sane_enable_when()
{
	for (const auto& row : kPlacementGrid)
	{
		switch (row.enable_when)
		{
		case EnableWhen::Always:
		case EnableWhen::SidebarLeftRight:
		case EnableWhen::ProfileVisible:
		case EnableWhen::ViewModeIcons:
		case EnableWhen::ViewModeList:
		case EnableWhen::PlacesEnabled:
		case EnableWhen::PlacesFavouritesEnabled:
			assert(!row.layout_mode_driven
					&& "non-layout-mode row claims layout-mode driver");
			break;
		case EnableWhen::Docked:
		case EnableWhen::Fullscreen:
			assert(row.layout_mode_driven
					&& "docked/fullscreen row missing layout-mode driver");
			break;
		}
	}
}

// Invariant 5 (T013): the placement grid covers exactly the required keys and
// nothing more — guards against the table silently growing stale.
static void test_placement_grid_complete_and_no_extras()
{
	std::set<std::string> placed;
	for (const auto& row : kPlacementGrid)
		placed.insert(row.setting_id);

	std::set<std::string> required;
	for (size_t i = 0; i < kRequiredCount; ++i)
		required.insert(kRequiredKeys[i]);

	// Every required key must be placed.
	for (const auto& k : required)
		assert(placed.count(k) == 1 && "required key missing from placement grid");

	// Every placed key must be in the required set (no extra/orphan rows).
	for (const auto& k : placed)
		assert(required.count(k) == 1 && "placement grid has orphan key not in required list");
}

// T013: synced_keys() must cover exactly governed_keys() — order-independent.
void test_synced_keys_cover_governed_keys()
{
	std::set<std::string> governed(WhiskerMenu::governed_keys().begin(),
		WhiskerMenu::governed_keys().end());
	std::set<std::string> synced(WhiskerMenu::synced_keys().begin(),
		WhiskerMenu::synced_keys().end());
	for (const auto& k : governed)
		assert(synced.count(k) == 1 && "governed key not synced by the Properties dialog");
	for (const auto& k : synced)
		assert(governed.count(k) == 1 && "synced key is not in the governed set");
}

int main()
{
	test_no_duplication();
	test_no_omission();
	test_exactly_six_tabs();
	test_sane_enable_when();
	test_placement_grid_complete_and_no_extras();
	test_synced_keys_cover_governed_keys();
	return 0;
}
