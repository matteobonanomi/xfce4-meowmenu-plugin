# Releasing MeowMenu

`NEWS` is the authority for the release version, date, and public note body.
The maintainer alone commits release metadata, creates and pushes tags, changes
branch protection, and publishes AUR metadata.

## Required checks on main

Branch protection for `main` must require these six stable contexts:

```text
build (ubuntu-26.04)
build (debian-13)
build (fedora-44)
sanitizers
static-checks
no-optional-deps
```

When these names change, add the new required contexts before removing their
predecessors. Confirm that a representative pull request reports all six
before completing the transition; do not knowingly leave `main` with missing
or weaker protection.

After a CI-shape change, record the first ten consecutive completed routine
pull-request or `main` runs. Measure each from the routine-run start through
its last required context. At least nine must complete within 15 minutes.
Replace a run only if a superseding change cancelled it or a
provider-documented hosted-service incident overlapped it, recording the
exclusion and the next consecutive replacement.

## Release identity

Inspect the shared identity and native package versions with:

```bash
python3 build-aux/news-version.py --version --news NEWS
python3 build-aux/news-version.py --tag --news NEWS
python3 build-aux/news-version.py --debian-version --news NEWS
python3 build-aux/news-version.py --rpm-version --news NEWS
python3 build-aux/news-version.py --arch-version --news NEWS
```

Release tags use `v<version>` and may be annotated or lightweight. In both
cases, the tag must match the top `NEWS` version exactly and resolve to a
commit reachable from `main`. Before final 1.0.0, 0.x releases are
experimental feature releases and `-rcN` releases are the more stable testing
channel. Both are standard public GitHub releases. GitHub's `latest` marker
follows chronological release order and is independent of that stability
guidance.

## Before tagging

1. Set a real date in the top `NEWS` entry and make its complete body suitable
   for publication without added headings or shell interpolation.
2. Review package manifests, AppStream metadata, affected documentation,
   support boundaries, and security/community routes.
3. From a clean checkout, run:

   ```bash
   meson setup build
   meson compile -C build
   meson test -C build --print-errorlogs
   appstreamcli validate --no-net \
     data/metainfo/io.github.matteobonanomi.xfce4-meowmenu-plugin.metainfo.xml
   ```

4. Merge through protected `main`, update the local branch, and derive the tag:

   ```bash
   git switch main
   git pull --ff-only origin main
   release_version="$(python3 build-aux/news-version.py --version --news NEWS)"
   release_tag="$(python3 build-aux/news-version.py --tag --news NEWS)"
   test "$release_tag" = "v${release_version}"
   ```

5. Create either tag kind and push it:

   ```bash
   # Annotated:
   git tag -a "$release_tag" -m "MeowMenu ${release_version}"

   # Or lightweight:
   git tag "$release_tag"

   git push origin "$release_tag"
   ```

## Automatic publication

Pushing a valid tag starts `.github/workflows/packaging.yml`, the sole GitHub
Release owner. The workflow:

1. resolves either tag kind to its immutable commit and checks `main`
   ancestry and exact `NEWS` identity;
2. creates one canonical tagged source archive;
3. builds, runs the full product suite, installs, version-checks, and smoke
   checks native packages for Ubuntu 26.04, Debian 13, and Fedora 44;
4. builds, tests, reviews, installs, and version-checks the Arch recipe;
5. runs repository, release, documentation, catalog, and AppStream checks;
6. creates `SHA256SUMS`, verifies the exact inventory, and passes every file
   to one publication command.

After all mandatory gates pass, the release becomes public automatically. Its
body is the complete matching `NEWS` entry. No confirmation phrase, evidence
URL, second workflow run, or separate publication action is required.

For `<version>`, the public release contains exactly:

```text
xfce4-meowmenu-plugin_<version>_ubuntu26.04_amd64.deb
xfce4-meowmenu-plugin_<version>_debian13_amd64.deb
xfce4-meowmenu-plugin-<version>-1.fc44.x86_64.rpm
xfce4-meowmenu-plugin-<version>.tar.gz
SHA256SUMS
```

Download all five files into one directory and run:

```bash
sha256sum -c SHA256SUMS
```

The manifest must name the other four files exactly once and must not name
itself. Inspect and install each native package in its matching clean target.
Automated package evidence does not substitute for separately recorded live
desktop testing.

## Development packaging check

Before tagging, the complete packaging workflow can be tested without creating
or changing a GitHub Release:

1. Push the candidate commit to `development`; this starts an artifact-only
   packaging run automatically.
2. Alternatively, once this workflow revision is present on the default
   branch, open the `packaging` workflow in GitHub Actions, choose
   **Run workflow**, select `development`, keep `mode` set to `artifact-only`,
   and leave `tag` empty.
3. After the run succeeds, download the `package-set-<version>-<run-id>`
   artifact and inspect its source archive, Ubuntu and Debian packages, Fedora
   package, and `SHA256SUMS`.

Artifact-only runs accept only the selected `development` commit. They use
read-only repository permissions, skip the publication job, and cannot delete,
create, or modify a GitHub Release.

The package recipes disable AccountsService and gtk-layer-shell. Source builds
may enable either integration when its development package is available.

## Existing-tag recovery

Use manual dispatch only to recover an existing, unmodified annotated or
lightweight tag that has no public GitHub Release. Select `main` as the
workflow ref, choose `recover-release`, and enter the existing tag. Recovery
refuses any workflow ref other than `main` and verifies that the workflow
revision is reachable from `origin/main` before it can resume a draft or change
a release.

Recovery uses the corrected release tools from `main` while all source and
package content still comes from the immutable selected tag. It runs the same
mandatory gates and automatic publication path as a pushed tag.

A matching private draft for the same tag is resumed without deleting or
replacing its verified assets. A conflicting draft fails closed, and a public
release is terminal: another run fails instead of creating a duplicate or
replacing assets. Runs for one tag are serialized, and a mandatory failure
never publishes a partial release.

For hosted recovery acceptance, first dispatch a disposable existing tag from
a non-`main` ref and confirm refusal before release mutation. Then dispatch
the same tag from `main`, verify one successful publication, rerun to confirm
duplicate refusal, and exercise one mandatory failure.

## Arch and AUR

Arch validation is mandatory release evidence, but its binary package,
`PKGBUILD`, `.SRCINFO`, and review logs are not GitHub Release assets. The
repository recipe uses a development checksum; the separately published AUR
metadata must use the verified public source checksum.

After the public tag and archive exist:

```bash
./dev/aur-release.sh ../xfce4-meowmenu-plugin-AUR
```

Review the generated `PKGBUILD`, `.SRCINFO`, checksum, and source verification
in the AUR clone. Commit and publish them manually; the helper performs no Git
commit or publication.
