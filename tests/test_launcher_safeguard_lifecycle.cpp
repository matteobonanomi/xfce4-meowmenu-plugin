/* test_launcher_safeguard_lifecycle:
 *
 * Freezes the the documented behavior invariant (render-invariants contract I13): the
 * transparent-surface scroll-reveal safeguard in the shared LauncherView base
 * connects a value-changed handler to the scrolled window's vertical
 * GtkAdjustment — the one safeguard connection on an object the view does NOT
 * own and which outlives it across menu rebuilds. That handler MUST stop firing
 * once the view is destroyed, or a later value-changed on the surviving
 * adjustment invokes queue_full_redraw_safeguard() -> get_widget() on a
 * freed view: the intermittent plugin crash/restart the documented behavior describes (closing,
 * rebuilding, or reloading the menu after scrolling a translucent list).
 *
 * Setup faithful to production: the view is placed in a real GtkScrolledWindow,
 * which installs its vertical adjustment on the scrollable child (firing the
 * base's notify::vadjustment hook) — exactly how the safeguard is wired at
 * runtime. We hold our own ref on that adjustment so it survives the view, as the
 * reused scrolled window does in production.
 *
 * The fix lives entirely in the header-only base LauncherView, so a minimal
 * concrete subclass over a real GtkTreeView exercises the identical production
 * code path (the notify::vadjustment binding, the view-"destroy" disconnect, and
 * the adjustment-swap disconnect) without dragging in the Settings/Plugin/xfconf
 * graph the real LauncherTreeView constructor requires. Exercising one concrete
 * view validates the base for both views (research R6).
 *
 * Observable: get_widget() is the first thing the safeguard touches on the view,
 * so a call counter on it tells us precisely whether the safeguard handler fired
 * for a given value-changed emission — reliably, unlike g_signal_handler_find,
 * which does not match the closure-connected handler here. After teardown the
 * handler must NOT fire (count unchanged) and the process must not fault; against
 * the pre-fix code this same emission is a use-after-free that segfaults and is
 * caught by the sanitizer CI (constitution VI).
 *
 * Constructing the GtkScrolledWindow / GtkTreeView builds their style contexts,
 * which GTK 3 cannot create without a display connection, so the test skips
 * cleanly when no display is present. CI runs it under a virtual display so the
 * safeguard lifetime path — and its use-after-free guard — is still exercised
 * under the sanitizers.
 */

#include "ui/launcher-view.h"
#include "config/xfce-helpers.h"
#include "launcher/command-timeout.h"
#include "launcher/launcher-safety.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <cstdio>
#include <cstdlib>

using namespace WhiskerMenu;

namespace
{

int g_failures = 0;

// Counts calls to the test view's get_widget(). The safeguard redraw calls
// get_widget() whenever it fires while translucent, so the delta around a
// value-changed emission reports whether the safeguard handler ran. It is a
// process-global (not a member), so it survives the view's destruction and stays
// readable after teardown.
int g_get_widget_calls = 0;

#define CHECK(cond) do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

// Minimal concrete LauncherView over a real GtkTreeView. It wires the shared base
// safeguard the same way the production views do (enable_hover_selection on the
// scrollable view widget) and tears the widget down in its destructor exactly as
// LauncherTreeView::~LauncherTreeView does, so the base "destroy" disconnect runs
// under the real lifecycle. Every other override is an inert stub: this test only
// drives the scroll-reveal lifetime path.
class TestLauncherView : public LauncherView
{
public:
	TestLauncherView()
	{
		m_widget = gtk_tree_view_new();
		g_object_ref_sink(m_widget);
		enable_hover_selection(m_widget);
	}

	~TestLauncherView() override
	{
		gtk_widget_destroy(m_widget);
		g_object_unref(m_widget);
	}

	GtkWidget* get_widget() const override
	{
		++g_get_widget_calls;
		return m_widget;
	}

	GtkTreePath* get_cursor() const override { return nullptr; }
	GtkTreePath* get_path_at_pos(int, int) const override { return nullptr; }
	GtkTreePath* get_selected_path() const override { return nullptr; }
	bool is_path_selected(GtkTreePath*) const override { return false; }
	void activate_path(GtkTreePath*) override {}
	void scroll_to_path(GtkTreePath*) override {}
	void select_path(GtkTreePath*) override {}
	void set_cursor(GtkTreePath*) override {}
	bool is_first_visual_row(GtkTreePath*) const override { return false; }

	void set_fixed_height_mode(bool) override {}
	void set_selection_mode(GtkSelectionMode) override {}

	void hide_tooltips() override {}
	void show_tooltips() override {}

	void clear_selection() override {}
	void collapse_all() override {}

	void set_model(GtkTreeModel*) override {}
	void unset_model() override {}

	void set_drag_source(GdkModifierType, const GtkTargetEntry*, gint, GdkDragAction) override {}
	void set_drag_dest(const GtkTargetEntry*, gint, GdkDragAction) override {}
	void unset_drag_source() override {}
	void unset_drag_dest() override {}

