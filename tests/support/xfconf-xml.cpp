#include "xfconf-xml.h"

#include <algorithm>
#include <cstring>

#include <glib.h>

namespace
{

struct ParserState
{
	std::vector<std::string> stack;
	std::vector<meowmenu_test::XfconfSnapshotEntry> entries;
};

const gchar* attribute(const gchar** names, const gchar** values,
		const char* wanted)
{
	for (gsize index = 0; names[index]; ++index)
		if (std::strcmp(names[index], wanted) == 0)
			return values[index];
	return nullptr;
}

void start_element(GMarkupParseContext*, const gchar* element,
		const gchar** names, const gchar** values, gpointer data, GError**)
{
	ParserState* state = static_cast<ParserState*>(data);
	if (std::strcmp(element, "property") == 0)
	{
		const gchar* name = attribute(names, values, "name");
		const gchar* type = attribute(names, values, "type");
		const gchar* value = attribute(names, values, "value");
		if (name)
		{
			state->stack.push_back(name);
			if (value)
			{
				std::string path;
				for (const std::string& component : state->stack)
					path += "/" + component;
				state->entries.push_back({path, type ? type : "", value});
			}
		}
	}
}

void end_element(GMarkupParseContext*, const gchar* element, gpointer data,
		GError**)
{
	ParserState* state = static_cast<ParserState*>(data);
	if (std::strcmp(element, "property") == 0 && !state->stack.empty())
		state->stack.pop_back();
}

}

namespace meowmenu_test
{

std::vector<XfconfSnapshotEntry> load_xfconf_snapshot(const char* filename)
{
	gchar* contents = nullptr;
	gsize length = 0;
	GError* error = nullptr;
	std::vector<XfconfSnapshotEntry> result;
	if (!g_file_get_contents(filename, &contents, &length, &error))
	{
		g_test_message("could not read Xfconf snapshot: %s",
			error ? error->message : "unknown error");
		g_clear_error(&error);
		return result;
	}
	ParserState state;
	GMarkupParser parser = {start_element, end_element, nullptr, nullptr, nullptr};
	GMarkupParseContext* context = g_markup_parse_context_new(
			&parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &state, nullptr);
	if (!g_markup_parse_context_parse(context, contents, length, &error) ||
			!g_markup_parse_context_end_parse(context, &error))
	{
		g_test_message("could not parse Xfconf snapshot: %s",
			error ? error->message : "unknown error");
		g_clear_error(&error);
		state.entries.clear();
	}
	g_markup_parse_context_free(context);
	g_free(contents);
	std::sort(state.entries.begin(), state.entries.end(),
			[](const XfconfSnapshotEntry& left, const XfconfSnapshotEntry& right) {
				return left.path < right.path;
			});
	return state.entries;
}

std::string canonical_xfconf_snapshot(
		const std::vector<XfconfSnapshotEntry>& entries)
{
	std::string result;
	for (const XfconfSnapshotEntry& entry : entries)
		result += entry.path + "|" + entry.type + "|" + entry.value + "\n";
	return result;
}

}
