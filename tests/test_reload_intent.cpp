/*
 * Exhaustive headless coverage for the Settings reload-intent classifier.
 */

#include "settings.h"

#include <cassert>
#include <cstddef>
#include <set>
#include <string>

using namespace WhiskerMenu;

namespace
{

struct IntentCase
{
	const char* property;
	ReloadIntent expected;
};

/* assert_exact_rules:
 * @cases: independently maintained active-setting inventory.
 * @count: number of exact property rules in @cases.
 *
 * Verifies that every active scalar/list setting has an explicit expected
 * result and that the inventory contains no duplicate path which could hide a
 * conflicting classification.
 */
void assert_exact_rules(const IntentCase* cases, std::size_t count)
{
	std::set<std::string> properties;
	for (std::size_t i = 0; i < count; ++i)
	{
		assert(cases[i].property && cases[i].property[0] == '/');
		assert(properties.insert(cases[i].property).second);
		assert(classify_reload_intent(cases[i].property) == cases[i].expected);
	}
}

}

int main()
{
	const IntentCase active_settings[] = {
		{ "/favorites", ReloadIntent::Content },
		{ "/recent", ReloadIntent::Content },
		{ "/custom-menu-file", ReloadIntent::Content },
		{ "/button-title", ReloadIntent::Button },
		{ "/button-icon", ReloadIntent::Button },
		{ "/show-button-title", ReloadIntent::Button },
		{ "/show-button-icon", ReloadIntent::Button },
		{ "/button-single-row", ReloadIntent::Button },
		{ "/launcher-show-name", ReloadIntent::Content },
		{ "/launcher-show-description", ReloadIntent::Content },
		{ "/launcher-show-tooltip", ReloadIntent::Layout },
		{ "/transparent-grid", ReloadIntent::Layout },
		{ "/launcher-icon-size", ReloadIntent::Layout },
		{ "/hover-switch-category", ReloadIntent::None },
		{ "/category-show-name", ReloadIntent::Layout },
		{ "/sort-categories", ReloadIntent::Content },
		{ "/category-icon-size", ReloadIntent::Layout },
		{ "/view-mode", ReloadIntent::Content },
		{ "/default-category", ReloadIntent::Layout },
		{ "/recent-items-max", ReloadIntent::Layout },
		{ "/favorites-in-recent", ReloadIntent::Content },
		{ "/stay-on-focus-out", ReloadIntent::Layout },
		{ "/profile-shape", ReloadIntent::Layout },
		{ "/confirm-session-command", ReloadIntent::Layout },
		{ "/search-actions", ReloadIntent::None },
		{ "/search/fuzzy-enabled", ReloadIntent::None },
		{ "/search/fuzzy-threshold", ReloadIntent::None },
		{ "/search/favorites-boost-enabled", ReloadIntent::None },
		{ "/search/favorites-boost-level", ReloadIntent::None },
		{ "/search/frecency-alpha", ReloadIntent::None },
		{ "/search/aliases", ReloadIntent::None },
		{ "/menu-width", ReloadIntent::Layout },
		{ "/menu-height", ReloadIntent::Layout },
		{ "/menu-opacity", ReloadIntent::Layout },
		{ "/schema-version", ReloadIntent::None },
		{ "/current-preset-id", ReloadIntent::Layout },
		{ "/initialized", ReloadIntent::None },
		{ "/corner-radius", ReloadIntent::Layout },
		{ "/panel-gap", ReloadIntent::Layout },
		{ "/sidebar-position", ReloadIntent::Layout },
		{ "/sidebar-enabled", ReloadIntent::Layout },
		{ "/search-bar-position", ReloadIntent::Layout },
		{ "/show-profile", ReloadIntent::Layout },
		{ "/show-session", ReloadIntent::Layout },
		{ "/grid-density", ReloadIntent::Layout },
		{ "/layout-mode", ReloadIntent::Layout },
		{ "/places/enabled", ReloadIntent::Layout },
		{ "/places/history-enabled", ReloadIntent::Layout },
		{ "/places/favourites-enabled", ReloadIntent::Layout },
		{ "/places/favourite-sync", ReloadIntent::Layout },
		{ "/places/max-items", ReloadIntent::Layout },
		{ "/places/remember-last-mode", ReloadIntent::Layout },
		{ "/places/last-mode", ReloadIntent::Layout },
		{ "/places/favourites", ReloadIntent::Layout },
		{ "/places/switch-show-icons", ReloadIntent::Layout },
		{ "/extras/calculator-engine", ReloadIntent::Content },
		{ "/extras/calculator-result-font-size", ReloadIntent::Content },
		{ "/extras/calculator-max-decimal-places", ReloadIntent::Content },
	};
	assert_exact_rules(active_settings,
			sizeof(active_settings) / sizeof(active_settings[0]));

	for (const char* property : {
		"/command-settings", "/command-lockscreen", "/command-switchuser",
		"/command-logoutuser", "/command-restart", "/command-shutdown",
		"/command-suspend", "/command-hibernate", "/command-logout",
		"/command-menueditor", "/command-profile",
		"/show-command-settings", "/show-command-lockscreen",
		"/show-command-switchuser", "/show-command-logoutuser",
		"/show-command-restart", "/show-command-shutdown",
		"/show-command-suspend", "/show-command-hibernate",
		"/show-command-logout", "/show-command-menueditor",
		"/show-command-profile",
	})
	{
		assert(classify_reload_intent(property) == ReloadIntent::Layout);
	}

	for (const char* property : {
		"/search-actions/action-0/name",
		"/search-actions/action-2/pattern",
		"/search-actions/action-4/command",
		"/search-actions/action-5/regex",
		"/search/aliases/org.example.App.desktop/0",
	})
	{
		assert(classify_reload_intent(property) == ReloadIntent::None);
	}

	for (const char* retired : {
		"/position-profile-alternate", "/position-search-alternate",
		"/position-commands-alternate", "/position-categories-alternate",
		"/position-categories-horizontal", "/profile-position",
		"/commands-position", "/unified-bar",
		"/places/switch-button-shape",
	})
	{
		assert(classify_reload_intent(retired) == ReloadIntent::None);
	}

	assert(classify_reload_intent("/unknown-setting") == ReloadIntent::None);
	assert(classify_reload_intent("command-settings") == ReloadIntent::None);
	assert(classify_reload_intent(nullptr) == ReloadIntent::None);
	return 0;
}
