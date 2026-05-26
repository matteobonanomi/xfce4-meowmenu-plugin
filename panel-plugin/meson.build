plugin_sources = [
  'applications-page.cpp',
  'preset.cpp',
  'preset-io.cpp',
  'launcher/category.cpp',
  'launcher/category-button.cpp',
  'command.cpp',
  'command-edit.cpp',
  'element.cpp',
  'favorites-page.cpp',
  'favourites-section.cpp',
  'history-section.cpp',
  'home-search-worker.cpp',
  'home-section.cpp',
  'icon-renderer.cpp',
  'icon-size.cpp',
  'image-menu-item.h',
  'launcher.cpp',
  'launcher-icon-view.cpp',
  'launcher-tree-view.cpp',
  'migration.cpp',
  'page.cpp',
  'places-item.cpp',
  'places-page.cpp',
  'plugin.cpp',
  'profile.cpp',
  'query.cpp',
  'recent-page.cpp',
  'register-plugin.c',
  'resizer.cpp',
  'run-action.cpp',
  'search-action.cpp',
  'config/usage-stats.cpp',
  'search-page.cpp',
  'settings.cpp',
  'settings-dialog.cpp',
  'ui/properties/common.cpp',
  'ui/properties/general.cpp',
  'ui/properties/user-session.cpp',
  'ui/properties/search-bar.cpp',
  'ui/properties/search-bar-aliases.cpp',
  'ui/properties/search-bar-actions.cpp',
  'ui/properties/results-view.cpp',
  'ui/properties/sidebar.cpp',
  'ui/properties/places.cpp',
  'slot.h',
  'window.cpp',
  xfce_revision_h,
  meowmenu_version_h,
]

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

plugin_lib = shared_module(
  'meowmenu',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  cpp_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-meowmenu-plugin'),
    '-DSETTINGS_MENUFILE="@0@"'.format(get_option('sysconfdir') / 'xdg' / 'menus' / 'xfce-settings-manager.menu'),
  ],
  include_directories: [
    include_directories('..'),
    include_directories('.'),
  ],
  dependencies: [
    accountsservice,
    exo,
    garcon,
    gio,
    glib,
    gtk,
    gtk_layer_shell,
    libxfce4panel,
    libxfce4ui,
    libxfce4util,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

popup_meowmenu_sources = [
  'core/xfce4-popup-meowmenu.cpp',
  xfce_revision_h,
]

popup_meowmenu = executable(
  'xfce4-popup-meowmenu',
  popup_meowmenu_sources,
  dependencies: [
    gio,
    glib,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'meowmenu.desktop.in',
  output: 'meowmenu.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)

install_man('core/xfce4-popup-meowmenu.1')
