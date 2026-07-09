/*
 * Copyright (C) 2026 Matteo Bonanomi
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "launcher-safety.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

bool WhiskerMenu::launcher_model_get_iter(GtkTreeModel* model,
		GtkTreePath* path, GtkTreeIter* iter)
{
	return model && path && iter && gtk_tree_model_get_iter(model, iter, path);
}

//-----------------------------------------------------------------------------

gchar** WhiskerMenu::launcher_editor_argv(const gchar* editor, const gchar* uri)
{
	if (!editor || !*editor || !uri || !*uri)
	{
		return nullptr;
	}

	gchar** argv = g_new0(gchar*, 3);
	argv[0] = g_strdup(editor);
	argv[1] = g_strdup(uri);
	return argv;
}

//-----------------------------------------------------------------------------

gchar* WhiskerMenu::launcher_hide_relpath_for_uri(const gchar* uri,
		const gchar* applications_dir)
{
	if (!uri || !applications_dir)
	{
		return nullptr;
	}

	GFile* source = g_file_new_for_uri(uri);
	if (!g_file_is_native(source))
	{
		g_object_unref(source);
		return nullptr;
	}

	GFile* applications = g_file_new_for_path(applications_dir);
	gchar* filename = g_file_get_relative_path(applications, source);
	gchar* relpath = (filename && *filename)
			? g_build_filename("applications", filename, nullptr)
			: nullptr;

	g_free(filename);
	g_object_unref(applications);
	g_object_unref(source);
	return relpath;
}
