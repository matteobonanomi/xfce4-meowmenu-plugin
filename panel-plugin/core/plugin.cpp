/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
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

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#include "version.h"
#endif

#include "plugin.h"

#include "launcher/applications-page.h"
#include "launcher/command.h"
#include "migration.h"
#include "settings.h"
#include "settings-dialog.h"
#include "ui/slot.h"
#include "window.h"
#include "window-size-clamp.h"

#include <glib/gstdio.h>
#include <libxfce4ui/libxfce4ui.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

extern "C"
{

#include "register-plugin.h"

void meowmenu_construct(XfcePanelPlugin* plugin)
{
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
	new Plugin(plugin);
}

}

//-----------------------------------------------------------------------------

Plugin::Plugin(XfcePanelPlugin* plugin) :
	m_plugin(plugin),
	m_window(nullptr),
	m_settings_dialog(nullptr),
	m_button(nullptr),
	m_opacity(100),
	m_file_icon(false),
	m_autohide_blocked(false),
	m_hide_time(0)
{
	// Create settings
	m_settings = new Settings(this);

	// Load default settings
	gchar* defaults_file = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, "xfce4/meowmenu/defaults.rc");
	m_settings->load(defaults_file, true);
	g_free(defaults_file);

	gchar* rc_file = xfce_panel_plugin_lookup_rc_file(m_plugin);
	gchar* save_file = xfce_panel_plugin_save_location(m_plugin, false);
	if (g_strcmp0(rc_file, save_file) != 0)
	{
		m_settings->load(rc_file, true);
	}
	g_free(rc_file);

	// Load user settings
	m_settings->load(xfce_panel_plugin_get_property_base(m_plugin));

	// One-shot pre-rename Xfconf migration. See migration.h/cpp for
	// the legacy-vs-current base mapping and the Whisker-presence gate.
	{
		XfconfChannel* panel_channel = xfconf_channel_new(xfce_panel_get_channel_name());
		WhiskerMenu::migrate_legacy_xfconf(panel_channel,
				xfce_panel_plugin_get_property_base(m_plugin));
		g_object_unref(panel_channel);
	}

	// Migrate old user settings if they exist
	if (m_settings->channel)
	{
		m_settings->load(save_file, false);
		g_remove(save_file);
	}
	g_free(save_file);

	m_opacity = m_settings->menu_opacity;

	// Switch to new icon only if theme is missing old icon
	if ((m_settings->button_icon_name == "xfce4-whiskermenu")
			&& !gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "xfce4-whiskermenu"))
	{
		m_settings->button_icon_name = "org.xfce.panel.meowmenu";
	}

	// Create toggle button
	m_button = xfce_panel_create_toggle_button();
	gtk_widget_set_name(m_button, "meowmenu-button");
	connect(m_button, "button-press-event",
		[this](GtkWidget* widget, GdkEvent* event) -> gboolean
		{
			GdkEventButton* button_event = reinterpret_cast<GdkEventButton*>(event);
			if ((button_event->type != GDK_BUTTON_PRESS) || (button_event->button != 1))
			{
				return GDK_EVENT_PROPAGATE;
			}

			GtkToggleButton* button GTK_TOGGLE_BUTTON(widget);
			if (!gtk_toggle_button_get_active(button))
			{
				show_menu(Window::PositionAtButton);
			}
			else
			{
				m_window->hide();
			}
			return GDK_EVENT_STOP;
		});
	gtk_widget_show(m_button);

	m_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
	gtk_container_add(GTK_CONTAINER(m_button), GTK_WIDGET(m_button_box));
	gtk_container_set_border_width(GTK_CONTAINER(m_button_box), 0);
	gtk_widget_show(GTK_WIDGET(m_button_box));

	m_button_icon = GTK_IMAGE(gtk_image_new());
	icon_changed(m_settings->button_icon_name);
	gtk_widget_set_tooltip_markup(m_button, m_settings->button_title);
	gtk_box_pack_start(m_button_box, GTK_WIDGET(m_button_icon), true, false, 0);
	if (m_settings->button_icon_visible)
	{
		gtk_widget_show(GTK_WIDGET(m_button_icon));
	}
	if (m_settings->button_title_visible)
	{
		gtk_widget_set_has_tooltip(m_button, false);
	}
	gtk_widget_set_sensitive(GTK_WIDGET(m_button_icon), false);

	m_button_label = GTK_LABEL(gtk_label_new(nullptr));
	gtk_label_set_markup(m_button_label, m_settings->button_title);
	gtk_box_pack_start(m_button_box, GTK_WIDGET(m_button_label), true, true, 0);
	if (m_settings->button_title_visible)
	{
		gtk_widget_show(GTK_WIDGET(m_button_label));
	}
	gtk_widget_set_sensitive(GTK_WIDGET(m_button_label), false);

	// Add plugin to panel
	gtk_container_add(GTK_CONTAINER(plugin), m_button);
	xfce_panel_plugin_add_action_widget(plugin, m_button);

	// Connect plugin signals to functions
	connect(m_plugin, "free-data",
		[this](XfcePanelPlugin*)
		{
			delete this;
		});

	connect(m_plugin, "configure-plugin",
		[this](XfcePanelPlugin*)
		{
			configure();
		});

	connect(m_plugin, "mode-changed",
		[this](XfcePanelPlugin*, XfcePanelPluginMode mode)
		{
			mode_changed(mode);
		});

	connect(m_plugin, "remote-event",
		[this](XfcePanelPlugin*, const gchar* name, const GValue* value) -> gboolean
		{
			return remote_event(name, value);
		});

	connect(m_plugin, "about",
		[this](XfcePanelPlugin*)
		{
			show_about();
		});

	connect(m_plugin, "size-changed",
		[this](XfcePanelPlugin*, gint size) -> gboolean
		{
			return size_changed(size);
		});

	xfce_panel_plugin_menu_show_about(plugin);
	xfce_panel_plugin_menu_show_configure(plugin);
	xfce_panel_plugin_menu_insert_item(plugin, GTK_MENU_ITEM(m_settings->command[Settings::CommandMenuEditor]->get_menuitem()));

	mode_changed(xfce_panel_plugin_get_mode(m_plugin));

	// Create menu window
	m_window = new Window(m_settings, this);
	connect(m_window->get_widget(), "hide",
		[this](GtkWidget*)
		{
			m_hide_time = g_get_monotonic_time();
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button), false);
			if (m_autohide_blocked)
			{
				xfce_panel_plugin_block_autohide(m_plugin, false);
			}
			m_autohide_blocked = false;
		});
}

