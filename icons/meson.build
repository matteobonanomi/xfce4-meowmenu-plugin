sizes = [16, 22, 24, 32, 48, 64, 128, 256]

foreach size : sizes
  install_data(
    'hi@0@-app-meowmenu.png'.format(size),
    rename: 'org.xfce.panel.meowmenu.png',
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
  )
endforeach

install_data(
  'hisc-app-meowmenu.svg',
  rename: 'org.xfce.panel.meowmenu.svg',
  install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

# Maintainer target: regenerate every PNG from the master SVG via
# tools/regen-icons.py (rsvg-convert preferred, inkscape fallback).
# build_by_default:false because end-user builds rely on the committed
# PNGs (FR-006); only maintainers invoke this when the SVG changes:
#   meson compile -C build regen-icons
regen_icon_outputs = []
foreach size : sizes
  regen_icon_outputs += 'hi@0@-app-meowmenu.png'.format(size)
endforeach

custom_target(
  'regen-icons',
  input: '..' / 'assets' / 'meowmenu.svg',
  output: regen_icon_outputs,
  # HACK: writing into the source tree (icons/) instead of the build dir
  # so the regenerated PNGs can be committed (FR-006). Meson normally
  # treats outputs as living in the build dir; we override with an
  # absolute path. Safe because this target is opt-in (build_by_default:
  # false) and only run by the maintainer.
  command: [
    python3, files('..' / 'tools' / 'regen-icons.py'),
    '--input', '@INPUT@',
    '--output-dir', meson.current_source_dir(),
  ],
  build_by_default: false,
)
