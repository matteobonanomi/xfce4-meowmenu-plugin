#ifndef MEOWMENU_TEST_TEMP_TREE_H
#define MEOWMENU_TEST_TEMP_TREE_H

#include <string>

namespace meowmenu_test
{

class TemporaryTree
{
public:
	TemporaryTree();
	~TemporaryTree();

	bool valid() const { return !root_.empty(); }
	const std::string& root() const { return root_; }
	std::string path(const char* name) const;
	bool mkdir(const char* name) const;
	bool write(const char* name, const char* content) const;

	TemporaryTree(const TemporaryTree&) = delete;
	TemporaryTree& operator=(const TemporaryTree&) = delete;

private:
	std::string root_;
};

}

#endif
