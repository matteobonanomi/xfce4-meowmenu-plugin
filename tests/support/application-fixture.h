#ifndef MEOWMENU_TEST_APPLICATION_FIXTURE_H
#define MEOWMENU_TEST_APPLICATION_FIXTURE_H

#include "gtk-fixture.h"
#include "temp-tree.h"

#include <string>

namespace meowmenu_test
{

class ApplicationFixture
{
public:
	ApplicationFixture(const char* id, const char* name,
			const char* command);

	bool valid() const { return tree_.valid() && !desktop_path_.empty(); }
	const std::string& desktop_path() const { return desktop_path_; }
	const std::string& desktop_id() const { return id_; }

private:
	TemporaryTree tree_;
	std::string id_;
	std::string desktop_path_;
};

}

#endif