	void reload_icon_size() override {}

private:
	GtkWidget* m_widget;
};

} // namespace

int main()
{
	GtkAdjustment* signal_owner = GTK_ADJUSTMENT(
			gtk_adjustment_new(0, 0, 10, 1, 1, 0));
	int signal_calls = 0;
	gulong signal_id = connect(signal_owner, "value-changed",
		[&signal_calls](GtkAdjustment*)
		{
			++signal_calls;
		});
	disconnect_signal(signal_owner, signal_id);
	g_signal_emit_by_name(signal_owner, "value-changed");
	CHECK(signal_calls == 0);
	CHECK(signal_id == 0);
	g_object_unref(signal_owner);

	CHECK(command_timeout_source_is_active(60));
	CHECK(command_timeout_source_is_active(0));
	CHECK(!command_timeout_source_is_active(-1));

	gchar** argv = launcher_editor_argv("xfce-desktop-item-edit",
			"file:///tmp/app '$(touch nope)'.desktop");
	CHECK(argv != nullptr);
	CHECK(g_strcmp0(argv[0], "xfce-desktop-item-edit") == 0);
	CHECK(g_strcmp0(argv[1], "file:///tmp/app '$(touch nope)'.desktop") == 0);
	CHECK(argv[2] == nullptr);
	g_strfreev(argv);

	CHECK(g_strcmp0(xfce_desktop_item_editor(XfceDependencyRegime::Legacy),
			"exo-desktop-item-edit") == 0);
	CHECK(g_strcmp0(xfce_desktop_item_editor(XfceDependencyRegime::Successor),
			"xfce-desktop-item-edit") == 0);

	gchar* relpath = launcher_hide_relpath_for_uri(
			"file:///usr/share/applications/app%20name.desktop",
			"/usr/share/applications");
	CHECK(g_strcmp0(relpath, "applications/app name.desktop") == 0);
	g_free(relpath);
	CHECK(launcher_hide_relpath_for_uri(
			"https://example.invalid/app.desktop",
			"/usr/share/applications") == nullptr);
	CHECK(launcher_hide_relpath_for_uri(
			"file:///opt/apps/app.desktop",
			"/usr/share/applications") == nullptr);

	GtkListStore* store = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_list_store_insert_with_values(store, nullptr, -1, 0, "row", -1);
	GtkTreeIter iter;
	GtkTreePath* valid_path = gtk_tree_path_new_from_indices(0, -1);
	GtkTreePath* stale_path = gtk_tree_path_new_from_indices(9, -1);
	CHECK(launcher_model_get_iter(GTK_TREE_MODEL(store), valid_path, &iter));
	CHECK(!launcher_model_get_iter(GTK_TREE_MODEL(store), stale_path, &iter));
	CHECK(!launcher_model_get_iter(GTK_TREE_MODEL(store), nullptr, &iter));
	gtk_tree_path_free(valid_path);
	gtk_tree_path_free(stale_path);
	g_object_unref(store);

	// Constructing the widgets below builds their style contexts, which GTK 3
	// cannot do without a display connection. Skip cleanly on a headless host
	// instead of aborting; CI supplies a virtual display so the safeguard
	// lifetime path still runs under the sanitizers.
	if (!gtk_init_check(nullptr, nullptr))
	{
		std::printf("# SKIP: GTK could not initialise (no display)\n");
		return 77; // meson exitcode protocol: 77 marks the test skipped
	}

	GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
	g_object_ref_sink(scrolled);

	TestLauncherView* view = new TestLauncherView();

	// Adding the scrollable view installs the scrolled window's vertical
	// adjustment on it (notify::vadjustment), binding the base safeguard. Enable
	// the guard so the callback takes the active path.
	gtk_container_add(GTK_CONTAINER(scrolled), view->get_widget());
	view->set_full_redraw_safeguard(true);

	// Hold our own ref: the adjustment must outlive the view, like the reused
	// scrolled window's adjustment in production.
	GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
	g_object_ref(adj);

	// Live path: with the guarded view alive, a value-changed must reach
	// the safeguard (it calls get_widget()).
	int before = g_get_widget_calls;
	g_signal_emit_by_name(adj, "value-changed");
	CHECK(g_get_widget_calls > before);

	// Destroy the view. Its "destroy" (and the adjustment unbinding the scrolled
	// window performs on child removal) must leave no live safeguard handler.
	delete view;

	// the documented behavior: emitting on the surviving adjustment after teardown must NOT reach
	// the safeguard — get_widget() is not called and nothing dereferences the
	// freed view. Against the pre-fix code this emission is a use-after-free.
	before = g_get_widget_calls;
	g_signal_emit_by_name(adj, "value-changed");
	CHECK(g_get_widget_calls == before);

	g_object_unref(adj);
	g_object_unref(scrolled);

	if (g_failures != 0)
	{
		std::fprintf(stderr, "test_launcher_safeguard_lifecycle: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("test_launcher_safeguard_lifecycle: ok\n");
	return EXIT_SUCCESS;
}
