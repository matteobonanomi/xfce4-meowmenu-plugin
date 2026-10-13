test_support = static_library(
  'meowmenu-test-support',
  files(
    'support/assertions.cpp',
    'support/glib-main-loop.cpp',
    'support/temp-tree.cpp',
    'support/gtk-fixture.cpp',
    'support/application-fixture.cpp',
    'support/xfconf-fixture.cpp',
    'support/xfconf-xml.cpp',
  ),
  include_directories: include_directories('support', '..' / 'panel-plugin'),
  dependencies: [gio, glib, gtk, xfconf],
)

test_support_dep = declare_dependency(
  link_with: test_support,
  include_directories: include_directories('support', '..' / 'panel-plugin'),
)

test_menu_mode_state = executable(
  'test_menu_mode_state',
  [
    'test_menu_mode_state.cpp',
    '..' / 'panel-plugin' / 'core' / 'menu-mode-state.cpp',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep],
)

test('menu_mode_state', test_menu_mode_state)

# Independent pure decisions share one executable. Each registration selects a
# named case, so failures still identify the owning contract without copying
# the same GTK-free setup into a dozen binaries.
test_contract_matrix_sources = [
  'test_contract_matrix.cpp',
  '..' / 'panel-plugin' / 'core' / 'category-lifetime.cpp',
  '..' / 'panel-plugin' / 'core' / 'interactive-resize.cpp',
  '..' / 'panel-plugin' / 'core' / 'layout-mode.cpp',
  '..' / 'panel-plugin' / 'core' / 'menu-composition.cpp',
  '..' / 'panel-plugin' / 'core' / 'opacity-model.cpp',
  '..' / 'panel-plugin' / 'core' / 'sidebar-layout.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-frame.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-keyboard.cpp',
  '..' / 'panel-plugin' / 'core' / 'window-size-clamp.cpp',
  '..' / 'panel-plugin' / 'config' / 'usage-stats.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'application-load-generation.cpp',
  '..' / 'panel-plugin' / 'settings-reload-intent.cpp',
  '..' / 'panel-plugin' / 'ui' / 'grid-cell-metrics.cpp',
  '..' / 'panel-plugin' / 'ui' / 'switch-icons.cpp',
  '..' / 'panel-plugin' / 'core' / 'user-session-relayout.cpp',
]

test_contract_matrix = executable(
  'test_contract_matrix',
  test_contract_matrix_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, glib, gtk, libxfce4util, xfconf],
)

foreach contract_case : [
  ['focus_router', 'keyboard'],
  ['window_size_clamp', 'geometry'], ['layout_sensitivity', 'layout'],
  ['window_frame', 'frame'],
  ['menu_composition', 'composition'], ['grid_cell_metrics', 'grid'],
  ['opacity_model', 'opacity'], ['switch_icons', 'icons'],
  ['user_session_relayout', 'category'], ['reload_intent', 'reload'],
  ['application_load_generation', 'generation'],
]
  test(
    contract_case[0],
    test_contract_matrix,
    args: ['--case=' + contract_case[1]],
  )
endforeach

