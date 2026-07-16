/* Optional calculator dependency policy across package formats. */

#include <cassert>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

static std::string read_file(const char* path)
{
	std::ifstream input(path);
	assert(input && "package metadata file is missing");
	std::ostringstream contents;
	contents << input.rdbuf();
	return contents.str();
}

static bool matches(const std::string& text, const char* expression)
{
	return std::regex_search(text, std::regex(expression,
		std::regex::ECMAScript | std::regex::icase));
}

int main()
{
	const std::string deb = read_file(MEOWMENU_DEBIAN_CONTROL);
	assert(matches(deb, R"(Recommends:[\s\S]*\bbc\b)"));
	assert(!matches(deb, R"(Build-Depends:[\s\S]*\bbc\b[\s\S]*Standards-Version:)"));
	assert(!matches(deb, R"(Depends:[\s\S]*\bbc\b[\s\S]*Recommends:)"));

	const std::string rpm = read_file(MEOWMENU_RPM_SPEC);
	assert(matches(rpm, R"((^|\n)Recommends:[ \t]+bc([ \t]*\n|$))"));
	assert(!matches(rpm, R"((^|\n)(BuildRequires|Requires):[^\n]*\bbc\b)"));

	const std::string arch = read_file(MEOWMENU_ARCH_PKGBUILD);
	assert(!matches(arch, R"((depends|makedepends|optdepends)=\([^)]*['\"]bc([:'\"]|$))"));
	return 0;
}
