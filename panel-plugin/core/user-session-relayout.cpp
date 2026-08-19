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

#include "core/user-session-relayout.h"

using namespace WhiskerMenu;

bool
WhiskerMenu::meow_container_contains_child(GtkContainer* container,
                                           GtkWidget* child)
{
	if (!GTK_IS_CONTAINER(container) || !GTK_IS_WIDGET(child))
		return false;

	GList* children = gtk_container_get_children(container);
	const bool found = g_list_find(children, child) != nullptr;
	g_list_free(children);
	return found;
}

bool
WhiskerMenu::meow_box_contains_child(GtkBox* box, GtkWidget* child)
{
	return GTK_IS_BOX(box)
			&& meow_container_contains_child(GTK_CONTAINER(box), child);
}

bool
WhiskerMenu::meow_box_repack_child(GtkBox* box, GtkWidget* child,
                                   bool pack_end, bool expand, bool fill,
                                   guint padding)
{
	if (!GTK_IS_BOX(box) || !GTK_IS_WIDGET(child))
		return false;

	g_object_ref(child);
	if (GtkWidget* parent = gtk_widget_get_parent(child))
	{
		if (GTK_IS_CONTAINER(parent))
			gtk_container_remove(GTK_CONTAINER(parent), child);
	}

	if (pack_end)
		gtk_box_pack_end(box, child, expand, fill, padding);
	else
		gtk_box_pack_start(box, child, expand, fill, padding);

	g_object_unref(child);
	return true;
}

bool
WhiskerMenu::meow_grid_attach_child(GtkGrid* grid, GtkWidget* child,
                                    gint left, gint top, gint width,
                                    gint height)
{
	if (!GTK_IS_GRID(grid) || !GTK_IS_WIDGET(child) || width <= 0 || height <= 0)
		return false;

	g_object_ref(child);
	if (GtkWidget* parent = gtk_widget_get_parent(child))
	{
		if (GTK_IS_CONTAINER(parent))
			gtk_container_remove(GTK_CONTAINER(parent), child);
	}
	gtk_grid_attach(grid, child, left, top, width, height);
	g_object_unref(child);
	return true;
}

bool
WhiskerMenu::meow_size_group_set_widget(GtkSizeGroup* group,
                                        GtkWidget* widget, bool member)
{
	if (!GTK_IS_SIZE_GROUP(group) || !GTK_IS_WIDGET(widget))
		return false;

	const bool current = g_slist_find(gtk_size_group_get_widgets(group), widget)
			!= nullptr;
	if (member && !current)
		gtk_size_group_add_widget(group, widget);
	else if (!member && current)
		gtk_size_group_remove_widget(group, widget);

	return member == (g_slist_find(gtk_size_group_get_widgets(group), widget)
			!= nullptr);
}

/* meow_configure_vertical_sidebar_width:
 * @sidebar: vertical navigation scroller that owns the width source.
 * @profile: optional visible header aligned with the navigation column.
 * @active: whether windowed vertical-column sizing is active.
 * @profile_visible: whether @profile contributes its natural width.
 *
 * Measures with stale requests cleared, reserves the automatic vertical
 * scrollbar's theme width, then fixes both column owners to the larger natural
 * width. Explicitly disabling expansion keeps later surplus window allocation
 * in the Results column rather than feeding it back through a cross-parent
 * GtkSizeGroup.
 *
 * Returns: the applied width, or -1 when inactive or given invalid widgets.
 */
