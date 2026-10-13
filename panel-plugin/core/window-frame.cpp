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

#include "window-frame.h"

#include <gtk/gtk.h>

namespace meow
{

// Supported corner-radius range. Mirrors the /corner-radius control range so
// the visible rounding always tracks what the GUI can request.
namespace
{
const int kMinCornerRadius = 0;
const int kMaxCornerRadius = 24;
const unsigned int kMaxMappedResultPreparationFrames = 8;
} // namespace

MappedResultFrame::MappedResultFrame() :
	m_owner(nullptr),
	m_toplevel(nullptr),
	m_result(nullptr),
	m_prepare(nullptr),
	m_prepare_data(nullptr),
	m_callback_id(0),
	m_owner_destroy_handler(0),
	m_result_destroy_handler(0),
	m_preparation_frames(0)
{
}

MappedResultFrame::~MappedResultFrame()
{
	cancel();
}

/* MappedResultFrame::schedule:
 * @owner: mapped widget whose frame clock delivers the callback.
 * @toplevel: launcher toplevel that owns composition and clipping.
 * @result: concrete result widget to invalidate.
 * @prepare: optional layout-readiness callback.
 * @prepare_data: borrowed context valid until cancellation or completion.
 *
 * Starts one bounded mapped-frame transaction and observes dependency
 * destruction so no callback can outlive its page-owned preparation context.
 * An already pending request remains authoritative and is not replaced.
 *
 * Returns: true when a callback is already pending or was scheduled.
 */
bool MappedResultFrame::schedule(GtkWidget* owner, GtkWidget* toplevel,
		GtkWidget* result, MeowMenuResultFramePrepare prepare,
		void* prepare_data)
{
	if (!GTK_IS_WIDGET(owner) || !GTK_IS_WIDGET(toplevel)
			|| !GTK_IS_WIDGET(result))
	{
		return false;
	}
	if (pending())
		return true;

	m_owner = owner;
	m_toplevel = GTK_WIDGET(g_object_ref(toplevel));
	m_result = GTK_WIDGET(g_object_ref(result));
	m_prepare = prepare;
	m_prepare_data = prepare_data;
	m_preparation_frames = 0;
	m_owner_destroy_handler = g_signal_connect(owner, "destroy",
			G_CALLBACK(on_owner_destroyed), this);
	if (result != owner)
	{
		m_result_destroy_handler = g_signal_connect(result, "destroy",
				G_CALLBACK(on_result_destroyed), this);
	}
	m_callback_id = gtk_widget_add_tick_callback(owner, on_frame, this,
			on_callback_destroyed);
	if (m_callback_id == 0)
		clear_retained_state();
	return m_callback_id != 0;
}

/* MappedResultFrame::cancel:
 *
 * Removes the current tick callback. GTK invokes the registered destroy
 * notifier synchronously, which releases every retained dependency.
 */
void MappedResultFrame::cancel()
{
	if (m_callback_id == 0)
		return;
	const guint callback_id = m_callback_id;
	m_callback_id = 0;
	if (GTK_IS_WIDGET(m_owner))
		gtk_widget_remove_tick_callback(m_owner, callback_id);
}

/* MappedResultFrame::on_frame:
 * @data: MappedResultFrame that owns this callback and its retained surfaces.
 *
 * Re-enters page layout preparation before independently damaging the result
 * and composed toplevel. Readiness polling stops after the established bound.
 *
 * Returns: G_SOURCE_CONTINUE only while readiness remains within the bound.
 */
gboolean MappedResultFrame::on_frame(GtkWidget*, GdkFrameClock*, gpointer data)
{
	MappedResultFrame* frame = static_cast<MappedResultFrame*>(data);
	++frame->m_preparation_frames;
	const bool ready = !frame->m_prepare
			|| frame->m_prepare(frame->m_prepare_data);
	meowmenu_queue_complete_result_frame(frame->m_toplevel, frame->m_result);
	return !ready && frame->m_preparation_frames
			< kMaxMappedResultPreparationFrames
			? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

void MappedResultFrame::on_callback_destroyed(gpointer data)
{
	MappedResultFrame* frame = static_cast<MappedResultFrame*>(data);
	frame->m_callback_id = 0;
	frame->clear_retained_state();
}

void MappedResultFrame::on_owner_destroyed(GtkWidget*, gpointer data)
{
	MappedResultFrame* frame = static_cast<MappedResultFrame*>(data);
	frame->m_owner_destroy_handler = 0;
	frame->cancel();
}

void MappedResultFrame::on_result_destroyed(GtkWidget*, gpointer data)
{
	MappedResultFrame* frame = static_cast<MappedResultFrame*>(data);
	frame->m_result_destroy_handler = 0;
	frame->cancel();
}

/* MappedResultFrame::clear_retained_state:
 *
 * Disconnects dependency observers and releases all retained or borrowed state
 * after GTK has removed the callback. Clearing fields before unref prevents a
 * widget finalizer from re-entering cancellation with stale data.
 */
void MappedResultFrame::clear_retained_state()
{
	GtkWidget* owner = m_owner;
	GtkWidget* toplevel = m_toplevel;
	GtkWidget* result = m_result;
	const gulong owner_handler = m_owner_destroy_handler;
	const gulong result_handler = m_result_destroy_handler;
	m_owner = nullptr;
	m_toplevel = nullptr;
	m_result = nullptr;
	m_prepare = nullptr;
	m_prepare_data = nullptr;
	m_owner_destroy_handler = 0;
	m_result_destroy_handler = 0;
	m_preparation_frames = 0;

	if (owner_handler && GTK_IS_WIDGET(owner)
			&& g_signal_handler_is_connected(owner, owner_handler))
	{
		g_signal_handler_disconnect(owner, owner_handler);
	}
	if (result_handler && GTK_IS_WIDGET(result)
			&& g_signal_handler_is_connected(result, result_handler))
	{
		g_signal_handler_disconnect(result, result_handler);
	}
	if (result)
		g_object_unref(result);
	if (toplevel)
		g_object_unref(toplevel);
}

int meowmenu_clamp_corner_radius(int radius)
{
	if (radius < kMinCornerRadius)
		return kMinCornerRadius;
	if (radius > kMaxCornerRadius)
		return kMaxCornerRadius;
	return radius;
}

bool meowmenu_frame_draws_border(bool is_fullscreen, bool supports_alpha)
{
	// Full-screen reads as one seamless surface (no outline); the composited
	// rounded stroke also needs an RGBA visual to be drawn at all.
	return !is_fullscreen && supports_alpha;
}

/* meowmenu_frameless_launcher_css:
 *
 * Keeps the complete Results scrollbar chrome transparent while deliberately
 * leaving its slider outside the rule.
 *
 * Returns: static CSS owned by this module.
 */
const char* meowmenu_frameless_launcher_css()
{
	// Themes may paint the hairline on the scrollbar rather than its trough.
	return
			".meowmenu scrolledwindow.launchers-pane,"
			".meowmenu scrolledwindow.launchers-pane > viewport,"
			".meowmenu scrolledwindow.launchers-pane scrollbar,"
			".meowmenu scrolledwindow.launchers-pane scrollbar trough"
			"{ background-color: transparent; background-image: none;"
			"  border: none; outline: none; box-shadow: none; }";
}

const char* meowmenu_list_selection_css()
{
	return
			".meowmenu treeview.launchers.view:selected,"
			".meowmenu treeview.launchers.view:selected:focus"
			"{ background-color: @theme_selected_bg_color;"
			"  background-image: none;"
			"  color: @theme_selected_fg_color; }";
}

bool meowmenu_queue_complete_window_frame(GtkWidget* widget)
{
	if (!GTK_IS_WIDGET(widget))
		return false;
	gtk_widget_queue_draw(widget);
	return true;
}

bool meowmenu_queue_complete_result_frame(GtkWidget* toplevel,
		GtkWidget* result)
{
	bool queued = false;
	if (GTK_IS_WIDGET(result))
	{
		gtk_widget_queue_draw(result);
		queued = true;
	}
	if (GTK_IS_WIDGET(toplevel))
	{
		gtk_widget_queue_draw(toplevel);
		queued = true;
	}
	return queued;
}

GtkWidget* meowmenu_create_default_heading_page(GtkWidget* content,
		const char* text, GtkWidget** heading_out)
{
	if (heading_out)
		*heading_out = nullptr;
	if (!GTK_IS_WIDGET(content) || !text)
		return nullptr;

	GtkWidget* heading = gtk_label_new(text);
	gtk_widget_set_halign(heading, GTK_ALIGN_START);
	gtk_widget_set_no_show_all(heading, TRUE);
	gtk_widget_set_visible(heading, FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(heading),
			"meow-default-heading");

	GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(outer), heading, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(outer), content, TRUE, TRUE, 0);
	if (heading_out)
		*heading_out = heading;
	return outer;
}

} // namespace meow
