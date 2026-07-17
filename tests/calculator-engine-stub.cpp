/*
 * Controllable calculator subprocess used by evaluator lifecycle tests.
 */

#include <glib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <string>

namespace
{

std::string basename_of(const char* path)
{
	const char* slash = path ? strrchr(path, G_DIR_SEPARATOR) : nullptr;
	return slash ? slash + 1 : (path ? path : "");
}

std::string read_stdin()
{
	return std::string(std::istreambuf_iterator<char>(std::cin),
			std::istreambuf_iterator<char>());
}

}

int main(int argc, char** argv)
{
	const char* mode_env = g_getenv("MEOWMENU_CALCULATOR_STUB_MODE");
	const std::string mode = mode_env ? mode_env : "success";
	const std::string program = basename_of(argv[0]);
	const std::string input = program == "bc" ? read_stdin() : std::string();

	if (mode == "delay")
	{
		g_usleep(3000000);
	}
	else if (mode == "failure")
	{
		return 7;
	}
	else if (mode == "stderr")
	{
		std::fputs("engine diagnostic\n", stderr);
	}
	else if (mode == "overflow")
	{
		std::fputc('1', stdout);
		for (int i = 0; i < 17000; ++i)
			std::fputc('0', stdout);
		std::fputc('\n', stdout);
		return 0;
	}

	// The bc adapter contract is deliberately validated inside the child. A
	// missing expression newline reproduces real GNU bc's silent-stdout syntax
	// failure and makes the evaluator regression test fail for the right reason.
	if (program == "bc")
	{
		if (argc != 3 || std::strcmp(argv[1], "-q") != 0
				|| std::strcmp(argv[2], "-l") != 0
				|| input.find("scale=") != 0 || input.size() < 2
				|| input.back() != '\n' || input.find('\n') == input.size() - 1)
		{
			std::fputs("invalid bc adapter\n", stderr);
			return 8;
		}
	}
	else if (program == "qalc")
	{
		if (argc < 3 || std::strcmp(argv[argc - 2], "--") != 0)
			return 9;
	}
	else if (program == "gcalccmd")
	{
		if (argc != 2)
			return 10;
	}

	const char* output = g_getenv("MEOWMENU_CALCULATOR_STUB_OUTPUT");
	std::fputs(output ? output : "4\n", stdout);
	return 0;
}
