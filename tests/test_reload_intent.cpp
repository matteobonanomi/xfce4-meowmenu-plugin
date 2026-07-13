/*
 * Headless coverage for the Settings reload-intent classifier.
 */

#include "settings.h"

#include <cassert>

using namespace WhiskerMenu;

static void expect_intent(const char* property, ReloadIntent expected)
{
	assert(classify_reload_intent(property) == expected);
}

int main()
{
	expect_intent("/profile-position", ReloadIntent::Layout);
	expect_intent("/commands-position", ReloadIntent::Layout);
	expect_intent("/search-bar-position", ReloadIntent::Layout);
	expect_intent("/sidebar-position", ReloadIntent::Layout);
	expect_intent("/sidebar-enabled", ReloadIntent::Layout);
	expect_intent("/category-show-name", ReloadIntent::Layout);
	expect_intent("/category-icon-size", ReloadIntent::Layout);
	expect_intent("/menu-width", ReloadIntent::Layout);
	expect_intent("/menu-height", ReloadIntent::Layout);
	expect_intent("/menu-opacity", ReloadIntent::Layout);
	expect_intent("/corner-radius", ReloadIntent::Layout);
	expect_intent("/panel-gap", ReloadIntent::Layout);
	expect_intent("/places/enabled", ReloadIntent::Layout);
	expect_intent("/places/switch-show-icons", ReloadIntent::Layout);
	expect_intent("/places/switch-button-shape", ReloadIntent::Layout);
	expect_intent("/transparent-grid", ReloadIntent::Layout);
	expect_intent("/show-command-lockscreen", ReloadIntent::Layout);

	expect_intent("/button-title", ReloadIntent::Button);
	expect_intent("/button-icon", ReloadIntent::Button);
	expect_intent("/show-button-title", ReloadIntent::Button);

	expect_intent("/custom-menu-file", ReloadIntent::Content);
	expect_intent("/favorites", ReloadIntent::Content);
	expect_intent("/recent", ReloadIntent::Content);
	expect_intent("/sort-categories", ReloadIntent::Content);
	expect_intent("/view-mode", ReloadIntent::Content);
	expect_intent("/launcher-show-name", ReloadIntent::Content);
	expect_intent("/launcher-show-description", ReloadIntent::Content);
	expect_intent("/favorites-in-recent", ReloadIntent::Content);

	expect_intent("/search/fuzzy-enabled", ReloadIntent::None);
	expect_intent("/current-preset-id", ReloadIntent::None);
	expect_intent(nullptr, ReloadIntent::None);

	return 0;
}
