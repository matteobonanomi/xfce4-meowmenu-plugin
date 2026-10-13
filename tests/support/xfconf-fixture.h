#ifndef MEOWMENU_TEST_XFCONF_FIXTURE_H
#define MEOWMENU_TEST_XFCONF_FIXTURE_H

#include "glib-main-loop.h"

#include <string>
#include <memory>

#include <gio/gio.h>

namespace meowmenu_test
{

class XfconfFixture
{
public:
	XfconfFixture();
	~XfconfFixture();

	/* start:
	 *
	 * Starts an isolated session bus and xfconfd when both are available.
	 * Returns false for a developer-only unavailable prerequisite.
	 */
	bool start();
	void stop();
	bool active() const { return active_; }
	const std::string& property_base() const { return property_base_; }

	XfconfFixture(const XfconfFixture&) = delete;
	XfconfFixture& operator=(const XfconfFixture&) = delete;

private:
	bool wait_for_service();
	void stop_child();

	GTestDBus* bus_;
	GPid xfconfd_pid_;
	bool active_;
	std::string property_base_;
	std::string scratch_;
	std::unique_ptr<EnvironmentScope> environment_;
};

}

#endif