int
WhiskerMenu::meow_configure_vertical_sidebar_width(GtkWidget* sidebar,
                                                   GtkWidget* profile,
                                                   bool active,
                                                   bool profile_visible)
{
	if (!GTK_IS_WIDGET(sidebar) || !GTK_IS_WIDGET(profile))
		return -1;

	int sidebar_height = -1;
	int profile_height = -1;
	gtk_widget_get_size_request(sidebar, nullptr, &sidebar_height);
	gtk_widget_get_size_request(profile, nullptr, &profile_height);
	gtk_widget_set_size_request(sidebar, -1, sidebar_height);
	gtk_widget_set_size_request(profile, -1, profile_height);
	gtk_widget_set_hexpand(sidebar, FALSE);
	gtk_widget_set_hexpand(profile, FALSE);
	if (!active)
		return -1;

	int sidebar_natural = 0;
	int profile_natural = 0;
	gtk_widget_get_preferred_width(sidebar, nullptr, &sidebar_natural);
	if (GTK_IS_SCROLLED_WINDOW(sidebar))
	{
		GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(sidebar));
		GtkWidget* scrollbar = gtk_scrolled_window_get_vscrollbar(
				GTK_SCROLLED_WINDOW(sidebar));
		int viewport_natural = 0;
		int scrollbar_natural = 0;
		if (GTK_IS_WIDGET(viewport))
			gtk_widget_get_preferred_width(viewport, nullptr,
					&viewport_natural);
		if (GTK_IS_WIDGET(scrollbar))
			gtk_widget_get_preferred_width(scrollbar, nullptr,
					&scrollbar_natural);
		gint scrollbar_spacing = 0;
		gtk_widget_style_get(sidebar,
				"scrollbar-spacing", &scrollbar_spacing, nullptr);
		GtkBorder padding = {};
		GtkBorder border = {};
		GtkStyleContext* style = gtk_widget_get_style_context(sidebar);
		gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);
		gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);
		const int reserved = viewport_natural + scrollbar_natural
				+ MAX(0, scrollbar_spacing)
				+ MAX(0, padding.left) + MAX(0, padding.right)
				+ MAX(0, border.left) + MAX(0, border.right);
		sidebar_natural = MAX(sidebar_natural, reserved);
	}
	if (profile_visible)
		gtk_widget_get_preferred_width(profile, nullptr, &profile_natural);
	const int width = MAX(sidebar_natural, profile_natural);
	gtk_widget_set_size_request(sidebar, width, sidebar_height);
	if (profile_visible)
		gtk_widget_set_size_request(profile, width, profile_height);
	return width;
}

/* meow_configure_vertical_sidebar_content:
 *
 * See the public declaration for the cross-axis allocation contract.
 */
bool
WhiskerMenu::meow_configure_vertical_sidebar_content(GtkWidget* categories,
                                                     bool active)
{
	if (!GTK_IS_CONTAINER(categories))
		return false;

	gtk_widget_set_hexpand(categories, active);
	gtk_widget_set_halign(categories, GTK_ALIGN_FILL);
	GList* children = gtk_container_get_children(GTK_CONTAINER(categories));
	for (GList* child = children; child; child = child->next)
	{
		GtkWidget* widget = GTK_WIDGET(child->data);
		if (!GTK_IS_RADIO_BUTTON(widget))
			continue;
		gtk_widget_set_hexpand(widget, active);
		gtk_widget_set_halign(widget, GTK_ALIGN_FILL);
		if (!active)
		{
			gtk_widget_set_margin_start(widget, 0);
			gtk_widget_set_margin_end(widget, 0);
		}
	}
	g_list_free(children);
	return true;
}

/* meow_session_spacer_position:
 * @right_sidebar: true when Session shares a row with a Right sidebar.
 * @left_to_right: true for a left-to-right interface direction.
 *
 * Selects the spacer position that places the command buttons at the required
 * physical edge without changing the existing no-sidebar arrangement.
 *
 * Returns: the command-box child index for the expanding spacer.
 */
int
WhiskerMenu::meow_session_spacer_position(bool right_sidebar,
		bool left_to_right)
{
	if (right_sidebar)
		return left_to_right ? 9 : 0;
	return left_to_right ? 0 : 9;
}

/* meow_configure_profile_sidebar_alignment:
 * @profile: outer Profile block that owns the shared sidebar width.
 * @content: inner avatar/name group positioned within @profile.
 * @leading_spacer: logical-leading Profile spacer.
 * @trailing_spacer: logical-trailing Profile spacer.
 * @category_button: visible category-button style reference.
 * @active: whether windowed vertical-sidebar alignment is active.
 * @category_names_visible: true for leading alignment; false for centring.
 *
 * Keeps width ownership separate from content positioning. Category buttons
 * place their child after both the theme border and padding, so both metrics
 * contribute to the Profile inset. Symmetric expanding spacers centre the
 * whole group when category labels are hidden and contribute tolerance to its
 * natural width.
 *
 * Returns: the applied logical inset, 0 when inactive, or -1 when invalid.
 */
