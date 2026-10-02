#ifndef MEOWMENU_PRIVATE_XFCONF_FIXTURE_H
#define MEOWMENU_PRIVATE_XFCONF_FIXTURE_H

/* run_with_private_xfconf:
 * @argc: test process argument count.
 * @argv: test process arguments; argv[0] must relaunch this executable.
 * @child_main: assertions that construct the runtime graph in the child.
 *
 * Runs @child_main in a subprocess connected to a private session bus and
 * xfconfd. The parent owns and stops both services after the child exits, so
 * panel-host fixtures cannot read or write the desktop's live configuration.
 *
 * Returns: the child result, or 77 when the private service is unavailable.
 */
int run_with_private_xfconf(int argc, char** argv,
		int (*child_main)(int argc, char** argv));

/* settle_private_xfconf:
 *
 * Gives asynchronous property writes a bounded opportunity to complete before
 * a test tears down its panel-owned Xfconf channels.
 */
void settle_private_xfconf();

/* seed_private_xfconf_profile:
 * @channel_name: private Xfconf channel name.
 * @property_base: panel-plugin property base to initialize.
 * @schema_version: current schema version expected by the test.
 *
 * Seeds an already initialized profile so lifecycle tests exercise their
 * target behavior without also replaying the migration write stream.
 */
void seed_private_xfconf_profile(const char* channel_name,
		const char* property_base, int schema_version);

#endif
