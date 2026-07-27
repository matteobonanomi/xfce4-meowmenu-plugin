/*
 * Copyright (C) 2026 MeowMenu contributors
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "places-item.h"

#include <cstring>

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

PlacesItem::PlacesItem(GFile* file, bool is_favourite) :
	m_file(file ? G_FILE(g_object_ref(file)) : nullptr),
	m_accessed(0),
	m_exists(false),
	m_is_directory(false),
	m_is_favourite(is_favourite)
{
	if (!m_file)
	{
		set_text("");
		set_tooltip(nullptr);
		return;
	}

	gchar* uri = g_file_get_uri(m_file);
	m_uri = uri ? uri : "";
	g_free(uri);

	// NOTE: query G_FILE_ATTRIBUTE_STANDARD_ICON synchronously — the items are
	// few (≤30) and the inode/MIME info is cached by GIO.
	GError* error = nullptr;
	GFileInfo* info = g_file_query_info(m_file,
			G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
			G_FILE_ATTRIBUTE_STANDARD_ICON ","
			G_FILE_ATTRIBUTE_STANDARD_TYPE,
			G_FILE_QUERY_INFO_NONE,
			nullptr, &error);

	gchar* path = g_file_get_path(m_file);
	const gchar* display = nullptr;
	if (info)
	{
		m_exists = true;
		m_is_directory = g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY;
		display = g_file_info_get_display_name(info);
		GIcon* icon = g_file_info_get_icon(info);
		if (icon)
		{
			set_icon_gicon(icon);
		}
	}
	else
	{
		if (error) g_error_free(error);
		// Missing/unreachable: still want a usable label and a generic icon.
		set_icon("text-x-generic");
	}

	if (display && *display)
	{
		set_text(display);
	}
	else
	{
		gchar* basename = path ? g_path_get_basename(path) : g_file_get_basename(m_file);
		set_text(basename ? basename : "");
		g_free(basename);
	}

	const char* label = get_text() ? get_text() : "";
	if (m_exists)
	{
		// Available: keep today's plain label and the path/URI tooltip.
		m_display_markup = label;
		set_tooltip(path ? path : m_uri.c_str());
	}
	else
	{
		// Missing: mute the label and flag the target as gone.
		// NOTE: escape the display name before wrapping it — the shared
		// tree-view renders COLUMN_TEXT as Pango markup, so an unescaped
		// "&" or "<" in a file name would corrupt the row.
		gchar* escaped = g_markup_escape_text(label, -1);
		// NOTE: dim via alpha over the inherited theme foreground rather than
		// a fixed colour, so the muted text stays legible on light and dark
		// themes (no hard-coded colour).
		gchar* markup = g_strdup_printf("<span alpha=\"55%%\">%s</span>",
				escaped ? escaped : "");
		m_display_markup = markup ? markup : "";
		g_free(markup);
		g_free(escaped);

		// Tooltip names the target and states it is missing; set_tooltip()
		// escapes the text for markup, so pass it raw.
		gchar* tip = g_strdup_printf(_("%s (missing)"), path ? path : m_uri.c_str());
		set_tooltip(tip);
		g_free(tip);
	}

	gchar* folded = g_utf8_casefold(get_text(), -1);
	m_casefolded_name = folded ? folded : "";
	g_free(folded);

	g_free(path);
	if (info)
	{
		g_object_unref(info);
	}
}

//-----------------------------------------------------------------------------

PlacesItem::~PlacesItem()
{
	if (m_file)
	{
		g_object_unref(m_file);
	}
}

//-----------------------------------------------------------------------------

/* open:
 * @screen: GdkScreen used to launch on the correct display.
 * @parent: a widget in the Places page used to parent the failure dialog; may
 *          be nullptr (the dialog is then unparented).
 *
 * Opens the file or folder with the system-default handler via GIO. Folders
 * fall back to "exo-open --launch FileManager <path>" on launch failure, with
 * the path shell-quoted (the documented behavior) so names containing spaces or shell-significant
 * characters open the exact target. On failure exactly one error dialog names
 * the item and states the underlying reason (the documented behavior).
 *
 * NOTE: the caller dispatches this before closing the menu so the item is not
 * freed mid-open; see PlacesPage::on_row_activated().
 */