int
WhiskerMenu::meow_configure_profile_sidebar_alignment(GtkWidget* profile,
		GtkWidget* content, GtkWidget* leading_spacer,
		GtkWidget* trailing_spacer, GtkWidget* category_button,
		bool active, bool category_names_visible)
{
	if (!GTK_IS_BOX(profile) || !GTK_IS_WIDGET(content)
			|| !GTK_IS_WIDGET(leading_spacer)
			|| !GTK_IS_WIDGET(trailing_spacer)
			|| !GTK_IS_WIDGET(category_button))
		return -1;

	gtk_widget_set_margin_start(content, 0);
	gtk_widget_set_margin_end(content, 0);
	gtk_widget_set_size_request(leading_spacer, -1, -1);
	gtk_widget_set_size_request(trailing_spacer, -1, -1);
	if (gtk_widget_get_parent(leading_spacer) == profile)
		gtk_box_set_child_packing(GTK_BOX(profile), leading_spacer,
				false, false, 0, GTK_PACK_START);
	if (gtk_widget_get_parent(content) == profile)
		gtk_box_set_child_packing(GTK_BOX(profile), content,
				false, false, 0, GTK_PACK_START);
	if (gtk_widget_get_parent(trailing_spacer) == profile)
		gtk_box_set_child_packing(GTK_BOX(profile), trailing_spacer,
				true, true, 0, GTK_PACK_START);
	if (!active)
		return 0;

	GtkBorder padding = {};
	GtkBorder border = {};
	GtkStyleContext* style = gtk_widget_get_style_context(category_button);
	gtk_style_context_get_padding(style, GTK_STATE_FLAG_NORMAL, &padding);
	gtk_style_context_get_border(style, GTK_STATE_FLAG_NORMAL, &border);
	const bool rtl = gtk_widget_get_direction(category_button)
			== GTK_TEXT_DIR_RTL;
	const int inset = rtl
			? MAX(0, padding.right) + MAX(0, border.right)
			: MAX(0, padding.left) + MAX(0, border.left);
	gtk_widget_set_size_request(leading_spacer, inset > 0 ? inset : -1, -1);
	if (category_names_visible)
		return inset;

	gtk_widget_set_size_request(trailing_spacer, inset > 0 ? inset : -1, -1);
	if (gtk_widget_get_parent(leading_spacer) == profile)
		gtk_box_set_child_packing(GTK_BOX(profile), leading_spacer,
				true, true, 0, GTK_PACK_START);
	if (gtk_widget_get_parent(trailing_spacer) == profile)
		gtk_box_set_child_packing(GTK_BOX(profile), trailing_spacer,
				true, true, 0, GTK_PACK_START);
	if (gtk_widget_get_parent(content) == profile)
	{
		gtk_box_set_child_packing(GTK_BOX(profile), content,
				false, false, 0, GTK_PACK_START);
	}
	return inset;
}

/* meow_reset_vertical_sidebar_scroll:
 * @sidebar: vertical category-navigation scroller.
 *
 * Resets after category ordering and visibility settle so a prior allocation
 * cannot reopen with the fixed Applications controls above the viewport.
 *
 * Returns: true when a valid adjustment was reset.
 */
bool
WhiskerMenu::meow_reset_vertical_sidebar_scroll(GtkScrolledWindow* sidebar)
{
	if (!GTK_IS_SCROLLED_WINDOW(sidebar))
		return false;
	GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(sidebar);
	if (!GTK_IS_ADJUSTMENT(adjustment))
		return false;
	gtk_adjustment_set_value(adjustment,
			gtk_adjustment_get_lower(adjustment));
	return true;
}

bool
WhiskerMenu::meow_box_reorder_child_if_present(GtkBox* box, GtkWidget* child,
                                               gint position)
{
	if (!meow_box_contains_child(box, child))
		return false;

	gtk_box_reorder_child(box, child, position);
	return true;
}

void
WhiskerMenu::meow_widget_set_visible_if_valid(GtkWidget* widget, bool visible)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_visible(widget, visible);
}

void
WhiskerMenu::meow_widget_set_can_focus_if_valid(GtkWidget* widget,
                                                bool can_focus)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_can_focus(widget, can_focus);
}

void
WhiskerMenu::meow_widget_set_hexpand_if_valid(GtkWidget* widget, bool expand)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_hexpand(widget, expand);
}

void
WhiskerMenu::meow_widget_set_vexpand_if_valid(GtkWidget* widget, bool expand)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_vexpand(widget, expand);
}

void
WhiskerMenu::meow_widget_set_halign_if_valid(GtkWidget* widget, GtkAlign align)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_halign(widget, align);
}

void
WhiskerMenu::meow_widget_set_valign_if_valid(GtkWidget* widget, GtkAlign align)
{
	if (GTK_IS_WIDGET(widget))
		gtk_widget_set_valign(widget, align);
}
