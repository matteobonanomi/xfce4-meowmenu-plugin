#!/usr/bin/env bash
#
# Builds a coherent pinned Xfce dependency stack for source-compatibility CI.
# The caller supplies the ordinary GTK/build prerequisites from its image.

set -euo pipefail

usage() {
  echo "usage: $0 4.16|4.18|4.20|successor PREFIX" >&2
}

cell="${1:-}"
prefix="${2:-}"
if [[ -z "$prefix" ]]; then
  usage
  exit 64
fi

case "$cell" in
  4.16)
    versions=(
      "libxfce4util:4.16.0"
      "xfconf:4.16.0"
      "libxfce4ui:4.16.1"
      "exo:4.16.4"
      "garcon:4.16.1"
      "xfce4-panel:4.16.3"
    )
    ;;
  4.18)
    versions=(
      "libxfce4util:4.18.2"
      "xfconf:4.18.3"
      "libxfce4ui:4.18.6"
      "exo:4.18.0"
      "garcon:4.18.2"
      "xfce4-panel:4.18.6"
    )
    ;;
  4.20)
    versions=(
      "libxfce4util:4.20.1"
      "xfconf:4.20.0"
      "libxfce4ui:4.20.2"
      "exo:4.20.0"
      "garcon:4.20.0"
      "xfce4-panel:4.20.4"
    )
    ;;
  successor)
    versions=(
      "libxfce4util:4.20.1"
      "xfconf:4.20.0"
      "libxfce4ui:4.21.0"
      "garcon:4.20.0"
      "xfce4-panel:4.20.4"
    )
    ;;
  *)
    usage
    exit 64
    ;;
esac

source_dir="${MEOWMENU_XFCE_SOURCE_DIR:-$(mktemp -d)}"
created_source_dir=false
if [[ -z "${MEOWMENU_XFCE_SOURCE_DIR:-}" ]]; then
  created_source_dir=true
fi
trap 'if $created_source_dir; then rm -rf -- "$source_dir"; fi' EXIT
mkdir -p "$prefix" "$source_dir"

export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$prefix/lib64/pkgconfig:$prefix/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PATH="$prefix/bin:$PATH"
export LD_LIBRARY_PATH="$prefix/lib:$prefix/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

for entry in "${versions[@]}"; do
  project="${entry%%:*}"
  version="${entry#*:}"
  series="${version%.*}"
  extension=tar.bz2
  if [[ "$project" == libxfce4ui && "$version" == 4.21.0 ]]; then
    extension=tar.xz
  fi
  archive="$source_dir/$project-$version.$extension"
  tree="$source_dir/$project-$version"
  url="https://archive.xfce.org/src/xfce/$project/$series/$project-$version.$extension"
  curl --fail --location --retry 3 --output "$archive" "$url" >&2
  tar -xf "$archive" -C "$source_dir" >&2
  if [[ -f "$tree/meson.build" ]]; then
    meson setup "$tree/_build" "$tree" \
      --prefix="$prefix" \
      --buildtype=release \
      --wrap-mode=nodownload >&2
    meson compile -C "$tree/_build" >&2
    meson install -C "$tree/_build" >&2
  else
    (
      cd "$tree"
      ./configure \
        --prefix="$prefix" \
        --disable-debug \
        --disable-static
      make -j"$(nproc)"
      make install
    ) >&2
  fi
done

printf 'MEOWMENU_XFCE_CELL=%q\n' "$cell"
printf 'MEOWMENU_XFCE_PREFIX=%q\n' "$prefix"
printf 'PKG_CONFIG_PATH=%q\n' "$PKG_CONFIG_PATH"
printf 'PATH=%q\n' "$PATH"
printf 'LD_LIBRARY_PATH=%q\n' "$LD_LIBRARY_PATH"