void PlacesItem::open(GdkScreen* screen, GtkWidget* parent)
{
	if (m_uri.empty())
	{
		return;
	}

	GdkAppLaunchContext* ctx = gdk_display_get_app_launch_context(
			gdk_screen_get_display(screen ? screen : gdk_screen_get_default()));

	GError* error = nullptr;
	const gboolean ok = g_app_info_launch_default_for_uri(
			m_uri.c_str(), G_APP_LAUNCH_CONTEXT(ctx), &error);
	if (ctx) g_object_unref(ctx);

	if (ok)
	{
		return;
	}

	if (m_is_directory)
	{
		gchar* path = g_file_get_path(m_file);
		if (path)
		{
			// the documented behavior: quote the path so the helper receives the exact folder
			// even with spaces, quotes, '$', backticks, or parentheses. The
			// command is parsed by g_shell_parse_argv inside Element::spawn().
			gchar* quoted = g_shell_quote(path);
			gchar* command = g_strdup_printf("exo-open --launch FileManager %s", quoted);
			// the documented behavior: Element::spawn() owns the single failure dialog for this
			// fallback, so the default-handler error above is dropped here to
			// avoid stacking a second dialog.
			spawn(screen, command, nullptr, true, nullptr);
			g_free(command);
			g_free(quoted);
			g_free(path);
			if (error) g_error_free(error);
			return;
		}
		g_free(path);
	}

	// the documented behavior: exactly one dialog, naming the item with a legible reason.
	gchar* message = build_open_error_message(error, get_text());
	GtkWindow* window = parent
			? GTK_WINDOW(gtk_widget_get_toplevel(parent)) : nullptr;
	xfce_dialog_show_error(window, nullptr, "%s", message);
	g_free(message);
	if (error) g_error_free(error);
}

//-----------------------------------------------------------------------------

/* open_containing:
 * @screen: GdkScreen used to launch on the correct display.
 * @parent: accepted so the Places open paths share a uniform (screen, parent)
 *          signature; this path shows no dialog of its own, so it is unused.
 *
 * Opens the item's parent folder with the default handler. the documented behavior: when the
 * item has no parent (e.g. a filesystem root) this is a defined silent no-op,
 * and a failed launch is intentionally not reported — never a blank dialog.
 */
void PlacesItem::open_containing(GdkScreen* screen, GtkWidget* /*parent*/)
{
	if (!m_file)
	{
		return;
	}
	GFile* parent_dir = g_file_get_parent(m_file);
	if (!parent_dir)
	{
		// Defined no-op: no parent to open (the documented behavior). Never a blank dialog.
		return;
	}
	gchar* uri = g_file_get_uri(parent_dir);
	if (uri)
	{
		GdkAppLaunchContext* ctx = gdk_display_get_app_launch_context(
				gdk_screen_get_display(screen ? screen : gdk_screen_get_default()));
		// NOTE: launch result is intentionally ignored — the documented behavior defines this as
		// a silent no-op on failure rather than an error dialog.
		g_app_info_launch_default_for_uri(uri, G_APP_LAUNCH_CONTEXT(ctx), nullptr);
		if (ctx) g_object_unref(ctx);
		g_free(uri);
	}
	g_object_unref(parent_dir);
}

//-----------------------------------------------------------------------------

void PlacesItem::copy_path()
{
	if (!m_file)
	{
		return;
	}
	gchar* path = g_file_get_path(m_file);
	GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, path ? path : m_uri.c_str(), -1);
	g_free(path);
}

//-----------------------------------------------------------------------------

/* open_in_terminal:
 * @screen: GdkScreen used to launch on the correct display.
 * @parent: accepted so the Places open paths share a uniform (screen, parent)
 *          signature; Element::spawn() owns any failure dialog, so it is unused.
 *
 * Opens a terminal with its working directory set to this folder, via Xfce's
 * TerminalEmulator launch alias.
 */
void PlacesItem::open_in_terminal(GdkScreen* screen, GtkWidget* /*parent*/)
{
	if (!m_file || !m_is_directory)
	{
		return;
	}
	gchar* path = g_file_get_path(m_file);
	if (!path)
	{
		return;
	}
	// NOTE: defer to Xfce's TerminalEmulator launch alias; this honors the
	// user's configured terminal without parsing xfce4-session.xml ourselves.
	// the documented behavior: quote the working directory so the terminal opens in the exact
	// folder even when its name contains shell-significant characters.
	gchar* quoted = g_shell_quote(path);
	gchar* command = g_strdup_printf("exo-open --launch TerminalEmulator --working-directory %s", quoted);
	spawn(screen, command, path, true, nullptr);
	g_free(command);
	g_free(quoted);
	g_free(path);
}

//-----------------------------------------------------------------------------

