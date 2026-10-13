#ifndef MEOWMENU_TEST_GLIB_MAIN_LOOP_H
#define MEOWMENU_TEST_GLIB_MAIN_LOOP_H

#include <glib.h>

#include <functional>
#include <string>
#include <vector>

namespace meowmenu_test
{

/* run_until:
 * @condition: predicate checked before each non-blocking context iteration.
 * @timeout_ms: monotonic upper bound in milliseconds.
 * @description: condition named in timeout output.
 *
 * Drains the selected context without allowing a blocked iteration to exceed
 * the deadline. A timeout is a test failure and returns false.
 */
bool run_until(const std::function<bool()>& condition, guint timeout_ms,
		const char* description, GMainContext* context = nullptr);

/* drain_for:
 * @duration_ms: bounded time during which asynchronous callbacks may settle.
 * @context: context to dispatch; NULL selects the default context.
 *
 * Runs the context with blocking iterations until the bounded timer fires.
 * This is used before destroying objects that own asynchronous D-Bus work.
 */
void drain_for(guint duration_ms, GMainContext* context = nullptr);

class SourceOwner
{
public:
	SourceOwner();
	~SourceOwner();

	void attach(guint source_id);
	void fired();
	void clear();
	guint id() const { return source_id_; }

	SourceOwner(const SourceOwner&) = delete;
	SourceOwner& operator=(const SourceOwner&) = delete;

private:
	guint source_id_;
};

class EnvironmentScope
{
public:
	explicit EnvironmentScope(const std::vector<std::string>& names);
	~EnvironmentScope();

	EnvironmentScope(const EnvironmentScope&) = delete;
	EnvironmentScope& operator=(const EnvironmentScope&) = delete;

private:
	struct SavedValue
	{
		std::string name;
		gchar* value;
	};
	std::vector<SavedValue> values_;
};

}

#endif
