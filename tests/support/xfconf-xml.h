#ifndef MEOWMENU_TEST_XFCONF_XML_H
#define MEOWMENU_TEST_XFCONF_XML_H

#include <string>
#include <vector>

namespace meowmenu_test
{

struct XfconfSnapshotEntry
{
	std::string path;
	std::string type;
	std::string value;
};

/* load_xfconf_snapshot:
 * @filename: XML snapshot in the Xfconf property format.
 *
 * Loads leaf values and returns them in canonical path order so fixture
 * comparisons do not depend on XML element ordering.
 */
std::vector<XfconfSnapshotEntry> load_xfconf_snapshot(const char* filename);
std::string canonical_xfconf_snapshot(
		const std::vector<XfconfSnapshotEntry>& entries);

}

#endif
