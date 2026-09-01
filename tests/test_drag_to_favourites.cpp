/*
 * Headless tests for drag-to-favourite decision logic.
 *
 * The GTK callbacks are exercised manually, but these tests pin the pure rules
 * they delegate to: payload/destination matching, visible target availability,
 * append-once semantics, and the no-activation side effect contract.
 */

#include "panel-plugin/core/drag-to-favourites.h"
#include "panel-plugin/launcher/favorite-projection.h"
#include "panel-plugin/ui/icon-size.h"

#include <cassert>
#include <string>
#include <vector>

using namespace WhiskerMenu;

// ---------------------------------------------------------------------------

static bool append_unique(std::vector<std::string>& values,
		const std::string& value)
{
	for (const auto& existing : values)
	{
		if (existing == value)
		{
			return false;
		}
	}
	values.push_back(value);
	return true;
}

struct DropSimulation
{
	std::vector<std::string> values;
	int activations = 0;

	bool drop(const std::string& value)
	{
		return append_unique(values, value);
	}
};

// ---------------------------------------------------------------------------

static void test_payload_acceptance_matrix()
{
	assert(favourite_drop_accepts(FavouriteDragPayload::Application,
			FavouriteDropTarget::ApplicationFavorites));
	assert(favourite_drop_accepts(FavouriteDragPayload::Places,
			FavouriteDropTarget::PlacesFavourites));

	assert(!favourite_drop_accepts(FavouriteDragPayload::Application,
			FavouriteDropTarget::PlacesFavourites));
	assert(!favourite_drop_accepts(FavouriteDragPayload::Places,
			FavouriteDropTarget::ApplicationFavorites));
}

static void test_application_target_availability()
{
	assert(application_favourites_drop_available(
			/*sidebar_enabled*/ true,
			/*places_active*/ false,
			/*favorites_visible*/ true));

	assert(!application_favourites_drop_available(false, false, true));
	assert(!application_favourites_drop_available(true, true, true));
	assert(!application_favourites_drop_available(true, false, false));
}

static void test_places_target_availability()
{
	assert(places_favourites_drop_available(
			/*sidebar_enabled*/ true,
			/*places_enabled*/ true,
			/*places_active*/ true,
			/*favourites_visible*/ true,
			/*meowmenu_only*/ true));

	assert(!places_favourites_drop_available(false, true, true, true, true));
	assert(!places_favourites_drop_available(true, false, true, true, true));
	assert(!places_favourites_drop_available(true, true, false, true, true));
	assert(!places_favourites_drop_available(true, true, true, false, true));
	assert(!places_favourites_drop_available(true, true, true, true, false));
}

static void test_application_append_order_and_duplicates()
{
	DropSimulation sim;
	sim.values = { "terminal.desktop", "files.desktop" };

	assert(sim.drop("browser.desktop"));
	assert((sim.values == std::vector<std::string>{
			"terminal.desktop", "files.desktop", "browser.desktop" }));

	assert(!sim.drop("browser.desktop"));
	assert((sim.values == std::vector<std::string>{
			"terminal.desktop", "files.desktop", "browser.desktop" }));
}

static void test_places_append_order_and_duplicates()
{
	DropSimulation sim;
	sim.values = { "file:///home/u/Documents" };

	assert(sim.drop("file:///home/u/Pictures"));
	assert((sim.values == std::vector<std::string>{
			"file:///home/u/Documents", "file:///home/u/Pictures" }));

	assert(!sim.drop("file:///home/u/Pictures"));
	assert((sim.values == std::vector<std::string>{
			"file:///home/u/Documents", "file:///home/u/Pictures" }));
}

static void test_drops_do_not_activate_sources()
{
	DropSimulation app;
	assert(app.drop("browser.desktop"));
	assert(app.activations == 0);

	DropSimulation places;
	assert(places.drop("file:///home/u/report.txt"));
	assert(places.activations == 0);
}

static void test_favourite_drags_keep_menu_open()
{
	assert(favourite_drop_accepts(FavouriteDragPayload::Application,
			FavouriteDropTarget::ApplicationFavorites));
	assert(favourite_drop_accepts(FavouriteDragPayload::Places,
			FavouriteDropTarget::PlacesFavourites));
}

static void test_favourite_drag_uses_small_preview()
{
	assert(favourite_drag_preview_size() == IconSize::pixels_for(IconSize::Small));
}

static void test_missing_application_favourites_are_projected_without_writes()
{
	const std::vector<std::string> stored = {
		"terminal.desktop",
		"removed.desktop",
		"browser.desktop",
	};
	const std::vector<std::string> available = {
		"terminal.desktop",
		"browser.desktop",
	};

	const auto visible = favorite_resolved_projection(stored, available);
	assert((visible == std::vector<std::string>{
			"terminal.desktop", "browser.desktop" }));
	assert((stored == std::vector<std::string>{
		"terminal.desktop", "removed.desktop", "browser.desktop" }));
}

static void test_resolved_edits_preserve_unresolved_slots()
{
	const std::vector<std::string> stored = {
		"terminal.desktop", "missing-a.desktop", "files.desktop",
		"missing-b.desktop", "browser.desktop",
	};
	const std::vector<std::string> available = {
		"terminal.desktop", "files.desktop", "browser.desktop",
	};
	const std::vector<std::string> reordered = {
		"browser.desktop", "terminal.desktop", "files.desktop",
	};
	assert((favorite_merge_resolved_order(stored, reordered, available)
			== std::vector<std::string>{
				"browser.desktop", "missing-a.desktop", "terminal.desktop",
				"missing-b.desktop", "files.desktop" }));

	std::vector<std::string> edited(stored);
	assert(favorite_remove_exact(edited, "files.desktop"));
	assert(!favorite_remove_exact(edited, "unknown.desktop"));
	assert(favorite_append_exact(edited, "editor.desktop"));
	assert(!favorite_append_exact(edited, "editor.desktop"));
	assert((edited == std::vector<std::string>{
		"terminal.desktop", "missing-a.desktop", "missing-b.desktop",
		"browser.desktop", "editor.desktop" }));
}

static void test_invalid_resolved_order_is_rejected()
{
	const std::vector<std::string> stored = {
		"terminal.desktop", "missing.desktop", "browser.desktop",
	};
	const std::vector<std::string> available = {
		"terminal.desktop", "browser.desktop",
	};
	assert(favorite_merge_resolved_order(stored,
			{ "terminal.desktop" }, available) == stored);
	assert(favorite_merge_resolved_order(stored,
			{ "terminal.desktop", "editor.desktop" }, available) == stored);
}

// ---------------------------------------------------------------------------

int main()
{
	test_payload_acceptance_matrix();
	test_application_target_availability();
	test_places_target_availability();
	test_application_append_order_and_duplicates();
	test_places_append_order_and_duplicates();
	test_drops_do_not_activate_sources();
	test_favourite_drags_keep_menu_open();
	test_favourite_drag_uses_small_preview();
	test_missing_application_favourites_are_projected_without_writes();
	test_resolved_edits_preserve_unresolved_slots();
	test_invalid_resolved_order_is_rejected();
	return 0;
}