//-----------------------------------------------------------------------------

Plugin::~Plugin()
{
	delete m_window;
	m_window = nullptr;

	gtk_widget_destroy(m_button);

	delete m_settings;
	m_settings = nullptr;
}

//-----------------------------------------------------------------------------

Plugin::ButtonStyle Plugin::get_button_style() const
{
	return ButtonStyle(m_settings->button_icon_visible | (m_settings->button_title_visible << 1));
}

//-----------------------------------------------------------------------------

std::string Plugin::get_button_title_default() const
{
	return m_settings->m_button_title_default;
}

//-----------------------------------------------------------------------------

std::string Plugin::get_property_base() const
{
	// Owned by the plugin; not freed here. May be NULL very early in setup.
	const gchar* base = xfce_panel_plugin_get_property_base(m_plugin);
	return base ? base : "";
}

//-----------------------------------------------------------------------------

void Plugin::get_menu_position(int* x, int* y) const
{
	xfce_panel_plugin_position_widget(m_plugin, m_window->get_widget(), m_button, x, y);
}

//-----------------------------------------------------------------------------

XfceScreenPosition Plugin::get_screen_position() const
{
	return xfce_panel_plugin_get_screen_position(m_plugin);
}

//-----------------------------------------------------------------------------

void Plugin::menu_hidden()
{
	m_hide_time = 0;
}

//-----------------------------------------------------------------------------

void Plugin::reload_button()
{
	if (m_button)
	{
		m_settings->prevent_invalid();
		icon_changed(m_settings->button_icon_name);
		set_button_style(get_button_style());
	}
}

//-----------------------------------------------------------------------------

void Plugin::reload_menu()
{
	if (m_window)
	{
		m_window->hide();
		m_window->get_applications()->invalidate();
	}
}

//-----------------------------------------------------------------------------

