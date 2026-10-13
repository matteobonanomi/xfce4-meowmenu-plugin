#include "assertions.h"

#include <cstdio>

namespace
{

meowmenu_test::ExpectedMessageScope* active_scope = nullptr;
bool strict_diagnostics = false;

GLogWriterOutput write_diagnostic(GLogLevelFlags level,
		const GLogField* fields, gsize field_count, gpointer)
{
	const gchar* message = "";
	for (gsize index = 0; index < field_count; ++index)
	{
		if (g_strcmp0(fields[index].key, "MESSAGE") == 0)
		{
			message = static_cast<const gchar*>(fields[index].value);
			break;
		}
	}
	const GLogLevelFlags relevant = static_cast<GLogLevelFlags>(
			level & (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL));
	if (relevant == 0)
		return g_log_writer_default(level, fields, field_count, nullptr);
	if (active_scope && active_scope->accept(relevant, message))
		return G_LOG_WRITER_HANDLED;
	if (strict_diagnostics)
	{
		std::fprintf(stderr, "unexpected native diagnostic: %s\n", message);
		g_test_fail();
		return G_LOG_WRITER_HANDLED;
	}
	return g_log_writer_default(level, fields, field_count, nullptr);
}

}

namespace meowmenu_test
{

void add_case(const char* path, void (*function)())
{
	g_test_add_func(path, function);
}

bool check(bool condition, const char* expression,
		const char* file, int line)
{
	if (!condition)
	{
		g_test_message("%s:%d: expected %s", file, line, expression);
		g_test_fail();
	}
	return condition;
}

bool check_string(const char* actual, const char* expected,
		const char* file, int line)
{
	const bool equal = g_strcmp0(actual, expected) == 0;
	if (!equal)
	{
		g_test_message("%s:%d: expected '%s', got '%s'", file, line,
				expected ? expected : "(null)", actual ? actual : "(null)");
		g_test_fail();
	}
	return equal;
}

void install_fatal_diagnostics()
{
	strict_diagnostics = true;
	g_log_set_writer_func(write_diagnostic, nullptr, nullptr);
}

ExpectedMessageScope::ExpectedMessageScope(GLogLevelFlags level,
		const char* message_pattern)
	: level_(level), pattern_(message_pattern ? message_pattern : ""), matches_(0)
{
	if (active_scope)
		g_test_message("nested expected diagnostic scopes are not supported");
	active_scope = this;
}

ExpectedMessageScope::~ExpectedMessageScope()
{
	if (matches_ != 1)
	{
		g_test_message("expected diagnostic '%s' occurred %u times",
				pattern_.c_str(), matches_);
		g_test_fail();
	}
	if (active_scope == this)
		active_scope = nullptr;
}

bool ExpectedMessageScope::accept(GLogLevelFlags level, const gchar* message)
{
	if ((level & level_) == 0 || !g_pattern_match_simple(pattern_.c_str(), message))
		return false;
	++matches_;
	return true;
}

}
