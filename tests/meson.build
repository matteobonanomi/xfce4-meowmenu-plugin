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
# under tests/fixtures/xfconf-keys.txt. Compiles migration.cpp into the test
# binary so the production meowmenu↔whiskermenu base derivation is exercised
# end-to-end (same linking pattern as test_migration). When xfconfd or
# dbus-daemon is missing the test prints a TAP SKIP and returns 0.
test_settings_key_inventory_sources = [
  'test_settings_key_inventory.cpp',
  '..' / 'panel-plugin' / 'migration.cpp',
]

test_settings_key_inventory = executable(
  'test_settings_key_inventory',
  test_settings_key_inventory_sources,
  cpp_args: [
    # NOTE: path baked in at configure time so the test binary is
    # independent of its working directory.
    '-DMEOWMENU_TEST_XFCONF_SNAPSHOT_PATH="'
      + meson.current_source_dir()
      + '/fixtures/xfconf-keys.txt"',
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
  '..' / 'panel-plugin' / 'launcher' / 'element.cpp',
]

test_home_search_lifecycle = executable(
  'test_home_search_lifecycle',
  test_home_search_lifecycle_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gio, gtk, libxfce4ui, libxfce4util],
)

test('home_search_lifecycle', test_home_search_lifecycle, timeout: 30)

# test_focus_router: headless coverage of the Tab-cycle resolution
# implemented in panel-plugin/core/window-keyboard.cpp. Pulls only the
# helper translation unit; no display server, no GTK widgets.
test_focus_router_sources = [
  'test_focus_router.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-keyboard.cpp',
]

test_focus_router = executable(
  'test_focus_router',
  test_focus_router_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk],
)

test('focus_router', test_focus_router)

# test_esc_ladder: headless coverage of the progressive Esc state
# machine in panel-plugin/core/window-keyboard.cpp.
test_esc_ladder_sources = [
  'test_esc_ladder.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-keyboard.cpp',
]

test_esc_ladder = executable(
  'test_esc_ladder',
  test_esc_ladder_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk],
)

test('esc_ladder', test_esc_ladder)

# test_key_routing: headless coverage of classify_key /
# is_printable_for_search. Fabricates GdkEventKey values directly.
test_key_routing_sources = [
  'test_key_routing.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-keyboard.cpp',
]

test_key_routing = executable(
  'test_key_routing',
  test_key_routing_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk],
)

test('key_routing', test_key_routing)

# test_theme_fallback: headless coverage of the theme-aware background
# fallback chooser in panel-plugin/core/theme-fallback.cpp. Pulls only the
# pure helper translation unit; no display server, no GTK widgets.
test_theme_fallback_sources = [
  'test_theme_fallback.cpp',
  '..' / 'panel-plugin' / 'core' / 'theme-fallback.cpp',
]

test_theme_fallback = executable(
  'test_theme_fallback',
  test_theme_fallback_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk],
)

test('theme_fallback', test_theme_fallback)

# test_window_size_clamp: headless coverage of the pure window default-size
# clamp in panel-plugin/core/window-size-clamp.cpp (HiDPI defensive guard,
# cases C1..C7). Pulls only the GTK-free helper translation unit; no display
# server, no GTK widgets.
test_window_size_clamp_sources = [
  'test_window_size_clamp.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-size-clamp.cpp',
]

test_window_size_clamp = executable(
  'test_window_size_clamp',
  test_window_size_clamp_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [],
)

test('window_size_clamp', test_window_size_clamp)
