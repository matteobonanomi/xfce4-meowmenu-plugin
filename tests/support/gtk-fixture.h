#ifndef MEOWMENU_TEST_GTK_FIXTURE_H
#define MEOWMENU_TEST_GTK_FIXTURE_H

#include <gtk/gtk.h>

#include <string>

namespace meowmenu_test
{

enum class DisplayRequirement
{
	DeveloperOptional,
	ContextRequired,
};

/* ensure_display:
 * @requirement: whether an absent display is a local skip or a test failure.
 * @description: fixture name included in the skip/failure message.
 *
 * Initializes GTK once and makes the display policy explicit at the call
 * site. Pure tests should not call this helper.
 */
bool ensure_display(DisplayRequirement requirement, const char* description);

class ScopedWidget
{
public:
	ScopedWidget();
	explicit ScopedWidget(GtkWidget* widget);
	~ScopedWidget();

	void reset(GtkWidget* widget);
	GtkWidget* get() const { return widget_; }
	GtkWidget* release();

	ScopedWidget(const ScopedWidget&) = delete;
	ScopedWidget& operator=(const ScopedWidget&) = delete;

private:
	GtkWidget* widget_;
};

class DesktopEntryFixture
{
public:
	DesktopEntryFixture(const char* id, const char* name,
			const char* command);

	const std::string& id() const { return id_; }
	const std::string& contents() const { return contents_; }

private:
	std::string id_;
	std::string contents_;
};

}

#endif
