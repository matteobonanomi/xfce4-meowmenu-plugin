#include "gtk-fixture.h"

#include <glib.h>

namespace
{

bool display_checked = false;
bool display_available = false;

}

namespace meowmenu_test
{

bool ensure_display(DisplayRequirement requirement, const char* description)
{
	if (g_strcmp0(g_getenv("MEOWMENU_REQUIRE_DISPLAY"), "1") == 0)
		requirement = DisplayRequirement::ContextRequired;
	if (!display_checked)
	{
		display_available = gtk_init_check(nullptr, nullptr);
		display_checked = true;
	}
	if (display_available)
		return true;
	if (requirement == DisplayRequirement::DeveloperOptional)
	{
		g_test_message("# SKIP: GTK display unavailable for %s",
				description ? description : "optional fixture");
		g_test_skip("GTK display unavailable");
	}
	else
	{
		g_test_message("required GTK display unavailable for %s",
				description ? description : "required fixture");
		g_test_fail();
	}
	return false;
}

ScopedWidget::ScopedWidget()
	: widget_(nullptr)
{
}

ScopedWidget::ScopedWidget(GtkWidget* widget)
	: widget_(nullptr)
{
	reset(widget);
}

ScopedWidget::~ScopedWidget()
{
	if (widget_)
		g_object_unref(widget_);
}

void ScopedWidget::reset(GtkWidget* widget)
{
	if (widget_)
		g_object_unref(widget_);
	widget_ = widget;
	if (widget_)
		g_object_ref_sink(widget_);
}

GtkWidget* ScopedWidget::release()
{
	GtkWidget* result = widget_;
	widget_ = nullptr;
	return result;
}

DesktopEntryFixture::DesktopEntryFixture(const char* id, const char* name,
		const char* command)
	: id_(id ? id : ""),
	  contents_("[Desktop Entry]\nType=Application\nName=" +
		std::string(name ? name : "") + "\nExec=" +
		std::string(command ? command : "") + "\n")
{
}

}
