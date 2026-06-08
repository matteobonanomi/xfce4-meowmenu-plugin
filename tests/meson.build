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

# test_preset: pure shadow logic for apply/diff/CRUD PLUS a completeness/agreement
# gate over the REAL built-in preset table. preset-builtins.cpp is a Settings-free
# translation unit, so the binary links it directly to read BUILTIN_PRESETS and
# governed_keys() without pulling in the full Settings/GTK stack. The shipped
# .meowpreset seeds are parsed from the path baked in at configure time.
test_preset_sources = [
  'test_preset.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-builtins.cpp',
]

test_preset = executable(
  'test_preset',
  test_preset_sources,
  cpp_args: [
    '-DMEOWMENU_TEST_PRESET_DIR="' + meson.current_source_dir() / '..' / 'data' / 'presets' + '"',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, xfconf],
)

test('preset', test_preset)

test_preset_io_sources = ['test_preset_io.cpp']

test_preset_io = executable(
  'test_preset_io',
  test_preset_io_sources,
  dependencies: [],
)

test('preset_io', test_preset_io)

# test_preset_xfconf: real integration coverage of the Settings-free preset data
# layer against a live xfconfd (save-refresh read side + schema-lenient seeded
# import). It links the real preset.cpp / preset-io.cpp / preset-builtins.cpp;
# the Settings-coupled functions in those units (save/rename/delete/import/export,
# apply_preset) reach the full Plugin graph, so they are pruned at link time with
# --gc-sections — only enumerate_user_presets(), find_preset_by_id() and
# enumerate_preset_files() and their Settings-free dependencies survive. The test
# skips cleanly (TAP SKIP, exit 0) when xfconfd / dbus-daemon are unavailable.
test_preset_xfconf_sources = [
  'test_preset_xfconf.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-io.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-builtins.cpp',
]

test_preset_xfconf = executable(
  'test_preset_xfconf',
  test_preset_xfconf_sources,
  cpp_args: ['-ffunction-sections', '-fdata-sections'],
  link_args: ['-Wl,--gc-sections'],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gio, xfconf, libxfce4ui],
)

test('preset_xfconf', test_preset_xfconf, timeout: 60)

# test_properties_tabs: pure placement-grid invariants PLUS the synced-keys
# coverage gate (T013). preset-builtins.cpp is Settings-free, so it links
# directly to expose governed_keys()/synced_keys() without a GTK display.
test_properties_tabs_sources = [
  'test_properties_tabs.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-builtins.cpp',
]

test_properties_tabs = executable(
  'test_properties_tabs',
  test_properties_tabs_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, xfconf],
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

# test_places_open: GTK-free coverage of the Places open-path hardening —
# the g_shell_quote round-trip handed to the external helper (FR-007/008) and
# the open-failure reason string built for the error dialog (FR-006). Compiles
# places-item.cpp plus its Element base (element.cpp) directly into the binary,
# mirroring test_home_search_lifecycle; no display is needed for these pure
# helpers, so the binary runs headless.
test_places_open_sources = [
  'test_places_open.cpp',
  '..' / 'panel-plugin' / 'places' / 'places-item.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'element.cpp',
]

test_places_open = executable(
  'test_places_open',
  test_places_open_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gio, gtk, libxfce4ui, libxfce4util],
)

test('places_open', test_places_open)

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

# test_window_frame: headless coverage of the pure window-frame helpers in
# panel-plugin/core/window-frame.cpp (radius clamp [0,24] + the composited
# rounded-border predicate). Pulls only the GTK-free helper translation unit;
# no display server, no GTK widgets.
test_window_frame_sources = [
  'test_window_frame.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-frame.cpp',
]

test_window_frame = executable(
  'test_window_frame',
  test_window_frame_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [],
)

test('window_frame', test_window_frame)

# test_sidebar_layout: headless coverage of the pure layout-decision mapping in
# panel-plugin/core/sidebar-layout.cpp (sidebar visibility, switch placement /
# orientation, forced icon/name states). GTK-free helper; no display.
test_sidebar_layout_sources = [
  'test_sidebar_layout.cpp',
  '..' / 'panel-plugin' / 'core' / 'sidebar-layout.cpp',
]

test_sidebar_layout = executable(
  'test_sidebar_layout',
  test_sidebar_layout_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [],
)

test('sidebar_layout', test_sidebar_layout)

# test_opacity_model: headless coverage of the pure percentage->alpha and
# per-region alpha-assignment helper in panel-plugin/core/opacity-model.cpp
# (the 0=transparent / 100=solid contract, no dead zone, no inter-region
# compounding). GTK-free helper; no display.
test_opacity_model_sources = [
  'test_opacity_model.cpp',
  '..' / 'panel-plugin' / 'core' / 'opacity-model.cpp',
]

test_opacity_model = executable(
  'test_opacity_model',
  test_opacity_model_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [],
)

test('opacity_model', test_opacity_model)

# test_switch_icons: coverage of the Apps/Places icon-chain resolver in
# panel-plugin/ui/switch-icons.cpp. Builds a synthetic on-disk icon theme so
# the present/absent cases are deterministic; GtkIconTheme lookups need no
# display.
test_switch_icons_sources = [
  'test_switch_icons.cpp',
  '..' / 'panel-plugin' / 'ui' / 'switch-icons.cpp',
]

test_switch_icons = executable(
  'test_switch_icons',
  test_switch_icons_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk],
)

test('switch_icons', test_switch_icons, timeout: 30)

# test_user_session_row: headless coverage of the pure user/session-row
# visibility decision mirrored from panel-plugin/core/window.cpp (both-hidden
# collapse in docked layouts; shared row preserved in unified-bar/full-screen)
# PLUS the normalize_user_session() coupling/normalisation contract — the docked
# Profile↔Commands edge coupling, the full-screen search-bar↔both coupling, the
# auto-snap direction, the per-option greying masks, and idempotency. The pure
# helper translation unit is linked directly; no display or Settings is needed.
test_user_session_row_sources = [
  'test_user_session_row.cpp',
  '..' / 'panel-plugin' / 'core' / 'user-session-layout.cpp',
]

test_user_session_row = executable(
  'test_user_session_row',
  test_user_session_row_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [],
)

test('user_session_row', test_user_session_row)
