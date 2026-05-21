test_smoke_sources = ['test_smoke.cpp']

test_smoke = executable(
  'test_smoke',
  test_smoke_sources,
  dependencies: [],
)

test('smoke', test_smoke)

test_schema_migration_sources = ['test_schema_migration.cpp']

test_schema_migration = executable(
  'test_schema_migration',
  test_schema_migration_sources,
  dependencies: [],
)

test('schema_migration', test_schema_migration)

test_preset_sources = ['test_preset.cpp']

test_preset = executable(
  'test_preset',
  test_preset_sources,
  dependencies: [],
)

test('preset', test_preset)

test_preset_io_sources = ['test_preset_io.cpp']

test_preset_io = executable(
  'test_preset_io',
  test_preset_io_sources,
  dependencies: [],
)

test('preset_io', test_preset_io)

test_properties_tabs_sources = ['test_properties_tabs.cpp']

test_properties_tabs = executable(
  'test_properties_tabs',
  test_properties_tabs_sources,
  dependencies: [],
)

test('properties_tabs', test_properties_tabs)

test_unified_bar_sources = ['test_unified_bar.cpp']

test_unified_bar = executable(
  'test_unified_bar',
  test_unified_bar_sources,
  dependencies: [],
)

test('unified_bar', test_unified_bar)

test_search_action_edit_sources = ['test_search_action_edit.cpp']

test_search_action_edit = executable(
  'test_search_action_edit',
  test_search_action_edit_sources,
  dependencies: [],
)

test('search_action_edit', test_search_action_edit)

test_places_sources = ['test_places.cpp']

test_places = executable(
  'test_places',
  test_places_sources,
  dependencies: [glib, gio],
)

test('places', test_places)

test_home_search_sources = ['test_home_search.cpp']

test_home_search = executable(
  'test_home_search',
  test_home_search_sources,
  dependencies: [glib, gio],
)

test('home_search', test_home_search)

# NOTE: test_migration covers the T1..T7 contract from
# .specify/specs/010-standalone-identity/contracts/xfconf-migration.md.
# It exercises panel-plugin/migration.cpp directly (source compiled into
# the test executable rather than linked against libmeowmenu, matching the
# pattern of the other tests in this directory). The fixture brings up a
# private session bus + xfconfd; when xfconfd is absent the test prints a
# TAP SKIP marker and returns 0.
test_migration_sources = [
  'test_migration.cpp',
  '..' / 'panel-plugin' / 'migration.cpp',
]

test_migration = executable(
  'test_migration',
  test_migration_sources,
  dependencies: [glib, gio, xfconf],
)

test('migration', test_migration, timeout: 60)
