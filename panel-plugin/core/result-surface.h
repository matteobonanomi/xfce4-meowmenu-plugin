/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEOWMENU_RESULT_SURFACE_H
#define MEOWMENU_RESULT_SURFACE_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class LauncherView;

/* ResultSurface:
 *
 * Provides the generic presentation operations shared by Applications and
 * Places results. The interface is non-owning: callers must not retain it past
 * the lifetime of the concrete page owned by Window.
 *
 * Activation, menus, drag payloads, asynchronous publication, focus leases,
 * and mapped-frame lifetime remain responsibilities of the concrete pages.
 */
class ResultSurface
{
public:
	virtual ~ResultSurface() = default;

	virtual GtkWidget* get_widget() const = 0;
	virtual LauncherView* get_view() const = 0;
	virtual void present() = 0;
	virtual void prepare_viewport_resize(int current_toplevel_width,
			int requested_toplevel_width) = 0;
	virtual int get_viewport_width() const = 0;
	virtual int get_minimum_viewport_width() const = 0;
	virtual void select_first() = 0;
	virtual bool focus_first_result() = 0;
};

}

#endif // MEOWMENU_RESULT_SURFACE_H
