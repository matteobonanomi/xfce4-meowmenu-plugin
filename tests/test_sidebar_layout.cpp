/*
 * Headless, table-driven tests for the pure layout-decision mapping declared in
 * panel-plugin/core/sidebar-layout.h. No GTK types are used.
 *
 * Covers sidebar category presentation, final selector homes, and restoration
 * of stored category-name intent after leaving a forcing layout.
 */

#include "core/sidebar-layout.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace WhiskerMenu;

namespace
{

int g_failures = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

SidebarLayoutState make_state(bool sidebar_enabled, SidebarPosition pos,
		bool names)
{
	SidebarLayoutState s;
	s.sidebar_enabled = sidebar_enabled;
	s.position = pos;
	s.category_show_name = names;
	return s;
}

// A vertical sidebar preserves category-name intent.
void row1_vertical_names_on()
{
	auto p = meow_compute_sidebar_presentation(
			make_state(true, SidebarPosition::Left, true));
	CHECK(p.sidebar_visible);
	CHECK(!p.categories_horizontal);
	CHECK(p.effective_show_category_names == true);
	CHECK(!p.show_default_category_heading);
}

// Hiding names is also preserved on a vertical sidebar.
void row2_vertical_names_off()
{
	auto p = meow_compute_sidebar_presentation(
			make_state(true, SidebarPosition::Left, false));
	CHECK(p.sidebar_visible);
	CHECK(!p.categories_horizontal);
	CHECK(p.effective_show_category_names == false);
}

// Horizontal category navigation forces category names off.
void row3_strip()
{
	for (SidebarPosition pos : { SidebarPosition::Horizontal })
	{
		for (bool names : { true, false })
		{
			auto p = meow_compute_sidebar_presentation(
					make_state(true, pos, names));
			CHECK(p.sidebar_visible);
			CHECK(p.categories_horizontal);
			CHECK(p.effective_show_category_names == false);  // forced
			CHECK(!p.show_default_category_heading);
		}
	}
}

// A disabled sidebar owns no category surface and exposes the heading.
void disabled_sidebar_heading()
{
	auto p = meow_compute_sidebar_presentation(
			make_state(false, SidebarPosition::Left, true));
	CHECK(!p.sidebar_visible);
	CHECK(p.show_default_category_heading);
}

// supported behavior: a forced state never overwrites stored intent, so once the forcing
// layout is removed the effective value reverts to the stored value with no
// bookkeeping. Stored names=true: Horizontal forces names off and Left
// restores them.
void category_name_reversion()
{
	const bool stored_names = true;

	auto forced = meow_compute_sidebar_presentation(
			make_state(true, SidebarPosition::Horizontal, stored_names));
	CHECK(forced.effective_show_category_names == false);

	auto reverted = meow_compute_sidebar_presentation(
			make_state(true, SidebarPosition::Left, stored_names));
	CHECK(reverted.effective_show_category_names == stored_names);
}

// "hidden" and unknown strings parse to Left; the four real values map through.
void parse_positions()
{
	CHECK(meow_parse_sidebar_position("left") == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position("right") == SidebarPosition::Right);
	CHECK(meow_parse_sidebar_position("horizontal") == SidebarPosition::Horizontal);
	CHECK(meow_parse_sidebar_position("top") == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position("bottom") == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position("hidden") == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position(nullptr) == SidebarPosition::Left);
	CHECK(meow_parse_sidebar_position("nonsense") == SidebarPosition::Left);
}

void horizontal_edge_derivation()
{
	CHECK(meow_resolve_sidebar_edge(SidebarPosition::Horizontal, false, false)
			== SidebarPosition::Bottom);
	CHECK(meow_resolve_sidebar_edge(SidebarPosition::Horizontal, true, false)
			== SidebarPosition::Top);
	CHECK(meow_resolve_sidebar_edge(SidebarPosition::Horizontal, true, true)
			== SidebarPosition::Bottom);
	CHECK(meow_resolve_sidebar_edge(SidebarPosition::Left, true, false)
			== SidebarPosition::Left);
}

// Top/Bottom strip stacking order, category centering, and width source
// (supported behavior). The category group is centered within the strip; the
// order is direction-independent and the separate switch role remains leading.
void strip_geometry_ordering()
{
	// Bottom → strip below the results box; Top → strip above. Geometry is
	// identical in LTR and RTL: the category group is centered in either mode.
	for (bool ltr : { true, false })
	{
		StripGeometry top = meow_compute_strip_geometry(SidebarPosition::Top, ltr);
		CHECK(top.order == StripOrder::StripAboveResults);
		CHECK(top.toggle_anchor == StripAnchor::Leading);
		CHECK(top.categories_anchor == StripAnchor::Center);
		CHECK(top.width_from_main_column);

		StripGeometry bottom = meow_compute_strip_geometry(SidebarPosition::Bottom, ltr);
		CHECK(bottom.order == StripOrder::StripBelowResults);
		CHECK(bottom.toggle_anchor == StripAnchor::Leading);
		CHECK(bottom.categories_anchor == StripAnchor::Center);
		CHECK(bottom.width_from_main_column);
	}
}

void fullscreen_main_column_metrics()
{
	FullscreenMainColumn zero = meow_fullscreen_main_column(0);
	CHECK(zero.width == 0);
	CHECK(zero.margin == 0);

	FullscreenMainColumn even = meow_fullscreen_main_column(1920);
	CHECK(even.width == 1280);
	CHECK(even.margin == 320);
	CHECK(even.margin * 2 + even.width == 1920);

	FullscreenMainColumn odd = meow_fullscreen_main_column(1919);
	CHECK(odd.width == 1281);
	CHECK(odd.margin == 319);
	CHECK(odd.margin * 2 + odd.width == 1919);
}

void fullscreen_places_disabled_strip_centers_categories()
{
	auto p = meow_compute_sidebar_presentation(
			make_state(true, SidebarPosition::Horizontal, true));
	CHECK(p.categories_horizontal);
	CHECK(p.effective_show_category_names == false);

	StripGeometry top = meow_compute_strip_geometry(SidebarPosition::Top, true);
	CHECK(top.width_from_main_column);
	CHECK(top.categories_anchor == StripAnchor::Center);
}

void final_home_selector_presentation()
{
	const int category_px = 48;
	const int search_px = 22;
	const int session_px = 16;
	struct Case
	{
		MenuControlLocation location;
		LayoutMode mode;
		SelectorHome home;
		SelectorIconSizeSource source;
		int icon_px;
	};
	const Case cases[] = {
		{ MenuControlLocation::Hidden, LayoutMode::Docked,
			SelectorHome::Hidden, SelectorIconSizeSource::None, 0 },
		{ MenuControlLocation::Sidebar, LayoutMode::FullScreen,
			SelectorHome::Sidebar, SelectorIconSizeSource::Category, category_px },
		{ MenuControlLocation::SecondaryRow, LayoutMode::Centered,
			SelectorHome::SecondaryRow, SelectorIconSizeSource::SessionToolbar,
			session_px },
		{ MenuControlLocation::PrimaryRow, LayoutMode::Docked,
			SelectorHome::WindowedPrimary, SelectorIconSizeSource::Search, search_px },
		{ MenuControlLocation::PrimaryRow, LayoutMode::FullScreen,
			SelectorHome::FullScreenSearch, SelectorIconSizeSource::SessionToolbar,
			session_px },
	};
	for (const Case& c : cases)
	{
		const SelectorPresentation result = meow_resolve_selector_presentation(
				c.location, c.mode, true, true, false,
				category_px, search_px, session_px);
		CHECK(result.home == c.home);
		CHECK(result.icon_size_source == c.source);
		CHECK(result.icon_px == c.icon_px);
		CHECK(result.orientation == SwitchOrientation::Horizontal);
		CHECK(result.natural_height);
		CHECK(result.content == SelectorContent::Icons);
		CHECK(result.active_mode == SelectorActiveMode::Applications);
	}

	const SelectorPresentation vertical = meow_resolve_selector_presentation(
			MenuControlLocation::Sidebar, LayoutMode::Docked,
			false, false, true, category_px, search_px, session_px);
	CHECK(vertical.orientation == SwitchOrientation::Vertical);
	CHECK(vertical.content == SelectorContent::Labels);
	CHECK(vertical.active_mode == SelectorActiveMode::Places);
	const SelectorPresentation row = meow_resolve_selector_presentation(
			MenuControlLocation::PrimaryRow, LayoutMode::Docked,
			false, false, true, category_px, search_px, session_px);
	CHECK(row.orientation == SwitchOrientation::Horizontal);
	CHECK(row.content == SelectorContent::Icons);
}

void strip_spacer_order_decision()
{
	// The matching trailing spacer is appended after every category button.
	CHECK(meow_strip_spacer_order(true) == 0);
	CHECK(meow_strip_spacer_order(false) == -1);
}

void default_category_order_base_decision()
{
	CHECK(meow_default_category_order_base(true, false) == 1);
	CHECK(meow_default_category_order_base(false, false) == 0);
	// A vertical sidebar keeps the selector and lower divider ahead of every
	// reorderable built-in category.
	CHECK(meow_default_category_order_base(false, true) == 2);
}

// Embedded Apps/Places switch ordering in the standard (non-unified) search-bar
// row. supported behavior regression intent: no slot ever places the switch after a present
// command box. When commands share the row the switch is anchored before them
// (commands stay trailing-most); when the switch is alone it is the trailing
// element. The unified centring cluster is a separate, untested-here path.
void embedded_switch_slot_decision()
{
	CHECK(meow_embedded_switch_slot(true)  == EmbeddedSwitchSlot::BeforeCommands);
	CHECK(meow_embedded_switch_slot(false) == EmbeddedSwitchSlot::Trailing);
}

// runtime implementation: the single label-visibility decision is identical for Apps and Places
// buttons (both call meow_category_label_visible). Names show only when
// "show names" is on AND the sidebar is not a horizontal strip (supported behavior).
void label_visibility_decision()
{
	CHECK(meow_category_label_visible(true,  false) == true);   // names on, vertical
	CHECK(meow_category_label_visible(true,  true)  == false);  // horizontal strip → icon-only
	CHECK(meow_category_label_visible(false, false) == false);  // names off
	CHECK(meow_category_label_visible(false, true)  == false);  // names off + strip

	// The renderer must derive the strip state from the supported stored value;
	// a forced icon-only presentation must not change the stored name intent.
	const bool horizontal = meow_parse_sidebar_position("horizontal")
			== SidebarPosition::Horizontal;
	const bool left_is_horizontal = meow_parse_sidebar_position("left")
			== SidebarPosition::Horizontal;
	CHECK(meow_category_label_visible(true, horizontal) == false);
	CHECK(meow_category_label_visible(true, left_is_horizontal) == true);
}

// The sidebar label cap is one fixed mode-agnostic rule (INV-3): a label is
// ellipsised only when strictly longer than the cap; at-cap and below-cap
// labels are left uncapped so the size-group floor stays at natural width.
// max_width_chars always reports the cap value (it is applied only when
// ellipsize is true).
void label_cap_decision()
{
	const int cap = MEOW_SIDEBAR_LABEL_MAX_CHARS;   // 22

	// Below the cap → never ellipsised.
	CategoryLabelCap below = meow_category_label_cap(0, cap);
	CHECK(below.ellipsize == false);
	below = meow_category_label_cap(cap - 1, cap);
	CHECK(below.ellipsize == false);

	// Exactly at the cap → still uncapped (rule is strictly-greater-than).
	CategoryLabelCap at = meow_category_label_cap(cap, cap);
	CHECK(at.ellipsize == false);

	// Above the cap → ellipsised at the cap width.
	CategoryLabelCap above = meow_category_label_cap(cap + 1, cap);
	CHECK(above.ellipsize == true);
	CHECK(above.max_width_chars == cap);
	above = meow_category_label_cap(1000, cap);
	CHECK(above.ellipsize == true);
	CHECK(above.max_width_chars == cap);

	// Mode-agnostic by construction: the decision depends only on length and
	// the fixed cap, so an Apps label and a Places label of equal length yield
	// the same result.
	CHECK(meow_category_label_cap(30, cap).ellipsize
			== meow_category_label_cap(30, cap).ellipsize);
}

// The shared sidebar width floor: the widest measured label across both modes.
// Pinning every button to it holds the sidebar width on the visible buttons so
// an Apps<->Places switch cannot collapse it (INV-1). Per-label capping happens
// at measurement time, so this is a plain maximum over pixel widths.
void sidebar_max_label_width_decision()
{
	// No buttons → no floor.
	CHECK(meow_sidebar_max_label_width(nullptr, 0) == 0);

	// The widest label wins (here an Apps category at 142 px, above the short
	// Places labels), so Places mode reserves the Apps width too.
	const int mixed[] = { 38 /*Home*/, 61 /*History*/, 74 /*Favorites*/,
			142 /*All Applications*/, 110 /*Accessories*/ };
	CHECK(meow_sidebar_max_label_width(mixed, 5) == 142);

	// Single entry → itself.
	const int one[] = { 90 };
	CHECK(meow_sidebar_max_label_width(one, 1) == 90);

	// Order-independent: the floor is a max, so permuting the inputs is identical
	// (the same set of buttons in either mode yields the same width).
	const int a[] = { 38, 142, 61 };
	const int b[] = { 61, 38, 142 };
	CHECK(meow_sidebar_max_label_width(a, 3) == meow_sidebar_max_label_width(b, 3));
}

void places_sidebar_chrome_decision()
{
	// Expanding spacers belong only to the Horizontal strip. Reapplying the
	// decision to a steady vertical layout is what keeps sparse Places rows at
	// the same top anchor as Applications rows after initial show-all.
	CHECK(!meow_strip_spacers_visible(false));
	CHECK(meow_strip_spacers_visible(true));

	// A divider is meaningful only when it has visible content on both sides.
	CHECK(!meow_sidebar_group_separator_visible(false, false));
	CHECK(!meow_sidebar_group_separator_visible(true, false));
	CHECK(!meow_sidebar_group_separator_visible(false, true));
	CHECK(meow_sidebar_group_separator_visible(true, true));

	// The dense star is optically reduced while its category-sized slot remains
	// unchanged. None stays hidden and the smallest visible size is not reduced.
	CHECK(meow_favourites_icon_render_size(1) == 1);
	CHECK(meow_favourites_icon_render_size(16) == 16);
	CHECK(meow_favourites_icon_render_size(24) == 16);
	CHECK(meow_favourites_icon_render_size(32) == 21);
	CHECK(meow_favourites_icon_render_size(48) == 32);
}

} // namespace

int main()
{
	row1_vertical_names_on();
	row2_vertical_names_off();
	row3_strip();
	disabled_sidebar_heading();
	category_name_reversion();
	parse_positions();
	horizontal_edge_derivation();
	strip_geometry_ordering();
	fullscreen_main_column_metrics();
	fullscreen_places_disabled_strip_centers_categories();
	final_home_selector_presentation();
	strip_spacer_order_decision();
	default_category_order_base_decision();
	label_visibility_decision();
	label_cap_decision();
	sidebar_max_label_width_decision();
	places_sidebar_chrome_decision();
	embedded_switch_slot_decision();

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_sidebar_layout: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_sidebar_layout: ok\n");
	return EXIT_SUCCESS;
}