# Package checks run without a display. Keep the pure integration checks
# available there while ensuring display-backed cases skip before creating GTK
# widgets.
test_grid_presentation = executable(
  'test_grid_presentation',
  [
    'test_grid_presentation.cpp',
    '..' / 'panel-plugin' / 'ui' / 'grid-presentation.cpp',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
)

test('grid_presentation', test_grid_presentation)

test_launcher_icon_view = executable(
  'test_launcher_icon_view',
  [
    'test_launcher_icon_view.cpp',
    '..' / 'panel-plugin' / 'ui' / 'launcher-icon-view.cpp',
    '..' / 'panel-plugin' / 'ui' / 'grid-cell-metrics.cpp',
    '..' / 'panel-plugin' / 'ui' / 'icon-renderer.cpp',
    '..' / 'panel-plugin' / 'core' / 'window-keyboard.cpp',
	'..' / 'panel-plugin' / 'core' / 'window-frame.cpp',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
  # Retained: launcher-icon-view.cpp also defines the complete view class.
  # Without pruning it fails to link at IconSize::get_size(), outside this
  # presentation-helper test's source set.
  cpp_args: ['-ffunction-sections', '-fdata-sections'],
  link_args: ['-Wl,--gc-sections'],
  dependencies: [test_support_dep, exo, garcon, glib, gio, gtk, libxfce4ui, libxfce4util, xfconf],
)

test('launcher_icon_view', test_launcher_icon_view, timeout: 30)

test_launcher_tree_view = executable(
  'test_launcher_tree_view',
  [
    'test_launcher_tree_view.cpp',
    '..' / 'panel-plugin' / 'ui' / 'launcher-tree-view.cpp',
    '..' / 'panel-plugin' / 'ui' / 'icon-renderer.cpp',
    '..' / 'panel-plugin' / 'core' / 'window-keyboard.cpp',
  ],
  # Retained: the complete view class reaches launcher_model_get_iter(),
  # Category RTTI, IconSize, and grid-cell metrics outside this focused test.
  cpp_args: ['-ffunction-sections', '-fdata-sections'],
  link_args: ['-Wl,--gc-sections'],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [exo, garcon, glib, gio, gtk, libxfce4ui, libxfce4util, xfconf],
)

test('launcher_tree_view', test_launcher_tree_view, timeout: 30)

test(
  'release_version',
  python3,
  args: [meson.current_source_dir() / 'test_release_version.py'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
)

test(
  'release_contract',
  python3,
  args: [meson.current_source_dir() / 'test_release_contract.py'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
)

test(
  'translation_status',
  python3,
  args: [meson.current_source_dir() / 'test_translation_status.py'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
  timeout: 60,
)

test(
  'repository_presentation',
  python3,
  args: [meson.current_source_dir() / 'test_repository_presentation.py'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
)

test(
  'documentation_links',
  python3,
  args: [meson.current_source_dir() / 'test_documentation_links.py'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
)

test(
  'dependency_contract',
  python3,
  args: [meson.current_source_dir() / 'test_dependency_contract.py'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
)

test(
  'dev_install_contract',
  python3,
  args: [meson.current_source_dir() / 'test_dev_install_contract.py'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
)

test(
  'suite_metrics_self_check',
  python3,
  args: [meson.current_source_dir() / 'test_suite_metrics.py',
         '--self-check', '--root', meson.current_source_dir() / '..'],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
)

# Compile the helper selector in both supported modes, independent of which
# optional helper library is present on the build host.
foreach helper_variant : [
  ['legacy', '-DMEOWMENU_XFCE_SUCCESSOR=0'],
  ['successor', '-DMEOWMENU_XFCE_SUCCESSOR=1'],
]
  test_xfce_helpers_variant = executable(
    'test_xfce_helpers_' + helper_variant[0],
    [
      'test_xfce_helpers.cpp',
      '..' / 'panel-plugin' / 'config' / 'xfce-helpers.cpp',
    ],
    cpp_args: [helper_variant[1]],
    include_directories: include_directories('..' / 'panel-plugin'),
    dependencies: [glib],
  )
  test('xfce_helpers_' + helper_variant[0], test_xfce_helpers_variant)
endforeach

# GLib refuses to run g_test assertions when Meson's release build enables
# G_DISABLE_ASSERT. Keep Calculator tests assert-enabled in every build type.
calculator_test_cpp_args = ['-UG_DISABLE_ASSERT']

test_calculator_engine = executable(
  'test_calculator_engine',
  ['test_calculator_engine.cpp', '..' / 'panel-plugin' / 'calculator' / 'calculator-engine.cpp'],
  cpp_args: calculator_test_cpp_args,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib],
)
test('calculator_engine', test_calculator_engine)

test_calculator_output = executable(
  'test_calculator_output',
  ['test_calculator_output.cpp', '..' / 'panel-plugin' / 'calculator' / 'calculator-output.cpp'],
  cpp_args: calculator_test_cpp_args,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib],
)
test('calculator_output', test_calculator_output)

calculator_engine_stub = executable(
  'calculator-engine-stub',
  'calculator-engine-stub.cpp',
  dependencies: [glib],
)

test_calculator_evaluator = executable(
  'test_calculator_evaluator',
  [
    'test_calculator_evaluator.cpp',
    '..' / 'panel-plugin' / 'calculator' / 'calculator-engine.cpp',
    '..' / 'panel-plugin' / 'calculator' / 'calculator-evaluator.cpp',
    '..' / 'panel-plugin' / 'calculator' / 'calculator-output.cpp',
  ],
  cpp_args: calculator_test_cpp_args + [
    '-DMEOWMENU_CALCULATOR_STUB_PATH="' + calculator_engine_stub.full_path() + '"',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gio],
)
test('calculator_evaluator', test_calculator_evaluator,
  depends: calculator_engine_stub,
  timeout: 15,
)

test_calculator_result = executable(
  'test_calculator_result',
  [
    'test_calculator_result.cpp',
    '..' / 'panel-plugin' / 'search' / 'calculator-result.cpp',
  ],
  cpp_args: calculator_test_cpp_args,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk, dependency('pangoft2')],
)
test('calculator_result', test_calculator_result)

test_calculator_search_results = executable(
  'test_calculator_search_results',
  [
    'test_calculator_search_results.cpp',
    '..' / 'panel-plugin' / 'search' / 'calculator-result.cpp',
  ],
  cpp_args: calculator_test_cpp_args,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk],
)
test('calculator_search_results', test_calculator_search_results)

test_schema_migration_sources = ['test_schema_migration.cpp']

test_schema_migration = executable(
  'test_schema_migration',
  test_schema_migration_sources,
  cpp_args: [
    '-DMEOWMENU_080_XFCONF_FIXTURE="' + meson.current_source_dir() / 'fixtures' / 'xfconf-0.8.0.xml' + '"',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, xfconf],
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
  dependencies: [glib, xfconf, libxfce4util],
)

test('preset', test_preset)

test_preset_io_sources = [
  'test_preset_io.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-io.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-builtins.cpp',
]

test_preset_io = executable(
  'test_preset_io',
  test_preset_io_sources,
  cpp_args: ['-ffunction-sections', '-fdata-sections'],
  link_args: ['-Wl,--gc-sections'],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, xfconf, libxfce4util],
)

test('preset_io', test_preset_io)

# test_preset_xfconf: real integration coverage of the Settings-free preset data
# layer against a live xfconfd (save-refresh read side + schema-lenient seeded
# import). It links the real preset.cpp / preset-io.cpp / preset-builtins.cpp;
# the Settings-coupled functions in those units (save/rename/delete/import/export,
# apply_preset) reach the full Plugin graph, so they are pruned at link time with
# --gc-sections — only enumerate_user_presets(), find_preset_by_id() and
# enumerate_preset_files() and their Settings-free dependencies survive. The test
# otherwise fails at Boolean::set(), Integer::set(), String::set(), and
# IconSize::set() from the uncalled Settings mutation paths.
# skips cleanly (TAP SKIP, exit 0) when xfconfd / dbus-daemon are unavailable.
test_preset_xfconf_sources = [
  'test_preset_xfconf.cpp',
  'private-xfconf-fixture.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-io.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-builtins.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'favorite-projection.cpp',
  '..' / 'panel-plugin' / 'settings-defaults.cpp',
]

test_preset_xfconf = executable(
  'test_preset_xfconf',
  test_preset_xfconf_sources,
  cpp_args: xfce_helper_cpp_args + [
    '-ffunction-sections',
    '-fdata-sections',
    '-DMEOWMENU_RC_UPGRADE_FIXTURE="' + meson.current_source_dir() / 'fixtures' / 'xfconf-1.0.0-rc1.xml' + '"',
    '-DMEOWMENU_AFFECTED_UPGRADE_FIXTURE="' + meson.current_source_dir() / 'fixtures' / 'xfconf-affected-composition-reset.xml' + '"',
  ],
  link_args: ['-Wl,--gc-sections'],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, glib, gio, xfconf, libxfce4ui],
)

test('preset_xfconf', test_preset_xfconf, timeout: 60)

# test_properties_tabs: pure placement-grid invariants plus the independent
# governed-key/dialog-control coverage gate.
test_properties_tabs_sources = [
  'test_properties_tabs.cpp',
  '..' / 'panel-plugin' / 'presets' / 'preset-builtins.cpp',
  '..' / 'panel-plugin' / 'ui' / 'properties' / 'preset-sync.cpp',
]

test_properties_tabs = executable(
  'test_properties_tabs',
  test_properties_tabs_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, xfconf],
)

test('properties_tabs', test_properties_tabs)

test_search_action_edit_sources = ['test_search_action_edit.cpp']

test_search_action_edit = executable(
  'test_search_action_edit',
  test_search_action_edit_sources + [
    '..' / 'panel-plugin' / 'config' / 'xfce-helpers.cpp',
    '..' / 'panel-plugin' / 'launcher' / 'command-interpreter.cpp',
    '..' / 'panel-plugin' / 'launcher' / 'element.cpp',
    '..' / 'panel-plugin' / 'search' / 'search-action.cpp',
    '..' / 'panel-plugin' / 'settings-bindings.cpp',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [glib, gtk, libxfce4ui, libxfce4util, xfconf],
)

test('search_action_edit', test_search_action_edit)

test_places_sources = [
  'test_places.cpp',
  '..' / 'panel-plugin' / 'config' / 'xfce-helpers.cpp',
  '..' / 'panel-plugin' / 'places' / 'places-item.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'command-interpreter.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'element.cpp',
]

test_places = executable(
  'test_places',
  test_places_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  cpp_args: xfce_helper_cpp_args,
  dependencies: [glib, gio, gtk, libxfce4ui, libxfce4util],
)

test('places', test_places)

test_drag_to_favourites_sources = [
  'test_drag_to_favourites.cpp',
  '..' / 'panel-plugin' / 'core' / 'drag-to-favourites.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'favorite-projection.cpp',
  '..' / 'panel-plugin' / 'ui' / 'icon-size.cpp',
]

test_drag_to_favourites = executable(
  'test_drag_to_favourites',
  test_drag_to_favourites_sources,
  include_directories: [
    include_directories('..'),
    include_directories('..' / 'panel-plugin'),
  ],
  dependencies: [glib, gio, libxfce4util, xfconf],
)

test('drag_to_favourites', test_drag_to_favourites)

test_desktop_drag_sources = [
  'test_desktop_drag.cpp',
  '..' / 'panel-plugin' / 'core' / 'desktop-drag.cpp',
  '..' / 'panel-plugin' / 'core' / 'layout-mode.cpp',
  '..' / 'panel-plugin' / 'ui' / 'icon-size.cpp',
]

test_desktop_drag = executable(
  'test_desktop_drag',
  test_desktop_drag_sources,
  include_directories: [
    include_directories('..'),
    include_directories('..' / 'panel-plugin'),
  ],
  dependencies: [glib, libxfce4util, xfconf],
)

test('desktop_drag', test_desktop_drag)

# test_places_open: GTK-free coverage of the Places open-path hardening —
# the g_shell_quote round-trip handed to the external helper (supported behavior) and
# the open-failure reason string built for the error dialog (supported behavior). Compiles
# places-item.cpp plus its Element base (element.cpp) directly into the binary,
# mirroring test_home_search_lifecycle; no display is needed for these pure
# helpers, so the binary runs headless.
test_places_open_sources = [
  'test_places_open.cpp',
  '..' / 'panel-plugin' / 'config' / 'xfce-helpers.cpp',
  '..' / 'panel-plugin' / 'places' / 'places-item.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'command-interpreter.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'element.cpp',
]

test_places_open = executable(
  'test_places_open',
  test_places_open_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  cpp_args: xfce_helper_cpp_args,
  dependencies: [glib, gio, gtk, libxfce4ui, libxfce4util],
)

test('places_open', test_places_open)

# test_search_ranking: characterization gate over Query match scoring. This is
# a pure unit test because query.cpp depends only on GLib, so linking the
# production source into the binary is direct. Launcher classifier bits and
# SearchPage boost arithmetic remain covered by their owning tests.
test_search_ranking_sources = [
  'test_search_ranking.cpp',
  '..' / 'panel-plugin' / 'search' / 'query.cpp',
]

test_search_ranking = executable(
  'test_search_ranking',
  test_search_ranking_sources,
  cpp_args: ['-DMEOWMENU_QUERY_NORMALIZATION_TEST'],
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
  '..' / 'panel-plugin' / 'config' / 'xfce-helpers.cpp',
  '..' / 'panel-plugin' / 'places' / 'home-search-worker.cpp',
  '..' / 'panel-plugin' / 'places' / 'places-item.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'command-interpreter.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'element.cpp',
]

test_home_search_lifecycle = executable(
  'test_home_search_lifecycle',
  test_home_search_lifecycle_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  cpp_args: xfce_helper_cpp_args,
  dependencies: [test_support_dep, glib, gio, gtk, libxfce4ui, libxfce4util],
)

test('home_search_lifecycle', test_home_search_lifecycle, timeout: 30)

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

# Persisted layout values share the production parsers used by rendering and
# sizing callers. This target covers every supported value and safe fallback.
test_layout_domains = executable(
  'test_layout_domains',
  [
    'test_layout_domains.cpp',
    '..' / 'panel-plugin' / 'core' / 'layout-mode.cpp',
    '..' / 'panel-plugin' / 'core' / 'sidebar-layout.cpp',
  ],
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [],
)

test('layout_domains', test_layout_domains)

test_settings_defaults = executable(
  'test_settings_defaults',
  ['test_settings_defaults.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('settings_defaults', test_settings_defaults, timeout: 30)

test_settings_dispatch = executable(
  'test_settings_dispatch',
  ['test_settings_dispatch.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('settings_dispatch', test_settings_dispatch, timeout: 30)

test_category_activation = executable(
  'test_category_activation',
  ['test_category_activation.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('category_activation', test_category_activation, timeout: 30)

# Production command parsing and PATH availability share the same owned argv
# boundary used by session and Element launch paths. Dead-section pruning keeps
# unrelated Settings and Run Action dialog dependencies outside this target.
test_command_interpretation_sources = [
  'test_command_interpretation.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'command-interpreter.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'element.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'launcher-safety.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'command.cpp',
  '..' / 'panel-plugin' / 'config' / 'xfce-helpers.cpp',
  '..' / 'panel-plugin' / 'search' / 'run-action.cpp',
]

foreach helper_variant : [
  ['legacy', '-DMEOWMENU_XFCE_SUCCESSOR=0'],
  ['successor', '-DMEOWMENU_XFCE_SUCCESSOR=1'],
]
  test_command_interpretation_variant = executable(
    'test_command_interpretation_' + helper_variant[0],
    test_command_interpretation_sources,
    include_directories: include_directories('..' / 'panel-plugin'),
    cpp_args: [
      '-ffunction-sections',
      '-fdata-sections',
      helper_variant[1],
    ],
    link_args: ['-Wl,--gc-sections'],
    dependencies: [glib, gtk, libxfce4ui, libxfce4util, xfconf],
  )
  test(
    'command_interpretation_' + helper_variant[0],
    test_command_interpretation_variant,
  )
endforeach

test_favorites_page = executable(
  'test_favorites_page',
  ['test_favorites_page.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('favorites_page', test_favorites_page, timeout: 30)

test_result_surfaces = executable(
  'test_result_surfaces',
  ['test_result_surfaces.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('result_surfaces', test_result_surfaces, timeout: 30)

test_result_drag_contract = executable(
  'test_result_drag_contract',
  ['test_result_drag_contract.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('result_drag_contract', test_result_drag_contract, timeout: 30)

test_window_transactions = executable(
  'test_window_transactions',
  ['test_window_transactions.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('window_transactions', test_window_transactions, timeout: 30)

test_plugin_lifecycle = executable(
  'test_plugin_lifecycle',
  ['test_plugin_lifecycle.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('plugin_lifecycle', test_plugin_lifecycle, timeout: 30)

test_settings_dialog_lifecycle = executable(
  'test_settings_dialog_lifecycle',
  ['test_settings_dialog_lifecycle.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('settings_dialog_lifecycle', test_settings_dialog_lifecycle, timeout: 30)

test_settings_lifecycle = executable(
  'test_settings_lifecycle',
  ['test_settings_lifecycle.cpp', 'private-xfconf-fixture.cpp'],
  link_with: meowmenu_test_runtime,
  include_directories: include_directories('..' / 'panel-plugin'),
  dependencies: [test_support_dep, garcon, glib, gtk, libxfce4panel, libxfce4ui, libxfce4util, xfconf],
)

test('settings_lifecycle', test_settings_lifecycle, timeout: 30)

# test_launcher_safeguard_lifecycle: headless freeze for the supported
# scroll-reveal safeguard lifetime. The fix lives entirely in the header-only
# shared base LauncherView, so the binary needs only that header (plus slot.h)
# and gtk — no production .cpp. It constructs a minimal concrete view, binds the
# scroll-reveal safeguard to an externally-owned adjustment, destroys the view,
# and asserts the handler is gone so a later value-changed never fires on a freed
# view (the supported behavior crash; also caught by the sanitizer CI). Constructing the
# widgets needs a display, so it skips cleanly when none is present and CI runs
# it under a virtual display so the sanitizers still exercise the freed-view path.
test_launcher_safeguard_lifecycle_sources = [
  'test_launcher_safeguard_lifecycle.cpp',
  '..' / 'panel-plugin' / 'config' / 'xfce-helpers.cpp',
  '..' / 'panel-plugin' / 'launcher' / 'launcher-safety.cpp',
]

test_launcher_safeguard_lifecycle = executable(
  'test_launcher_safeguard_lifecycle',
  test_launcher_safeguard_lifecycle_sources,
  include_directories: include_directories('..' / 'panel-plugin'),
  cpp_args: xfce_helper_cpp_args,
  dependencies: [glib, gtk],
)

test('launcher_safeguard_lifecycle', test_launcher_safeguard_lifecycle, timeout: 30)
