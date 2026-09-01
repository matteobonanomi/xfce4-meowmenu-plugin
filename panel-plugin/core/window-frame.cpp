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

struct MappedResultFrame
{
	GtkWidget* toplevel;
	GtkWidget* result;
	guint* callback_id;
	MeowMenuResultFramePrepare prepare;
	void* prepare_data;
	guint id;
	unsigned int preparation_frames;
};

/* queue_mapped_result_frame:
 * @data: owned MappedResultFrame retained by GTK until callback destruction.
 *
 * Re-enters page layout preparation from the mapped launcher clock, then
 * damages both the concrete result and composed toplevel. An incomplete result
 * retains the callback for a bounded number of frames so hidden-stack mapping
 * and the resulting layout can settle without user input.
 *
 * Returns: G_SOURCE_CONTINUE while readiness is pending within the bound;
 * otherwise G_SOURCE_REMOVE.
 */
gboolean queue_mapped_result_frame(GtkWidget*, GdkFrameClock*, gpointer data)
{
	MappedResultFrame* frame = static_cast<MappedResultFrame*>(data);
	++frame->preparation_frames;
	const bool ready = !frame->prepare
			|| frame->prepare(frame->prepare_data);
	meow::meowmenu_queue_complete_result_frame(
			frame->toplevel, frame->result);
	if (!ready && frame->preparation_frames
			< kMaxMappedResultPreparationFrames)
	{
		return G_SOURCE_CONTINUE;
	}
	if (frame->callback_id && *frame->callback_id == frame->id)
		*frame->callback_id = 0;
	return G_SOURCE_REMOVE;
}

/* destroy_mapped_result_frame:
 * @data: owned MappedResultFrame whose GTK callback has ended or was removed.
 *
 * Clears the caller's slot only when it still names this callback, releases
 * the widgets retained across the frame boundary, and destroys the context.
 */
void destroy_mapped_result_frame(gpointer data)
{
	MappedResultFrame* frame = static_cast<MappedResultFrame*>(data);
	if (frame->callback_id && *frame->callback_id == frame->id)
		*frame->callback_id = 0;
	g_object_unref(frame->toplevel);
	g_object_unref(frame->result);
	delete frame;
}
} // namespace

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

bool meowmenu_schedule_mapped_result_frame(GtkWidget* owner,
		GtkWidget* toplevel, GtkWidget* result, guint* callback_id,
		MeowMenuResultFramePrepare prepare, void* prepare_data)
{
	if (!GTK_IS_WIDGET(owner) || !GTK_IS_WIDGET(toplevel)
			|| !GTK_IS_WIDGET(result) || !callback_id)
	{
		return false;
	}
	if (*callback_id != 0)
		return true;

	MappedResultFrame* frame = new MappedResultFrame{
			GTK_WIDGET(g_object_ref(toplevel)),
			GTK_WIDGET(g_object_ref(result)), callback_id,
			prepare, prepare_data, 0, 0};
	frame->id = gtk_widget_add_tick_callback(owner,
			queue_mapped_result_frame, frame, destroy_mapped_result_frame);
	*callback_id = frame->id;
	return frame->id != 0;
}

void meowmenu_cancel_mapped_result_frame(GtkWidget* owner, guint* callback_id)
{
	if (!callback_id || *callback_id == 0)
		return;
	const guint id = *callback_id;
	*callback_id = 0;
	if (GTK_IS_WIDGET(owner))
		gtk_widget_remove_tick_callback(owner, id);
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
