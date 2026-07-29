# Releasing MeowMenu

This guide records the reusable release procedure and the additional gates for
the current release candidate. Examples use `v1.0.0` so they remain clear for a
future stable release; commands that act on a real release derive its identity
from `NEWS`. The next planned tag is `v0.9.0-rc1`, so candidate-specific
sections retain that exact value.

The maintainer alone merges, commits release metadata, creates and pushes tags,
authorizes publication, and publishes AUR metadata.

## Release identity

`NEWS` is the version and changelog authority. A stable-release example would
use public version `1.0.0` and annotated tag `v1.0.0`; do not copy those
illustrative values into release metadata. Inspect the actual shared values
with:

```bash
python3 build-aux/news-version.py --version --news NEWS
python3 build-aux/news-version.py --tag --news NEWS
python3 build-aux/news-version.py --debian-version --news NEWS
python3 build-aux/news-version.py --rpm-version --news NEWS
python3 build-aux/news-version.py --arch-version --news NEWS
```

For the current candidate, those commands produce public version
`0.9.0-rc1`, annotated tag `v0.9.0-rc1`, and these native package versions:

| Package | Version |
|---|---|
| Debian and Ubuntu | `0.9.0~rc1-1` |
| Fedora 44 | `0.9.0~rc1-1.fc44` |
| Arch | `0.9.0rc1-1` |

The transformations preserve native upgrade ordering before RC2 and 1.0.0.

## Before tagging

1. Set a real date in the top `NEWS` entry and ensure its bullets describe all
   changes since the previous published release.
2. Review the package seeds, AppStream metadata, documentation, support
   boundaries, security policy, and community routes.
3. From a clean checkout, run:

   ```bash
   meson setup build
   meson compile -C build
   meson test -C build --print-errorlogs
   appstreamcli validate --no-net \
     data/metainfo/io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml
   ```

4. Merge through the protected `main` process. Update local `main`, then create
   an annotated tag:

   ```bash
   git switch main
   git pull --ff-only origin main
   release_version="$(python3 build-aux/news-version.py --version --news NEWS)"
   release_tag="$(python3 build-aux/news-version.py --tag --news NEWS)"
   test "$release_tag" = "v${release_version}"
   git tag -a "$release_tag" -m "MeowMenu ${release_version}"
   git push origin "$release_tag"
   ```

   A lightweight tag, a tag not reachable from `main`, or a tag different from
   the top `NEWS` version is rejected. With a stable `1.0.0` top entry, these
   commands create the illustrative `v1.0.0` tag; for the current candidate
   they create `v0.9.0-rc1`.

## Private candidate workflow

The tag starts `.github/workflows/packaging.yml`, the sole GitHub Release
owner. It:

1. validates the annotated tag, `main` ancestry, date, and exact `NEWS`
   identity;
2. creates the canonical tagged source archive with one stable top-level
   directory;
3. builds, tests, installs, and version-checks Ubuntu 26.04, Debian 13, and
   Fedora 44 packages;
4. validates the Arch recipe through build, tests, namcap review, installation,
   and version ordering;
5. runs blocking AppStream validation and exposes advisory lintian/rpmlint
   output;
6. generates structured notes and `SHA256SUMS`;
7. creates or reuses one private draft prerelease, uploads the exact assets,
   downloads them again, and verifies their inventory and checksums.

Any failure leaves the release private. A rerun may replace assets only on the
matching draft; a pre-existing public release for the tag is an error.

The expected assets are:

```text
xfce4-meowmenu-plugin_0.9.0-rc1_ubuntu26.04_amd64.deb
xfce4-meowmenu-plugin_0.9.0-rc1_debian13_amd64.deb
xfce4-meowmenu-plugin-0.9.0-rc1-1.fc44.x86_64.rpm
xfce4-meowmenu-plugin-0.9.0-rc1.tar.gz
SHA256SUMS
```

Download all five from the draft and verify:

```bash
sha256sum -c SHA256SUMS
tar -tzf xfce4-meowmenu-plugin-0.9.0-rc1.tar.gz \
  | sed 's#/.*##' | sort -u
dpkg-deb -f xfce4-meowmenu-plugin_0.9.0-rc1_*_amd64.deb Version
rpm -qp --qf '%{VERSION}-%{RELEASE}\n' \
  xfce4-meowmenu-plugin-0.9.0-rc1-1.fc44.x86_64.rpm
```

The archive command must print only
`xfce4-meowmenu-plugin-0.9.0-rc1`.

## Live release gates

Using the private draft packages, complete both primary Xubuntu
26.04/Xfce 4.20/X11/amd64 procedures in [docs/testing.md](docs/testing.md):

- a fresh-profile core run, including the Modern default and post-login
  persistence;
- a 0.8.0 upgrade, including panel item, Xfconf values, favourites, layout,
  Calculator choices, custom preset files, a second migration pass, and
  post-login persistence.

Record the results in a durable public issue or prepared release evidence.
Review the support matrix, known limitations, translations, notes, five
assets, checksums, package versions, and feedback/security links as one unit.
Enable GitHub private vulnerability reporting before publication.

## Publish the prerelease

Manually run the packaging workflow for `v0.9.0-rc1` with:

- **Publish** enabled;
- **Authorization** exactly `publish v0.9.0-rc1`;
- the durable primary live-evidence URL.

The workflow repeats all gates before changing visibility. The public record is
`prerelease=true` and is never selected as the latest stable release.

After publication, download the public assets again, rerun `sha256sum -c`,
inspect package versions, and recheck public links before announcing RC1.

## Arch and AUR

The in-repository `dist/arch/PKGBUILD` uses `sha256sums=('SKIP')` for
development and CI. The separately published AUR `PKGBUILD` and `.SRCINFO`,
prepared with a verified source checksum, are authoritative for AUR users.

After the public tag/archive exists:

```bash
./dev/aur-release.sh ../xfce4-meowmenu-plugin-AUR
```

Review the generated `PKGBUILD`, `.SRCINFO`, checksum, and source verification
in the AUR clone. Commit and publish them manually; the helper performs no Git
commit or publication.
