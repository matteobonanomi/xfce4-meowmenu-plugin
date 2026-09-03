# Version is derived from the top entry of NEWS via build-aux/news-version.py.
# Do not hard-code the version here. To bump the version, prepend a new entry
# to NEWS in the form 'X.Y.Z (YYYY-MM-DD)' and re-run `meson setup --reconfigure`.
# NOTE: run_command() is inlined into project() because meson requires project()
# to be the first statement in the top-level meson.build.
project(
  'xfce4-meowmenu-plugin',
  'c',
  'cpp',
version : run_command(
    find_program('python3'),
    meson.current_source_dir() / 'build-aux' / 'news-version.py',
    '--version',
    '--news', meson.current_source_dir() / 'NEWS',
    check: true,
  ).stdout().strip(),
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.54.0',
  default_options : ['c_std=gnu11', 'cpp_std=gnu++11', 'buildtype=debugoptimized', 'warning_level=2']
)

python3 = find_program('python3')
news_version_py = files('build-aux/news-version.py')

meowmenu_release_date = run_command(
  python3, news_version_py, '--date',
  '--news', meson.current_source_dir() / 'NEWS',
  check: true,
).stdout().strip()

# Generate panel-plugin/version.h from version.h.in. Consumed by plugin.cpp
# for the About dialog and --version output (the documented behavior).
meowmenu_version_h = configure_file(
  input: 'panel-plugin' / 'version.h.in',
  output: 'version.h',
  configuration: {
    'VERSION': meson.project_version(),
    'RELEASE_DATE': meowmenu_release_date,
  },
)

pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
copyright_year = '2026'

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.50.0',
  'gtk': '>= 3.22.0',
  'xfce4': '>= 4.16.0',
  'accountsservice': '>= 0.6.45',
  'gtk-layer-shell': '>= 0.7',
}

gio = dependency('gio-2.0', version: dependency_versions['glib'])
glib = dependency('glib-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
garcon = dependency('garcon-1', version: dependency_versions['xfce4'])
libxfce4panel = dependency('libxfce4panel-2.0', version: dependency_versions['xfce4'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['xfce4'])
if libxfce4ui.version().version_compare('< 4.21.0')
  exo = dependency('exo-2', version: dependency_versions['xfce4'])
else
  exo = dependency('', required: false)
endif
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['xfce4'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfce4'])
accountsservice = dependency('accountsservice', version: dependency_versions['accountsservice'], required: get_option('accountsservice'))
gtk_layer_shell = dependency('gtk-layer-shell-0', version: dependency_versions['gtk-layer-shell'], required: get_option('gtk-layer-shell'))

feature_cflags = []

if accountsservice.found()
  feature_cflags += '-DHAVE_ACCOUNTS_SERVICE=1'
endif
if gtk_layer_shell.found()
  feature_cflags += '-DHAVE_GTK_LAYER_SHELL=1'
endif

extra_cflags = []
extra_cxxflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cxxflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 's']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_DOMAIN="@0@"'.format(meson.project_name()),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

extra_cflags_check = extra_cxxflags_check + [
  '-Wold-style-definition',
  '-Wnested-externs',
  '-Wno-declaration-after-statement',
  '-Werror=implicit-function-declaration'
]

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DCOPYRIGHT_YEAR="@0@"'.format(copyright_year),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues"',
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(cxx.get_supported_arguments(extra_cxxflags_check), language: 'cpp')
add_project_arguments(extra_cflags, language: ['c', 'cpp'])
add_project_arguments(feature_cflags, language: ['c', 'cpp'])

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

subdir('panel-plugin')
subdir('icons')
subdir('po')

# Built-in preset files. Per contracts/preset-file.md §Locations, these are
# installed read-only under ${datadir}/meowmenu/presets/ and enumerated at
# runtime by panel-plugin/preset-io.cpp.
meowmenu_preset_files = run_command(
  'sh', '-c', 'ls data/presets/*.meowpreset 2>/dev/null || true',
  check: false,
).stdout().strip().split('\n')
if meowmenu_preset_files.length() > 0 and meowmenu_preset_files[0] != ''
  install_data(
    meowmenu_preset_files,
    install_dir: get_option('prefix') / get_option('datadir') / 'meowmenu' / 'presets',
  )
endif

install_data(
  'NEWS',
  install_dir: pkgdatadir,
)

# Generate and install localized AppStream metadata from the main catalog.
i18n.merge_file(
  input: 'data' / 'metainfo' / 'io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml',
  output: 'io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml',
  po_dir: 'po',
  type: 'xml',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'metainfo',
)

# Keep the configured version synchronized with the top NEWS entry.
test(
  'version-consistency',
  python3,
  args: [news_version_py, '--check', meson.project_version(),
         '--news', meson.current_source_dir() / 'NEWS'],
)

if get_option('enable_tests')
  subdir('tests')
endif
