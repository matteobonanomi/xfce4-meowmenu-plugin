#include "application-fixture.h"

namespace meowmenu_test
{

ApplicationFixture::ApplicationFixture(const char* id, const char* name,
		const char* command)
	: id_(id ? id : "")
{
	DesktopEntryFixture entry(id, name, command);
	if (tree_.valid() && tree_.write((id_ + ".desktop").c_str(),
			entry.contents().c_str()))
		desktop_path_ = tree_.path((id_ + ".desktop").c_str());
}

}
