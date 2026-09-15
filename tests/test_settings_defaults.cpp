/*
 * Characterization of runtime and migration-seed setting defaults.
 */

#include "settings.h"

#include "core/plugin.h"
#include "private-xfconf-fixture.h"
#include "settings-defaults.h"

#include <cassert>
#include <cstdio>
#include <string>

#include <xfconf/xfconf.h>

using namespace WhiskerMenu;

namespace
{

enum class DefaultClass
{
	Ordinary,
	PresetDerived,
	HistoricalOnly,
	CalculatorDivergence
};

struct DefaultCase
{
	const char* property;
	std::string runtime_value;
	const char* migration_seed;
	DefaultClass classification;
};

std::string boolean_value(bool value)
{
	return value ? "true" : "false";
}

std::string integer_value(int value)
{
	return std::to_string(value);
}

/* assert_default_matrix:
 * @settings: newly constructed Settings object, before an Xfconf load.
 *
 * Freezes the two existing default authorities before they are unified. Values
 * classified as ordinary must agree byte-for-byte; the remaining rows name
 * intentional preset, migration-history, and Calculator exceptions.
 */
void assert_default_matrix(Settings& settings)
{
	const DefaultCase cases[] = {
		{ "/corner-radius", integer_value(settings.corner_radius), "0", DefaultClass::Ordinary },
		{ "/panel-gap", integer_value(settings.panel_gap), "0", DefaultClass::Ordinary },
		{ "/sidebar-position", static_cast<const char*>(settings.sidebar_position), "left", DefaultClass::Ordinary },
		{ "/sidebar-enabled", boolean_value(settings.sidebar_enabled), "true", DefaultClass::Ordinary },
		{ "/search-bar-position", static_cast<const char*>(settings.search_bar_position), "top", DefaultClass::Ordinary },
		{ "/grid-density", static_cast<const char*>(settings.grid_density), "medium", DefaultClass::Ordinary },
		{ "/layout-mode", static_cast<const char*>(settings.layout_mode), "docked", DefaultClass::Ordinary },
		{ "/places/enabled", boolean_value(settings.places_enabled), "false", DefaultClass::Ordinary },
		{ "/places/history-enabled", boolean_value(settings.places_history_enabled), "true", DefaultClass::Ordinary },
		{ "/places/favourites-enabled", boolean_value(settings.places_favourites_enabled), "true", DefaultClass::Ordinary },
		{ "/places/favourite-sync", static_cast<const char*>(settings.places_favourite_sync), "meowmenu", DefaultClass::Ordinary },
		{ "/places/max-items", integer_value(settings.places_max_items), "20", DefaultClass::Ordinary },
		{ "/places/remember-last-mode", boolean_value(settings.places_remember_last_mode), "false", DefaultClass::Ordinary },
		{ "/places/last-mode", static_cast<const char*>(settings.places_last_mode), "apps", DefaultClass::Ordinary },
		{ "/transparent-grid", boolean_value(settings.transparent_grid), "false", DefaultClass::Ordinary },
		{ "/show-profile", boolean_value(settings.show_profile), "true", DefaultClass::Ordinary },
		{ "/show-session", boolean_value(settings.show_session), "true", DefaultClass::Ordinary },
		{ "/extras/calculator-result-font-size", integer_value(settings.calculator_result_font_size), "-1", DefaultClass::Ordinary },
		{ "/extras/calculator-max-decimal-places", integer_value(settings.calculator_max_decimal_places), "4", DefaultClass::Ordinary },

		{ "/menu-opacity", integer_value(settings.menu_opacity), "active-preset-or-100", DefaultClass::PresetDerived },
		{ "/places/switch-show-icons", boolean_value(settings.places_switch_show_icons), "active-preset-or-false", DefaultClass::PresetDerived },
		{ "/current-preset-id", static_cast<const char*>(settings.current_preset_id), "fresh-or-upgrade-derived", DefaultClass::PresetDerived },

		{ "/categories-opacity", "no-runtime-setting", "historical-v1-only", DefaultClass::HistoricalOnly },
		{ "/apps-opacity", "no-runtime-setting", "historical-v1-only", DefaultClass::HistoricalOnly },
		{ "/full-screen-opacity", "no-runtime-setting", "historical-v2-only", DefaultClass::HistoricalOnly },
		{ "/profile-position", "no-runtime-setting", "historical-upgrade-only", DefaultClass::HistoricalOnly },

		{ "/extras/calculator-engine", static_cast<const char*>(settings.calculator_engine), "bc-for-nonclassic-builtins-otherwise-none", DefaultClass::CalculatorDivergence },
	};

	unsigned int ordinary_count = 0;
	unsigned int preset_count = 0;
	unsigned int historical_count = 0;
	unsigned int calculator_count = 0;
	for (const DefaultCase& entry : cases)
	{
		assert(entry.property && *entry.property);
		switch (entry.classification)
		{
		case DefaultClass::Ordinary:
			assert(entry.runtime_value == entry.migration_seed);
			++ordinary_count;
			break;
		case DefaultClass::PresetDerived:
			assert(entry.runtime_value != entry.migration_seed);
			++preset_count;
			break;
		case DefaultClass::HistoricalOnly:
			assert(entry.runtime_value == "no-runtime-setting");
			++historical_count;
			break;
		case DefaultClass::CalculatorDivergence:
			assert(entry.runtime_value == "none");
			assert(std::string(entry.migration_seed)
					== "bc-for-nonclassic-builtins-otherwise-none");
			++calculator_count;
			break;
		}
	}

	assert(ordinary_count == 19);
	assert(preset_count == 3);
	assert(historical_count == 4);
	assert(calculator_count == 1);
}

/* assert_descriptor_policy:
 *
 * Verifies the typed defaults retain the runtime validation bounds as well as
 * the values shared with migration seeding. Exceptional seeds stay named and
 * distinct instead of being folded into the ordinary descriptor set.
 */
void assert_descriptor_policy()
{
	assert(std::string(DEFAULT_CORNER_RADIUS.property) == "/corner-radius");
	assert(DEFAULT_CORNER_RADIUS.value == 0);
	assert(DEFAULT_CORNER_RADIUS.minimum == 0);
	assert(DEFAULT_CORNER_RADIUS.maximum == 24);
	assert(!DEFAULT_CORNER_RADIUS.reject_to_default);

	assert(std::string(DEFAULT_PANEL_GAP.property) == "/panel-gap");
	assert(DEFAULT_PANEL_GAP.value == 0);
	assert(DEFAULT_PANEL_GAP.minimum == 0);
	assert(DEFAULT_PANEL_GAP.maximum == 50);

	assert(std::string(DEFAULT_PLACES_MAX_ITEMS.property) == "/places/max-items");
	assert(DEFAULT_PLACES_MAX_ITEMS.value == 20);
	assert(DEFAULT_PLACES_MAX_ITEMS.minimum == 0);
	assert(DEFAULT_PLACES_MAX_ITEMS.maximum == 30);

	assert(DEFAULT_CALCULATOR_RESULT_FONT_SIZE.value == -1);
	assert(DEFAULT_CALCULATOR_RESULT_FONT_SIZE.minimum == -1);
	assert(DEFAULT_CALCULATOR_RESULT_FONT_SIZE.maximum == 6);
	assert(DEFAULT_CALCULATOR_RESULT_FONT_SIZE.reject_to_default);
	assert(DEFAULT_CALCULATOR_MAX_DECIMAL_PLACES.value == 4);
	assert(DEFAULT_CALCULATOR_MAX_DECIMAL_PLACES.minimum == 0);
	assert(DEFAULT_CALCULATOR_MAX_DECIMAL_PLACES.maximum == 10);
	assert(DEFAULT_CALCULATOR_MAX_DECIMAL_PLACES.reject_to_default);

	assert(HISTORICAL_CATEGORIES_OPACITY_SEED == 100);
	assert(HISTORICAL_APPS_OPACITY_SEED == 100);
	assert(HISTORICAL_FULL_SCREEN_OPACITY_SEED == 100);
	assert(PRESET_MENU_OPACITY_SEED_FALLBACK == 100);
	assert(!PRESET_SWITCH_SHOW_ICONS_SEED_FALLBACK);
	assert(std::string(CALCULATOR_ENGINE_RUNTIME_DEFAULT) == "none");
	assert(std::string(CALCULATOR_ENGINE_NONCLASSIC_SEED) == "bc");
}

}

static int run_test(int argc, char** argv)
{
	if (!gtk_init_check(&argc, &argv))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77;
	}

	XfcePanelPlugin* host = XFCE_PANEL_PLUGIN(g_object_new(
			XFCE_TYPE_PANEL_PLUGIN,
			"name", "meowmenu",
			"display-name", "MeowMenu",
			"comment", "Settings defaults test host",
			"unique-id", 9008,
			nullptr));
	assert(host);
	assert(xfconf_init(nullptr));
	XfconfChannel* channel = xfconf_channel_new_with_property_base(
			xfce_panel_get_channel_name(),
			xfce_panel_plugin_get_property_base(host));
	assert(channel);
	assert(xfconf_channel_set_bool(channel, "/initialized", true));
	assert(xfconf_channel_set_int(channel, "/schema-version", 13));
	g_object_unref(channel);

	Plugin* plugin = new Plugin(host);
	assert_descriptor_policy();
	assert_default_matrix(*plugin->get_settings());
	g_signal_emit_by_name(host, "free-data");
	std::printf("test_settings_defaults: ok\n");
	return 0;
}

int main(int argc, char** argv)
{
	return run_with_private_xfconf(argc, argv, &run_test);
}
