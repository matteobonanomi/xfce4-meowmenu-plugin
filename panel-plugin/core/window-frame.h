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

#ifndef MEOWMENU_CORE_WINDOW_FRAME_H
#define MEOWMENU_CORE_WINDOW_FRAME_H

#include <gtk/gtk.h>

namespace meow
{

/* Reports page-owned layout readiness from a mapped launcher frame. */
typedef bool (*MeowMenuResultFramePrepare)(void* data);

/* MappedResultFrame:
 *
 * Owns one coalesced mapped-frame request and every object retained until its
 * delivery. Dependency destruction, explicit cancellation, and owner teardown
 * all clear the pending callback before borrowed preparation data can be used.
 */
class MappedResultFrame
{
public:
	MappedResultFrame();
	~MappedResultFrame();

	MappedResultFrame(const MappedResultFrame&) = delete;
	MappedResultFrame(MappedResultFrame&&) = delete;
	MappedResultFrame& operator=(const MappedResultFrame&) = delete;
	MappedResultFrame& operator=(MappedResultFrame&&) = delete;

	/* schedule:
	 * @owner: mapped widget whose frame clock delivers the callback.
	 * @toplevel: launcher toplevel that owns composition and clipping.
	 * @result: concrete result widget to invalidate.
	 * @prepare: optional layout-readiness callback.
	 * @prepare_data: borrowed context valid until cancellation or completion.
	 *
	 * Retains the presentation widgets, polls readiness for a bounded number of
	 * frames, and damages both surfaces on every delivery. Repeated requests
	 * coalesce without replacing the retained transaction.
	 *
	 * Returns: true when a callback is already pending or was scheduled.
	 */
	bool schedule(GtkWidget* owner, GtkWidget* toplevel, GtkWidget* result,
			MeowMenuResultFramePrepare prepare = nullptr,
			void* prepare_data = nullptr);

	/* cancel:
	 *
	 * Idempotently removes the callback and releases all retained widgets and
	 * borrowed preparation data before a page or view is replaced.
	 */
	void cancel();

	bool pending() const { return m_callback_id != 0; }

private:
	static gboolean on_frame(GtkWidget*, GdkFrameClock*, gpointer data);
	static void on_callback_destroyed(gpointer data);
	static void on_owner_destroyed(GtkWidget* widget, gpointer data);
	static void on_result_destroyed(GtkWidget* widget, gpointer data);
	void clear_retained_state();

private:
	GtkWidget* m_owner;
	GtkWidget* m_toplevel;
	GtkWidget* m_result;
	MeowMenuResultFramePrepare m_prepare;
	void* m_prepare_data;
	guint m_callback_id;
	gulong m_owner_destroy_handler;
	gulong m_result_destroy_handler;
	unsigned int m_preparation_frames;
};

/* meowmenu_clamp_corner_radius:
 * @radius: a requested corner radius in logical pixels (may be out of range).
 *
 * Clamps the requested radius into the supported window-frame range [0, 24].
 * This is the single source of truth shared by the draw path (which builds the
 * rounded clip) and the live property-changed handler, so the visible rounding
 * can never diverge from the control's range. The mapping is monotonic: a
 * larger in-range request never yields a smaller result.
 *
 * Returns: the clamped radius, always in [0, 24].
 */
int meowmenu_clamp_corner_radius(int radius);

/* meowmenu_frame_draws_border:
 * @is_fullscreen: true when the menu is in full-screen layout.
 * @supports_alpha: true when an RGBA visual / compositor is available.
 *
 * Decides whether the single rounded, composited border stroke is emitted.
 * It is drawn only for the docked, composited case: a full-screen menu reads as
 * one seamless surface (no outline), and without a compositor the rounded path
 * cannot be drawn. The non-composited docked square-border fallback is NOT this
 * predicate — that case is handled separately under its own !is_fullscreen
 * guard in the draw fallback.
 *
 * Returns: true iff (!is_fullscreen && supports_alpha).
 */
bool meowmenu_frame_draws_border(bool is_fullscreen, bool supports_alpha);

/* meowmenu_frameless_launcher_css:
 *
 * Returns the scoped GTK CSS rule that removes persistent Results viewport,
 * scrollbar, and trough chrome without selecting the scrollbar slider. The
 * returned string is static and must not be freed.
 */
const char* meowmenu_frameless_launcher_css();

/* meowmenu_list_selection_css:
 *
 * Returns the scoped GTK rule that gives selected list results the active
 * theme's selected background and foreground. Icon-grid styling is excluded.
 * The returned string is static and must not be freed.
 */
const char* meowmenu_list_selection_css();

/* meowmenu_queue_complete_window_frame:
 * @widget: launcher toplevel after a complete result-model publication.
 *
 * Invalidates the complete composed window once. Child-only damage is not
 * sufficient for the launcher's manual root-child propagation when a hidden
 * loading stack is replaced before the first map.
 *
 * Returns: true when a valid widget was queued.
 */
bool meowmenu_queue_complete_window_frame(GtkWidget* widget);

/* meowmenu_queue_complete_result_frame:
 * @toplevel: launcher toplevel that owns composition and clipping.
 * @result: concrete visible result widget.
 *
 * Invalidates both presentation surfaces independently. A valid composed
 * toplevel never suppresses the concrete result damage required after a model
 * was populated while hidden.
 *
 * Returns: true when at least one valid widget was queued.
 */
bool meowmenu_queue_complete_result_frame(GtkWidget* toplevel,
		GtkWidget* result);

/* meowmenu_create_default_heading_page:
 * @content: built-in result page to wrap.
 * @text: translated heading text for the page.
 * @heading_out: optional borrowed pointer to the created heading label.
 *
 * Creates the common sidebar-disabled page surface. The heading ignores
 * show-all and starts hidden; the resolved sidebar presentation owns its
 * visibility. The returned floating GtkBox is intended to be adopted by a
 * GtkStack.
 *
 * Returns: the new wrapper, or NULL for invalid input.
 */
GtkWidget* meowmenu_create_default_heading_page(GtkWidget* content,
		const char* text, GtkWidget** heading_out);

} // namespace meow

#endif // MEOWMENU_CORE_WINDOW_FRAME_H
