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

# test_settings_key_inventory: fail-closed drift gate against the snapshot
# under .specify/specs/013-refactor-foundation/artifacts/xfconf-keys.txt.
# Compiles migration.cpp into the test binary so the production
# meowmenu↔whiskermenu base derivation is exercised end-to-end (same
# linking pattern as test_migration). When xfconfd or dbus-daemon is
# missing the test prints a TAP SKIP and returns 0.
test_settings_key_inventory_sources = [
  'test_settings_key_inventory.cpp',
  '..' / 'panel-plugin' / 'migration.cpp',
]

test_settings_key_inventory = executable(
  'test_settings_key_inventory',
  test_settings_key_inventory_sources,
  cpp_args: [
    # NOTE: meson.current_source_dir() is the tests/ directory; the
    # snapshot artifact lives at the project root under .specify/. The
    # path is baked in at configure time so the test binary does not
    # need to walk argv[0] to find it. Project requires meson >= 0.54,
    # so we avoid project_source_root() (0.56+) and use the older
    # current_source_dir() + parent traversal instead.
    '-DMEOWMENU_TEST_XFCONF_SNAPSHOT_PATH="'
      + meson.current_source_dir()
      + '/../.specify/specs/013-refactor-foundation/artifacts/xfconf-keys.txt"',
  ],
  dependencies: [glib, gio, xfconf],
)

test('settings_key_inventory', test_settings_key_inventory, timeout: 60)

# test_search_ranking: characterization gate over query.cpp's match
# scoring (RF-SEARCH-001..005). Pure unit test: query.cpp depends only on
# glib, so linking the production source into the binary is direct. The
# launcher.cpp classifier OR-bits and search-page.cpp boost arithmetic are
# deliberately out of scope here — see research.md §3.11.
test_search_ranking_sources = [
  'test_search_ranking.cpp',
  '..' / 'panel-plugin' / 'search' / 'query.cpp',
]

test_search_ranking = executable(
  'test_search_ranking',
  test_search_ranking_sources,
  dependencies: [glib],
)

test('search_ranking', test_search_ranking)

# test_home_search_lifecycle: behavioural freeze for home-search-worker.cpp
# (BFS order, hidden/blocklist skipping, symlink-to-dir avoidance, cap,
# cancel, on_done dispatch). Compiles the worker plus its PlacesItem
# dependency directly into the test binary; element.cpp is needed for
# Element's set_icon and spawn symbols that places-item.cpp pulls in.
# Skips cleanly when no display is available.
test_home_search_lifecycle_sources = [
  'test_home_search_lifecycle.cpp',
  '..' / 'panel-plugin' / 'places' / 'home-search-worker.cpp',
  '..' / 'panel-plugin' / 'places' / 'places-item.cpp',
  '..' / 'panel-plugin' / 'element.cpp',
]

test_home_search_lifecycle = executable(
  'test_home_search_lifecycle',
  test_home_search_lifecycle_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gio, gtk, libxfce4ui, libxfce4util],
)

test('home_search_lifecycle', test_home_search_lifecycle, timeout: 30)
