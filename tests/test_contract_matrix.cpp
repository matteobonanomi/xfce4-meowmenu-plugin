/* Compact production-linked checks for independent, display-free contracts. */

#include "core/category-lifetime.h"
#include "core/interactive-resize.h"
#include "core/layout-mode.h"
#include "core/menu-composition.h"
#include "core/opacity-model.h"
#include "core/sidebar-layout.h"
#include "core/window-frame.h"
#include "core/window-keyboard.h"
#include "core/window-size-clamp.h"
#include "config/usage-stats.h"
#include "launcher/application-load-generation.h"
#include "settings.h"
#include "ui/grid-cell-metrics.h"
#include "ui/switch-icons.h"
#include "core/user-session-relayout.h"

#include <gtk/gtk.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace WhiskerMenu;

namespace
{

int failures = 0;
bool display_unavailable = false;

#define CHECK(value) do { \
	if (!(value)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); \
		++failures; \
	} \
} while (0)

void test_focus_contract();

void test_keyboard_contract()
{
	using namespace Keyboard;
	PhysicalDirection direction = PhysicalDirection::Up;
	CHECK(normalize_direction(GDK_KEY_Left, &direction));
	CHECK(direction == PhysicalDirection::Left);
	CHECK(normalize_direction(GDK_KEY_KP_Right, &direction));
	CHECK(direction == PhysicalDirection::Right);
	CHECK(tab_action(true) == TabAction::ToggleMode);
	CHECK(tab_action(false) == TabAction::Inert);
	CHECK(esc_action(classify_esc_state(false, false, true))
			== EscAction::ClearQuery);
	CHECK(esc_action(classify_esc_state(true, true, true))
			== EscAction::CloseContextMenu);
	GdkEventKey event = {};
	event.keyval = GDK_KEY_a;
	CHECK(classify_key(&event) == KeyClass::Printable);
	CHECK(is_printable_for_search(&event));
	test_focus_contract();
}

void test_focus_contract()
{
	using namespace Keyboard;
	FocusTarget origin;
	origin.target_id = 1;
	origin.region = NavigationRegion::Results;
	origin.kind = FocusTargetKind::ResultItem;
	origin.rectangle = NavigationRect(50, 50, 10, 10);
	origin.usable = true;
	FocusTarget candidate = origin;
	candidate.target_id = 2;
	candidate.rectangle = NavigationRect(70, 50, 10, 10);
	std::vector<FocusTarget> targets{candidate};
	CHECK(choose_spatial_target(origin.rectangle, PhysicalDirection::Right,
			targets, false) == 0);
	CHECK(decide_navigation(origin, PhysicalDirection::Right, targets, {}, false)
			.kind == NavigationDecisionKind::InternalMove);
}

void test_geometry_contract()
{
	int width = 0;
	int height = 0;
	meow::clamp_default_size(820, 600, 640, 480, &width, &height);
	CHECK(width <= 640 && height <= 480 && width > 0 && height > 0);
	GdkRectangle monitor{100, 50, 1000, 800};
	int x = 0;
	int y = 0;
	meow::centered_origin(monitor, 400, 300, &x, &y);
	CHECK(x == 400 && y == 300);
	using namespace InteractiveResize;
	CHECK(direction_axes(Direction::Right).horizontal);
	CHECK(opposite_edge_anchor(Rectangle{10, 20, 30, 40}).left == 10);
}

void test_layout_domain_contract()
{
	CHECK(layout_mode_from_key("centered") == LayoutMode::Centered);
	CHECK(layout_mode_from_key("unknown") == LayoutMode::Docked);
	CHECK(layout_mode_key_is_supported("fullscreen"));
	CHECK(!layout_mode_key_is_supported("floating"));
	CHECK(control_enabled(LayoutControl::MenuWidth, LayoutMode::Docked));
	CHECK(!control_enabled(LayoutControl::MenuWidth, LayoutMode::FullScreen));
	CHECK(meow_parse_sidebar_position("right") == SidebarPosition::Right);
	CHECK(meow_parse_sidebar_position("invalid") == SidebarPosition::Left);
	CHECK(meow_resolve_sidebar_edge(SidebarPosition::Horizontal, false, false)
			== SidebarPosition::Bottom);
}

void test_composition_contract()
{
	MenuCompositionInput input{};
	input.layout_mode = LayoutMode::Docked;
	input.primary_edge = PrimaryEdge::Top;
	input.sidebar = CompositionSidebar::Right;
	input.show_profile = true;
	input.show_session = true;
	input.available_session_actions = 2;
	input.places_enabled = true;
	input.direction = MenuDirection::LeftToRight;
	const MenuComposition result = meow_resolve_menu_composition(input);
	CHECK(result.sidebar == CompositionSidebar::Right);
	CHECK(result.effective_profile);
	CHECK(result.effective_session);
	CHECK(result.apps_places_location != MenuControlLocation::Hidden);
	CHECK(meow_composition_has_vertical_sidebar(result.sidebar));
}