void PlacesItem::open_with(GtkWidget* parent)
{
	if (!m_file || m_is_directory)
	{
		return;
	}

	GFileInfo* info = g_file_query_info(m_file,
			G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
	const gchar* content_type = info ? g_file_info_get_content_type(info) : nullptr;

	GtkWidget* dialog = gtk_app_chooser_dialog_new_for_content_type(
			parent ? GTK_WINDOW(gtk_widget_get_toplevel(parent)) : nullptr,
			GTK_DIALOG_MODAL,
			content_type ? content_type : "application/octet-stream");

	GError* error = nullptr;
	gboolean launch_failed = FALSE;
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		GAppInfo* app = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(dialog));
		if (app)
		{
			GList* files = g_list_append(nullptr, m_file);
			launch_failed = !g_app_info_launch(app, files, nullptr, &error);
			g_list_free(files);
			g_object_unref(app);
		}
	}
	// Destroy the chooser before reporting, so the error dialog is not parented
	// to a window that is about to disappear.
	gtk_widget_destroy(dialog);
	if (info) g_object_unref(info);

	if (launch_failed)
	{
		// the documented behavior: a failed launch is explained, not silently swallowed.
		gchar* message = build_open_error_message(error, get_text());
		xfce_dialog_show_error(
				parent ? GTK_WINDOW(gtk_widget_get_toplevel(parent)) : nullptr,
				nullptr, "%s", message);
		g_free(message);
	}
	if (error) g_error_free(error);
}

//-----------------------------------------------------------------------------

void PlacesItem::add_desktop_link(GtkWidget* /*parent*/)
{
	if (!m_file)
	{
		return;
	}
	const gchar* desktop = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
	if (!desktop)
	{
		return;
	}
	gchar* basename = g_file_get_basename(m_file);
	gchar* target = g_build_filename(desktop, basename, nullptr);
	g_free(basename);

	gchar* source_path = g_file_get_path(m_file);
	if (source_path)
	{
		GError* error = nullptr;
		// HACK: a true freedesktop .desktop file would be richer, but a plain
		// symlink renders correctly on every Xfce desktop and avoids the
		// "untrusted launcher" prompt.
		GFile* link = g_file_new_for_path(target);
		if (!g_file_make_symbolic_link(link, source_path, nullptr, &error))
		{
			xfce_dialog_show_error(nullptr, error, _("Unable to add desktop link."));
			if (error) g_error_free(error);
		}
		g_object_unref(link);
		g_free(source_path);
	}
	g_free(target);
}

//-----------------------------------------------------------------------------

bool PlacesItem::search(const gchar* casefolded_filter) const
{
	if (!casefolded_filter || !*casefolded_filter)
	{
		return true;
	}
	if (m_casefolded_name.empty())
	{
		return false;
	}
	return strstr(m_casefolded_name.c_str(), casefolded_filter) != nullptr;
}

//-----------------------------------------------------------------------------

gchar* PlacesItem::places_filter_casefold(const gchar* filter)
{
	if (!filter || !*filter)
	{
		return nullptr;
	}
	return g_utf8_casefold(filter, -1);
}

//-----------------------------------------------------------------------------

gchar* PlacesItem::build_open_error_message(const GError* error, const char* display_name)
{
	const char* name = (display_name && *display_name)
			? display_name : _("the selected item");

	// Prefer the underlying, already-localized GIO/spawn reason; fall back to a
	// defined non-empty phrasing so a failed open is never reported blank.
	const char* reason = (error && error->message && *error->message)
			? error->message : _("the location could not be opened");

	gchar* raw = g_strdup_printf(_("Unable to open \"%s\": %s"), name, reason);

	// Walk the message one UTF-8 character at a time, dropping invalid bytes and
	// control characters, so a corrupt reason can never render as a garbled
	// dialog. g_utf8_get_char_validated() flags malformed/incomplete sequences;
	// such bytes are skipped rather than copied.
	GString* clean = g_string_new(nullptr);
	const char* p = raw;
	while (*p)
	{
		gunichar c = g_utf8_get_char_validated(p, -1);
		if (c == static_cast<gunichar>(-1) || c == static_cast<gunichar>(-2))
		{
			// Invalid or incomplete sequence: drop one byte and resync.
			++p;
			continue;
		}
		const char* next = g_utf8_next_char(p);
		if (c >= 0x20 && c != 0x7f)
		{
			g_string_append_len(clean, p, next - p);
		}
		p = next;
	}
	g_free(raw);

	return g_string_free(clean, FALSE);
}

//-----------------------------------------------------------------------------
