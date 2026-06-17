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

// Column placement within a section's two equal-width halves. Full = spans both
// halves (no midpoint applies) — a whole-row slider, an unchanged section, or a
// single-column tab. Every keyed row now pins an explicit column, cross-checked
// against the placement contract in kColumnContract below.
enum class Column
{
	Full,
	C1,
	C2,
};

struct Row
{
	const char* setting_id;   // matches Xfconf key for keyed settings
	Tab         tab;
	EnableWhen  enable_when;
	bool        layout_mode_driven; // true iff enable_when uses /layout-mode
	Column      column;             // C1/C2 within the section; General rows only
};

// The placement grid. Order is informational; the invariant checks are
// order-independent.
const Row kPlacementGrid[] = {
	// General — column per contracts/general-tab-placement.md.
	{ "current-preset-id",         Tab::General,     EnableWhen::Always,     false, Column::C1 },
	{ "button-title-visible",      Tab::General,     EnableWhen::Always,     false, Column::C1 },
	{ "button-title",              Tab::General,     EnableWhen::Always,     false, Column::C1 },
	{ "button-icon-visible",       Tab::General,     EnableWhen::Always,     false, Column::C2 },
	{ "button-icon-name",          Tab::General,     EnableWhen::Always,     false, Column::C2 },
	{ "button-single-row",         Tab::General,     EnableWhen::Always,     false, Column::C1 },
	{ "layout-mode",               Tab::General,     EnableWhen::Always,     false, Column::C1 },
	{ "panel-gap",                 Tab::General,     EnableWhen::Always,     false, Column::C1 },
	{ "menu-width",                Tab::General,     EnableWhen::Docked,     true,  Column::C1 },
	{ "menu-height",               Tab::General,     EnableWhen::Docked,     true,  Column::C2 },
	{ "corner-radius",             Tab::General,     EnableWhen::Always,     false, Column::C2 },
	// Menu opacity is sensitive in every layout mode (gated on compositing, not
	// /layout-mode), so it is Always / not layout-mode-driven.
	{ "menu-opacity",              Tab::General,     EnableWhen::Always,     false, Column::C1 },
	{ "stay-on-focus-out",         Tab::General,     EnableWhen::Always,     false, Column::C2 },

	// User / Session — NOT relaid out into two columns; every keyed row spans
	// the section (Full), left-aligned (FR-010/011).
	{ "profile-position",          Tab::UserSession, EnableWhen::Always,         false, Column::Full },
	{ "profile-shape",             Tab::UserSession, EnableWhen::ProfileVisible, false, Column::Full },
	{ "commands-position",         Tab::UserSession, EnableWhen::Always,         false, Column::Full },
	{ "confirm-session-command",   Tab::UserSession, EnableWhen::Always,         false, Column::Full },

	// Search Bar — column per contracts/tab-placement.md.
	{ "search-bar-position",       Tab::SearchBar,   EnableWhen::Always, false, Column::C1 },
	{ "fuzzy-enabled",             Tab::SearchBar,   EnableWhen::Always, false, Column::C1 },
	{ "fuzzy-threshold",           Tab::SearchBar,   EnableWhen::Always, false, Column::C2 },
	{ "favorites-boost-enabled",   Tab::SearchBar,   EnableWhen::Always, false, Column::C1 },
	{ "favorites-boost-level",     Tab::SearchBar,   EnableWhen::Always, false, Column::C2 },
	{ "frecency-alpha",            Tab::SearchBar,   EnableWhen::Always, false, Column::Full },

	// App Grid — column per contracts/tab-placement.md. view-mode (the icons/
	// list/tree button-box) keeps its unchanged whole-section layout, so it stays
	// Full. The former app-box opacity control was removed (one menu-opacity now).
	{ "view-mode",                 Tab::AppGrid,     EnableWhen::Always,        false, Column::Full },
	{ "grid-density",              Tab::AppGrid,     EnableWhen::ViewModeIcons, false, Column::C1 },
	{ "launcher-icon-size",        Tab::AppGrid,     EnableWhen::ViewModeIcons, false, Column::C2 },
	{ "launcher-show-name",        Tab::AppGrid,     EnableWhen::Always,        false, Column::C1 },
	{ "launcher-show-tooltip",     Tab::AppGrid,     EnableWhen::Always,        false, Column::C2 },
	{ "launcher-show-description", Tab::AppGrid,     EnableWhen::ViewModeList,  false, Column::C1 },

	// Sidebar — column per contracts/tab-placement.md. The unchanged
	// Default-category radio group spans the section (Full); the rest split across
	// C1/C2. The former sidebar opacity control was removed (one menu-opacity now).
	{ "category-show-name",        Tab::Sidebar,     EnableWhen::SidebarLeftRight, false, Column::C1 },
	{ "category-icon-size",        Tab::Sidebar,     EnableWhen::Always,           false, Column::C2 },
	{ "sidebar-position",          Tab::Sidebar,     EnableWhen::Always,           false, Column::C1 },
	{ "category-hover-activate",   Tab::Sidebar,     EnableWhen::Always,           false, Column::C1 },
	{ "sort-categories",           Tab::Sidebar,     EnableWhen::Always,           false, Column::C2 },
	{ "default-category",          Tab::Sidebar,     EnableWhen::Always,           false, Column::Full },
	{ "recent-items-max",          Tab::Sidebar,     EnableWhen::Always,           false, Column::C1 },
	{ "favorites-in-recent",       Tab::Sidebar,     EnableWhen::Always,           false, Column::C2 },

	// Places (milestone 005). Controls bound by init_places_tab():
	//   /places/enabled                — top-level switch (Always within tab).
	//   /places/history-enabled        — page-gated by /places/enabled.
	//   /places/favourites-enabled     — page-gated by /places/enabled.
	//   /places/favourite-sync         — additionally gated by favourites-enabled.
	//   /places/max-items              — page-gated by /places/enabled.
	//   /places/remember-last-mode     — page-gated by /places/enabled.
	// NOTE: /places/last-mode, /places/favourites, and /places/switch-show-icons
	// are NOT on the placement grid — the first two are Xfconf-backed runtime
	// state, and switch-show-icons is a dialog control the existing model does
	// not key (its C2 placement is the visual contract, verified by the relayout
	// and manually per SC-004, not by a headless column assertion). The test
	// models the keyed dialog surface, not the full schema. Column per
	// contracts/tab-placement.md.
	{ "places/enabled",              Tab::Places,      EnableWhen::Always,                  false, Column::C1 },
	{ "places/history-enabled",      Tab::Places,      EnableWhen::PlacesEnabled,           false, Column::C1 },
	{ "places/favourites-enabled",   Tab::Places,      EnableWhen::PlacesEnabled,           false, Column::C2 },
	{ "places/favourite-sync",       Tab::Places,      EnableWhen::PlacesFavouritesEnabled, false, Column::C1 },
	{ "places/max-items",            Tab::Places,      EnableWhen::PlacesEnabled,           false, Column::C2 },
	{ "places/remember-last-mode",   Tab::Places,      EnableWhen::PlacesEnabled,           false, Column::C1 },
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
	"menu-opacity", "stay-on-focus-out",
	"profile-position", "profile-shape", "commands-position",
	"confirm-session-command",
	"search-bar-position",
	"fuzzy-enabled", "fuzzy-threshold",
	"favorites-boost-enabled", "favorites-boost-level",
	"frecency-alpha",
	"view-mode", "grid-density",
	"launcher-icon-size", "launcher-show-name",
	"launcher-show-tooltip", "launcher-show-description",
	"category-show-name", "category-icon-size",
	"sidebar-position",
	"category-hover-activate", "sort-categories",
	"default-category", "recent-items-max", "favorites-in-recent",
	// Places milestone-005 controls
	"places/enabled", "places/history-enabled", "places/favourites-enabled",
	"places/favourite-sync", "places/max-items",
	"places/remember-last-mode",
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

// Independent reference for the per-tab C1/C2/Full placement map, transcribed by
// hand from contracts/general-tab-placement.md (General) and
// contracts/tab-placement.md (Search Bar, Results View / app-grid, Sidebar,
// Places, User/Session). Kept separate from kPlacementGrid so the assertion
// below is a genuine cross-check, not a tautology: a wrong column in the grid
// (or here) fails the test.
struct ColumnExpectation
{
	const char* setting_id;
	Column      column;
};

const ColumnExpectation kColumnContract[] = {
	// General — every keyed control pins a C1/C2 half (never Full; only the
	// keyless FullScreen info-bar spans both halves).
	{ "current-preset-id",         Column::C1 },
	{ "button-title-visible",      Column::C1 },
	{ "button-title",              Column::C1 },
	{ "button-icon-visible",       Column::C2 },
	{ "button-icon-name",          Column::C2 },
	{ "button-single-row",         Column::C1 },
	{ "layout-mode",               Column::C1 },
	{ "panel-gap",                 Column::C1 },
	{ "menu-width",                Column::C1 },
	{ "menu-height",               Column::C2 },
	{ "corner-radius",             Column::C2 },
	{ "menu-opacity",              Column::C1 },
	{ "stay-on-focus-out",         Column::C2 },

	// User / Session — single column; every keyed row spans the section.
	{ "profile-position",          Column::Full },
	{ "profile-shape",             Column::Full },
	{ "commands-position",         Column::Full },
	{ "confirm-session-command",   Column::Full },

	// Search Bar — position C1; fuzzy switch C1 / threshold C2; boost switch C1 /
	// level C2; recency-weight slider is whole-row (Full).
	{ "search-bar-position",       Column::C1 },
	{ "fuzzy-enabled",             Column::C1 },
	{ "fuzzy-threshold",           Column::C2 },
	{ "favorites-boost-enabled",   Column::C1 },
	{ "favorites-boost-level",     Column::C2 },
	{ "frecency-alpha",            Column::Full },

	// Results View (app-grid) — view selector unchanged (Full); the Layout
	// section splits across C1/C2. The former opacity slider was removed.
	{ "view-mode",                 Column::Full },
	{ "grid-density",              Column::C1 },
	{ "launcher-icon-size",        Column::C2 },
	{ "launcher-show-name",        Column::C1 },
	{ "launcher-show-tooltip",     Column::C2 },
	{ "launcher-show-description", Column::C1 },

	// Sidebar — Default-category radios are whole-row (Full); the rest split
	// across C1/C2. The former opacity slider was removed.
	{ "category-show-name",        Column::C1 },
	{ "category-icon-size",        Column::C2 },
	{ "sidebar-position",          Column::C1 },
	{ "category-hover-activate",   Column::C1 },
	{ "sort-categories",           Column::C2 },
	{ "default-category",          Column::Full },
	{ "recent-items-max",          Column::C1 },
	{ "favorites-in-recent",       Column::C2 },

	// Places — enable C1 / show-icons C2 (show-icons is off-grid, see above);
	// history C1 / favourites C2; sync C1 / max C2; remember-last-mode C1.
	{ "places/enabled",            Column::C1 },
	{ "places/history-enabled",    Column::C1 },
	{ "places/favourites-enabled", Column::C2 },
	{ "places/favourite-sync",     Column::C1 },
	{ "places/max-items",          Column::C2 },
	{ "places/remember-last-mode", Column::C1 },
};

constexpr size_t kColumnContractCount =
	sizeof(kColumnContract) / sizeof(kColumnContract[0]);

// Invariant 6 (US-1 placement map): every keyed Properties control places into
// the column the contract reference names, and the reference covers exactly the
// keyed grid — neither table may drift ahead of the other (SC-001 / FR-012…
// FR-030). General keyed rows additionally must never be Full (only the keyless
// info-bar spans both halves there).
static void test_columns_match_contract()
{
	for (const auto& row : kPlacementGrid)
	{
		if (row.tab == Tab::General)
			assert(row.column != Column::Full
					&& "General control has no C1/C2 column assigned");

		bool found = false;
		for (size_t i = 0; i < kColumnContractCount; ++i)
		{
			if (std::string(row.setting_id) == kColumnContract[i].setting_id)
			{
				assert(row.column == kColumnContract[i].column
						&& "control column does not match the placement contract");
				found = true;
				break;
			}
		}
		assert(found && "control missing from the column contract reference");
	}

	// And the reverse: every contracted key is actually on the grid, so the
	// reference table cannot silently drift ahead of the placement grid.
	std::set<std::string> grid_keys;
	for (const auto& row : kPlacementGrid)
		grid_keys.insert(row.setting_id);
	for (size_t i = 0; i < kColumnContractCount; ++i)
		assert(grid_keys.count(kColumnContract[i].setting_id) == 1
				&& "column contract names a key absent from the placement grid");
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
	test_columns_match_contract();
	test_synced_keys_cover_governed_keys();
	return 0;
}