void test_grid_contract()
{
	const GridCellMetrics metrics = meow_grid_cell_metrics(4, 32, 6, false, 2);
	CHECK(metrics.minimum_width > 0);
	CHECK(metrics.natural_height >= metrics.minimum_height);
	CHECK(meow_grid_cell_width(4, 32, false).minimum == metrics.minimum_width);
	const GridColumnLayout layout = meow_grid_column_layout(640, 8, 8, 4, 96);
	CHECK(layout.columns >= 1 && layout.item_width > 0);
	CHECK(meow_grid_effective_viewport_width(800, 900, 700) <= 700);
}

void test_opacity_contract()
{
	CHECK(std::fabs(meowmenu_opacity_alpha(0)) < 1e-9);
	CHECK(std::fabs(meowmenu_opacity_alpha(100) - 1.0) < 1e-9);
	CHECK(meowmenu_background_translucent(99));
	CHECK(!meowmenu_background_translucent(100));
	char css[MEOWMENU_CSS_ALPHA_BUFSZ] = {};
	CHECK(std::strcmp(meowmenu_format_css_alpha(0.6, css), "0.600") == 0);
	CHECK(meowmenu_effective_background_alpha(60, false) == 1.0);
}

void test_icon_contract()
{
	const ModeButtonLabels text = meow_mode_button_labels(false,
		"Apps", "Applications");
	CHECK(std::strcmp(text.visible_text, "Apps") == 0);
	CHECK(text.tooltip_text == nullptr);
	const ModeButtonLabels icons = meow_mode_button_labels(true,
		"Apps", "Applications");
	CHECK(icons.visible_text == nullptr);
	CHECK(std::strcmp(icons.tooltip_text, "Applications") == 0);
	CHECK(meow_selector_session_icon_px(20) > 0);
}

void test_reload_contract()
{
	CHECK(classify_reload_intent("/button-title") == ReloadIntent::Button);
	CHECK(classify_reload_intent("/view-mode") == ReloadIntent::Content);
	CHECK(classify_reload_intent("/menu-width") == ReloadIntent::Layout);
	CHECK(classify_reload_intent("/not-a-setting") == ReloadIntent::None);
}

void test_frame_contract()
{
	CHECK(meow::meowmenu_clamp_corner_radius(-1) == 0);
	CHECK(meow::meowmenu_clamp_corner_radius(100) == 24);
	CHECK(meow::meowmenu_frame_draws_border(false, true));
	CHECK(!meow::meowmenu_frame_draws_border(true, true));
	CHECK(std::strstr(meow::meowmenu_frameless_launcher_css(), "viewport") != nullptr);
}

void test_generation_contract()
{
	ApplicationLoadGeneration generation(4);
	CHECK(generation.start());
	CHECK(generation.candidate_ready(true));
	CHECK(generation.commit(4, true));
	CHECK(generation.status() == ApplicationLoadStatus::Committed);
	ApplicationLoadGeneration stale(5);
	CHECK(stale.start());
	CHECK(!stale.commit(6, true));
	CHECK(stale.status() == ApplicationLoadStatus::Discarded);
}

void test_usage_contract()
{
	UsageStats stats;
	CHECK(stats.get_frecency("missing.desktop", 0.5) == 0.0);
	CHECK(stats.get_frecency(nullptr, 0.5) == 0.0);
}

void test_category_contract()
{
	if (!gtk_init_check(nullptr, nullptr))
	{
		display_unavailable = true;
		return;
	}
	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* fallback = gtk_button_new();
	GtkWidget* active = gtk_toggle_button_new();
	gtk_box_pack_start(GTK_BOX(box), fallback, false, false, 0);
	gtk_box_pack_start(GTK_BOX(box), active, false, false, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active), TRUE);
	CHECK(active_toggle_child_or_default(GTK_CONTAINER(box), fallback) == active);
	std::vector<GtkWidget*> widgets{active};
	detach_category_widgets(nullptr, widgets);
	CHECK(widgets.empty());
	gtk_widget_destroy(box);
}

using Case = void (*)();

struct NamedCase
{
	const char* name;
	Case function;
};

const NamedCase cases[] = {
	{"keyboard", test_keyboard_contract},
	{"focus", test_focus_contract},
	{"geometry", test_geometry_contract},
	{"layout", test_layout_domain_contract},
	{"composition", test_composition_contract},
	{"grid", test_grid_contract},
	{"opacity", test_opacity_contract},
	{"icons", test_icon_contract},
	{"reload", test_reload_contract},
	{"frame", test_frame_contract},
	{"generation", test_generation_contract},
	{"usage", test_usage_contract},
	{"category", test_category_contract},
};

Case find_case(const char* name)
{
	for (const NamedCase& item : cases)
		if (std::strcmp(item.name, name) == 0)
			return item.function;
	return nullptr;
}

}

int main(int argc, char** argv)
{
	const char* requested = nullptr;
	for (int i = 1; i < argc; ++i)
	{
		if (std::strncmp(argv[i], "--case=", 7) == 0)
			requested = argv[i] + 7;
	}
	if (!requested)
	{
		for (const NamedCase& item : cases)
			item.function();
	}
	else if (Case function = find_case(requested))
		function();
	else
		return 2;
	if (failures != 0)
		return 1;
	if (display_unavailable)
		return 77;
	return 0;
}
