# Arch Linux package source

This directory holds the Arch Linux package recipe for
`xfce4-meowmenu-plugin`, kept alongside the Debian (`debian/`) and RPM
(`dist/rpm/`) sources.

- `PKGBUILD` — the package recipe (source of truth for the AUR package).

The committed recipe builds from the published `vX.Y.Z` GitHub tag archive and
uses `sha256sums=('SKIP')` so the in-tree recipe does not carry a stale,
hand-pinned checksum. The authoritative AUR metadata, including the release
checksum and `.SRCINFO`, is generated at release time by `dev/aur-release.sh` in
the sibling AUR clone.

The recipe includes `check()` so candidate validation exercises the complete
Meson suite. The release workflow also builds, lints, and smoke-installs the
recipe as a publication prerequisite.

## Reproducing the build locally

On an Arch machine (or an `archlinux:base-devel` container), `makepkg` refuses
to run as root, so build as an unprivileged user:

```bash
docker run --rm -it -v "$PWD":/src -w /src archlinux:base-devel bash
pacman -Syu --noconfirm --needed git namcap pacman-contrib sudo
useradd -m -G wheel builder
echo '%wheel ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/wheel
chown -R builder:builder /src
```

Then, from `dist/arch/`:

```bash
# 1. pkgver must equal the derived Arch version
test "$(python3 ../../build-aux/news-version.py --arch-version)" \
     = "$(bash -c 'source PKGBUILD; printf %s "$pkgver"')"

# 2. validate the source archive named by the recipe
sudo -u builder makepkg --verifysource --noconfirm

# 3. build, lint, install
sudo -u builder LC_ALL=C makepkg --syncdeps --noconfirm --cleanbuild --force
pkg="$(ls -1 *.pkg.tar.* | head -n1)"
namcap PKGBUILD "$pkg"
sudo pacman -U --noconfirm "$pkg"
pacman -Qi xfce4-meowmenu-plugin
```

To build from an untagged working tree instead of a published tag, patch a
**scratch copy** of `PKGBUILD` (never this committed file) to point at a local
`git archive` tarball with a freshly computed checksum.

## Release-time AUR metadata

`dev/aur-release.sh` copies the recipe to the sibling AUR clone, refreshes the
checksum from the live release tarball, regenerates `.SRCINFO`, and verifies the
download there. The generated AUR metadata is not committed in this repository.
It is the authoritative checksum-bearing recipe for AUR users; the
in-repository `SKIP` placeholder is only a development seed.

## Tolerated namcap warnings

`namcap` errors fail the build; warnings are advisory. Every warning observed
on a clean run is listed here with a one-line justification so future drift is
visible. The first clean run produced only "implicitly satisfied dependency"
warnings — namcap noting libraries linked transitively through the declared
`depends` (chiefly `gtk3`) and through `base`/the toolchain. Listing them as
explicit `depends` is discouraged in Arch packaging (they are pulled in by the
declared higher-level packages), so they are tolerated:

| Warning (implicitly satisfied dependency) | Why tolerated |
|-------------------------------------------|---------------|
| `glibc` | C runtime; part of `base`, never listed explicitly. |
| `libgcc` / `libstdc++` (`gcc-libs`) | Toolchain runtime; pulled by `base`/`gcc-libs`. |
| `cairo` | Transitive via `gtk3`. |
| `gdk-pixbuf2` | Transitive via `gtk3`. |
| `at-spi2-core` (`libatk-1.0`) | Transitive via `gtk3`. |
| `hicolor-icon-theme` | Icon-theme hierarchy; pulled via `gtk3`. |

If a future namcap run reports a warning not listed above, add it here with a
justification or fix the recipe — do not silently ignore it.