void Plugin::set_button_style(ButtonStyle style)
{
	m_settings->button_icon_visible = style & ShowIcon;
	if (m_settings->button_icon_visible)
	{
		gtk_widget_show(GTK_WIDGET(m_button_icon));
	}
	else
	{
		gtk_widget_hide(GTK_WIDGET(m_button_icon));
	}

	m_settings->button_title_visible = style & ShowText;
	if (m_settings->button_title_visible)
	{
		gtk_widget_show(GTK_WIDGET(m_button_label));
		gtk_widget_set_has_tooltip(m_button, false);
	}
	else
	{
		gtk_widget_hide(GTK_WIDGET(m_button_label));
		gtk_widget_set_has_tooltip(m_button, true);
	}

	update_size();
}

//-----------------------------------------------------------------------------

void Plugin::set_button_title(const std::string& title)
{
	m_settings->button_title = title;
	gtk_label_set_markup(m_button_label, m_settings->button_title);
	gtk_widget_set_tooltip_markup(m_button, m_settings->button_title);
	gtk_widget_set_has_tooltip(m_button, !m_settings->button_title_visible);
	update_size();
}

//-----------------------------------------------------------------------------

void Plugin::set_button_icon_name(const std::string& icon)
{
	m_settings->button_icon_name = icon;
	icon_changed(icon.c_str());
	update_size();
}

//-----------------------------------------------------------------------------

void Plugin::set_loaded(bool loaded)
{
	gtk_widget_set_sensitive(GTK_WIDGET(m_button_icon), loaded);
	gtk_widget_set_sensitive(GTK_WIDGET(m_button_label), loaded);
}

//-----------------------------------------------------------------------------

void Plugin::configure()
{
	if (m_settings_dialog)
	{
		gtk_window_present(GTK_WINDOW(m_settings_dialog->get_widget()));
		return;
	}

	m_settings_dialog = new SettingsDialog(m_settings, this);
	connect(m_settings_dialog->get_widget(), "destroy",
		[this](GtkWidget*)
		{
			m_settings->search_actions.save();
			delete m_settings_dialog;
			m_settings_dialog = nullptr;
		});
}

//-----------------------------------------------------------------------------

void Plugin::icon_changed(const gchar* icon)
{
	if (!g_path_is_absolute(icon))
	{
		gtk_image_set_from_icon_name(m_button_icon, icon, GTK_ICON_SIZE_BUTTON);
		m_file_icon = false;
	}
	else
	{
		gtk_image_clear(m_button_icon);
		m_file_icon = true;
	}
}

//-----------------------------------------------------------------------------

void Plugin::mode_changed(XfcePanelPluginMode mode)
{
	gtk_label_set_angle(m_button_label, (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) ? 270: 0);
	update_size();
}

//-----------------------------------------------------------------------------

gboolean Plugin::remote_event(const gchar* name, const GValue* value)
{
	if (strcmp(name, "popup"))
	{
		return false;
	}

	// Ignore event if menu lost focus and hid within last 1/4 second;
	// needed for toggling as remote event happens after focus is lost
	if (m_hide_time)
	{
		const gint64 delta = g_get_monotonic_time() - m_hide_time;
		m_hide_time = 0;
		if (delta < 250000)
		{
			return true;
		}
	}

	if (gtk_widget_get_visible(m_window->get_widget()))
	{
		m_window->hide();
	}
	else
	{
		show_menu((value && G_VALUE_HOLDS_INT(value)) ? g_value_get_int(value) : Window::PositionAtButton);
	}

	return true;
}

//-----------------------------------------------------------------------------

/* Plugin::show_news_dialog:
 * @parent: transient parent window for the dialog.
 *
 * Reads the installed NEWS file from PACKAGE_DATADIR and shows its content
 * in a scrollable, read-only text dialog. Falls back to a short message when
 * the file cannot be found (e.g. during an uninstalled development build).
 */
static void show_news_dialog(GtkWindow* parent)
{
	gchar* news_text = nullptr;
	gchar* news_path = g_build_filename(PACKAGE_DATADIR, "NEWS", nullptr);

	if (!g_file_get_contents(news_path, &news_text, nullptr, nullptr))
	{
		news_text = g_strdup(_("News file not found.\n"
			"Run 'meson install' to make it available."));
	}
	g_free(news_path);

	GtkWidget* dialog = gtk_dialog_new_with_buttons(
		_("What's New"),
		parent,
		static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
		_("Close"), GTK_RESPONSE_CLOSE,
		nullptr);
	// Clamp the 520x420 logical default to the active monitor's work area so
	// the dialog can never open off-screen or larger than the screen at very
	// large effective scales. Shrink-only, so at a normal 1x work area the
	// size passes through unchanged.
	//
	// HACK: the dialog is not realized yet here, so we resolve the monitor
	// under its (already-shown) parent when possible, falling back to the
	// primary (then first) monitor. If no monitor can be resolved at all,
	// the work area is left 0x0, which the clamp treats as "no constraint"
	// and returns the size as-is.
	{
		GdkRectangle workarea = { 0, 0, 0, 0 };
		GdkDisplay* display = gtk_widget_get_display(dialog);
		GdkMonitor* monitor = nullptr;
		if (parent)
		{
			if (GdkWindow* pwin = gtk_widget_get_window(GTK_WIDGET(parent)))
				monitor = gdk_display_get_monitor_at_window(display, pwin);
		}
		if (!monitor)
			monitor = gdk_display_get_primary_monitor(display);
		if (!monitor && gdk_display_get_n_monitors(display) > 0)
			monitor = gdk_display_get_monitor(display, 0);
		if (monitor)
			gdk_monitor_get_workarea(monitor, &workarea);

		int clamped_w = 0, clamped_h = 0;
		meow::clamp_default_size(520, 420, workarea.width, workarea.height,
			&clamped_w, &clamped_h);
		gtk_window_set_default_size(GTK_WINDOW(dialog), clamped_w, clamped_h);
	}

	GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_margin_start(scrolled, 6);
	gtk_widget_set_margin_end(scrolled, 6);
	gtk_widget_set_margin_top(scrolled, 6);
	gtk_widget_set_margin_bottom(scrolled, 6);

	GtkWidget* text_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), false);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), false);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 8);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 8);

	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
	gtk_text_buffer_set_text(buffer, news_text, -1);
	g_free(news_text);

	gtk_container_add(GTK_CONTAINER(scrolled), text_view);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
		scrolled, true, true, 0);
	gtk_widget_show_all(dialog);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

/* Plugin::show_about:
 *
 * Shows the MeowMenu About dialog. Adds a "What's New" button that opens
 * the NEWS file in a separate scrollable dialog without closing the About
 * dialog. Credits section acknowledges the original Whisker Menu project.
 */
void Plugin::show_about()
{
	// NOTE: response ID 1 is user-defined; avoids collisions with GTK built-in IDs.
	static const gint RESPONSE_NEWS = 1;

	const gchar* authors[] = {
		"Matteo Bonanomi",
		nullptr };

	const gchar* whisker_credits[] = {
		"Graeme Gott <graeme@gottcode.org>",
		"https://gitlab.xfce.org/panel-plugins/xfce4-whiskermenu-plugin",
		"https://gottcode.org",
		nullptr };

	GtkWidget* dialog = gtk_about_dialog_new();
	GtkAboutDialog* about = GTK_ABOUT_DIALOG(dialog);

	gtk_about_dialog_set_program_name(about, "MeowMenu");
	gtk_about_dialog_set_version(about, MEOWMENU_VERSION " (" MEOWMENU_RELEASE_DATE ")");
	gtk_about_dialog_set_comments(about, _("Alternate launcher for XFCE4"));
	gtk_about_dialog_set_website(about, PLUGIN_WEBSITE);
	gtk_about_dialog_set_copyright(about, "Copyright \302\251 2026 Matteo Bonanomi");
	gtk_about_dialog_set_license_type(about, GTK_LICENSE_GPL_2_0);
	gtk_about_dialog_set_authors(about, authors);
	gtk_about_dialog_set_logo_icon_name(about, "org.xfce.panel.meowmenu");
	gtk_about_dialog_add_credit_section(about, _("Based on Whisker Menu"), whisker_credits);

	gtk_dialog_add_button(GTK_DIALOG(dialog), _("What's New"), RESPONSE_NEWS);

	gint response;
	do
	{
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		if (response == RESPONSE_NEWS)
		{
			show_news_dialog(GTK_WINDOW(dialog));
		}
	}
	while (response == RESPONSE_NEWS);

	gtk_widget_destroy(dialog);
}

//-----------------------------------------------------------------------------

gboolean Plugin::size_changed(gint size)
{
	GtkOrientation panel_orientation = xfce_panel_plugin_get_orientation(m_plugin);
	GtkOrientation orientation = panel_orientation;
	XfcePanelPluginMode mode = xfce_panel_plugin_get_mode(m_plugin);

	// Make icon expand to fill button if title is not visible
	gtk_box_set_child_packing(GTK_BOX(m_button_box), GTK_WIDGET(m_button_icon),
			!m_settings->button_title_visible,
			!m_settings->button_title_visible,
			0, GTK_PACK_START);

	// Resize icon
	if (m_settings->button_single_row)
	{
		size /= xfce_panel_plugin_get_nrows(m_plugin);
	}
	gint icon_size = xfce_panel_plugin_get_icon_size(m_plugin);
	if (!m_settings->button_single_row)
	{
		icon_size *= xfce_panel_plugin_get_nrows(m_plugin);
	}
	gtk_image_set_pixel_size(m_button_icon, icon_size);

	// Load icon from absolute path
	if (m_file_icon)
	{
		const gint scale = gtk_widget_get_scale_factor(m_button);
		gint max_width = icon_size * scale;
		gint max_height = icon_size * scale;
		if (mode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
		{
			max_width *= 6;
		}
		else
		{
			max_height *= 6;
		}

		GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_size(m_settings->button_icon_name, max_width, max_height, nullptr);
		if (pixbuf)
		{
			// Handle high dpi
			cairo_surface_t* surface = gdk_cairo_surface_create_from_pixbuf(pixbuf, scale, nullptr);
			gtk_image_set_from_surface(m_button_icon, surface);
			cairo_surface_destroy(surface);
			g_object_unref(pixbuf);
		}
	}

	// Make panel button square only if single row and title hidden
	if (!m_settings->button_title_visible && (m_settings->button_single_row || (xfce_panel_plugin_get_nrows(m_plugin) == 1)))
	{
		gtk_widget_set_size_request(m_button, size, size);
	}
	else
	{
		gtk_widget_set_size_request(m_button, -1, -1);
	}

	// Use single panel row if requested and title hidden
	if (m_settings->button_title_visible || !m_settings->button_single_row)
	{
		xfce_panel_plugin_set_small(m_plugin, false);

		// Put title next to icon if panel is wide enough
		GtkRequisition label_size;
		gtk_widget_get_preferred_size(GTK_WIDGET(m_button_label), nullptr, &label_size);
		if (mode == XFCE_PANEL_PLUGIN_MODE_DESKBAR &&
				m_settings->button_title_visible &&
				m_settings->button_icon_visible &&
				label_size.width <= (size - icon_size - 4))
		{
			orientation = GTK_ORIENTATION_HORIZONTAL;
		}
	}
	else
	{
		xfce_panel_plugin_set_small(m_plugin, true);
	}

	// Fix alignment in deskbar mode
	if ((panel_orientation == GTK_ORIENTATION_VERTICAL) && (orientation == GTK_ORIENTATION_HORIZONTAL))
	{
		gtk_box_set_child_packing(m_button_box, GTK_WIDGET(m_button_label), false, false, 0, GTK_PACK_START);
	}
	else
	{
		gtk_box_set_child_packing(m_button_box, GTK_WIDGET(m_button_label), true, true, 0, GTK_PACK_START);
	}

	gtk_orientable_set_orientation(GTK_ORIENTABLE(m_button_box), orientation);

	return true;
}

//-----------------------------------------------------------------------------

void Plugin::update_size()
{
	size_changed(xfce_panel_plugin_get_size(m_plugin));
}

//-----------------------------------------------------------------------------

void Plugin::show_menu(int position)
{
	if (m_settings->menu_opacity != m_opacity)
	{
		if ((m_opacity == 100) || (m_settings->menu_opacity == 100))
		{
			delete m_window;
			m_window = new Window(m_settings, this);
			connect(m_window->get_widget(), "hide",
				[this](GtkWidget*)
				{
					m_hide_time = g_get_monotonic_time();
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button), false);
					if (m_autohide_blocked)
					{
						xfce_panel_plugin_block_autohide(m_plugin, false);
					}
					m_autohide_blocked = false;
				});
		}
		m_opacity = m_settings->menu_opacity;
	}

	position = CLAMP(position, Window::PositionAtButton, Window::PositionAtCenter);
	if (position == Window::PositionAtButton)
	{
		m_autohide_blocked = true;
		xfce_panel_plugin_block_autohide(m_plugin, true);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_button), true);
	}
	m_window->show(Window::Position(position));
	m_hide_time = 0;
}

//-----------------------------------------------------------------------------
